export module xlings.process;

import std;
import mcpplibs.os;
export import xlings.process.env;
import xlings.process.win32;

namespace hxlings::process_detail {

namespace os = mcpplibs::os;

bool SpawnDetachedPosixProcess(
    const std::filesystem::path& program,
    const std::vector<std::string>& args,
    bool sanitize_env,
    bool clear_appimage_env
) {
    os::process::spawn_options opts;
    opts.program = program;
    opts.args = args;
    opts.in = os::process::stdio::null;
    opts.out = os::process::stdio::null;
    opts.err = os::process::stdio::null;
    opts.own_unit = false;
    opts.bound_lifetime = false;
    if (sanitize_env) {
        opts.env = BuildSanitizedEnvOverrides();
    }
    if (clear_appimage_env) {
        for (const char* var_name : {"APPIMAGE", "APPDIR", "ARGV0", "OWD"}) {
            opts.env.emplace_back(var_name, std::nullopt);
        }
    }
    auto spawned = os::process::process::spawn(opts);
    if (!spawned) return false;
    spawned->detach();
    return true;
}

} // namespace hxlings::process_detail

export namespace hxlings {

ProcessResult RunProcessStreaming(
    const std::vector<std::string>& args,
    std::function<void(const std::string&)> on_line = {},
    std::shared_ptr<ActiveProcessControl> control = nullptr
) {
    namespace os = mcpplibs::os;

    ProcessResult result;
    if (args.empty()) {
        result.error_message = "empty command arguments";
        return result;
    }
    if (control && control->cancel_requested.load(std::memory_order_acquire)) {
        result.cancelled = true;
        result.error_message = "Operation cancelled";
        return result;
    }

    if constexpr (os::is_windows) {
        return process_detail::RunWindowsProcessStreamingImpl<os::is_windows>(
            args,
            std::move(on_line),
            std::move(control)
        );
    }

    os::process::spawn_options opts;
    opts.program = os::fs::from_utf8(args[0]);
    opts.args.assign(args.begin() + 1, args.end());
    opts.in = os::process::stdio::null;
    opts.out = os::process::stdio::pipe;
    opts.err = os::process::stdio::output;
    opts.own_unit = true;
    opts.bound_lifetime = true;
    const std::filesystem::path program_dir = process_detail::ExtractExecutableParentDir(args[0]);
    opts.env = process_detail::BuildSanitizedEnvOverrides(program_dir);

    auto spawned = os::process::process::spawn(opts);
    if (!spawned) {
        result.error_message = "spawn failed for '" + args[0] + "': " + spawned.error().message();
        return result;
    }

    os::process::process proc = std::move(*spawned);
    if (control) {
        control->pid.store(static_cast<std::int64_t>(proc.id()), std::memory_order_release);
    }
    process_detail::PidResetGuard pid_guard{control};

    std::string pending;
    bool term_sent = false;
    bool kill_sent = false;
    std::chrono::steady_clock::time_point cancel_time;

    auto emit_chunk = [&](std::string_view chunk) {
        result.raw_output.append(chunk);
        pending.append(chunk);
        std::size_t nl = std::string::npos;
        while ((nl = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, nl);
            pending.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (on_line) on_line(line);
        }
    };

    auto check_cancel = [&]() {
        if (!control || !control->cancel_requested.load(std::memory_order_acquire)) return;
        result.cancelled = true;
        if (!term_sent) {
            term_sent = true;
            cancel_time = std::chrono::steady_clock::now();
            proc.terminate();
        } else if (!kill_sent &&
                   std::chrono::steady_clock::now() - cancel_time > std::chrono::milliseconds(1500)) {
            kill_sent = true;
            proc.kill();
            proc.end_unit();
        }
    };

    check_cancel();

    while (true) {
        check_cancel();
        auto chunk = proc.read_output_for(std::chrono::milliseconds(100));
        if (!chunk) {
            break;
        }
        if (!chunk->has_value()) {
            auto st = proc.try_wait();
            if (st && st->has_value()) {
                auto tail = proc.read_output_for(std::chrono::milliseconds(20));
                if (tail && tail->has_value() && !(*tail)->empty()) {
                    emit_chunk(**tail);
                    continue;
                }
                break;
            }
            continue;
        }
        if ((*chunk)->empty()) {
            break;
        }
        emit_chunk(**chunk);
    }

    if (!pending.empty()) {
        if (pending.back() == '\r') pending.pop_back();
        if (!pending.empty() && on_line) on_line(pending);
    }

    while (true) {
        check_cancel();
        if (result.cancelled && !kill_sent &&
            std::chrono::steady_clock::now() - cancel_time > std::chrono::milliseconds(500)) {
            kill_sent = true;
            proc.kill();
            proc.end_unit();
        }
        auto waited = proc.wait_for(std::chrono::milliseconds(100));
        if (!waited) {
            if (result.error_message.empty()) {
                result.error_message = waited.error().message();
            }
            break;
        }
        if (waited->has_value()) {
            result.exit_code = (*waited)->shell_code();
            break;
        }
    }

    if (result.cancelled && result.error_message.empty()) {
        result.error_message = "Operation cancelled";
    }
    return result;
}

std::string NormalizeBrowserUrl(std::string_view raw_url) {
    std::size_t start = 0;
    while (start < raw_url.size() &&
           (raw_url[start] == ' ' || raw_url[start] == '\t' || raw_url[start] == '\r' || raw_url[start] == '\n')) {
        ++start;
    }
    std::size_t end = raw_url.size();
    while (end > start &&
           (raw_url[end - 1] == ' ' || raw_url[end - 1] == '\t' || raw_url[end - 1] == '\r' || raw_url[end - 1] == '\n')) {
        --end;
    }
    const std::string_view sv = raw_url.substr(start, end - start);
    if (sv.empty()) return {};

    auto ascii_lower = [](char ch) -> char {
        return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
    };
    auto starts_with_ci = [&](std::string_view prefix) -> bool {
        if (sv.size() < prefix.size()) return false;
        for (std::size_t i = 0; i < prefix.size(); ++i) {
            if (ascii_lower(sv[i]) != prefix[i]) return false;
        }
        return true;
    };

    std::string scheme;
    std::size_t rest_pos = 0;
    if (starts_with_ci("https://")) {
        scheme = "https://";
        rest_pos = 8;
    } else if (starts_with_ci("http://")) {
        scheme = "http://";
        rest_pos = 7;
    } else {
        return {};
    }

    const std::string_view rest = sv.substr(rest_pos);
    if (rest.empty()) return {};
    if (rest[0] == '/' || rest[0] == '?' || rest[0] == '#') return {};
    for (const unsigned char ch : sv) {
        if (ch <= 0x20u || ch == 0x7Fu || ch == '"' || ch == '\'' || ch == '<' || ch == '>') {
            return {};
        }
    }
    return scheme + std::string(rest);
}

bool IsSafeExternalUrl(std::string_view raw_url) {
    return !NormalizeBrowserUrl(raw_url).empty();
}

bool OpenExternalUrl(std::string_view raw_url) {
    const std::string url = NormalizeBrowserUrl(raw_url);
    if (url.empty()) return false;

    namespace os = mcpplibs::os;
    if constexpr (os::is_windows) {
        return process_detail::ShellExecuteOpenWindowsImpl<os::is_windows>(url);
    } else if constexpr (os::is_macos) {
        return process_detail::SpawnDetachedPosixProcess(os::fs::from_utf8("/usr/bin/open"), {url}, false, false);
    } else {
        return process_detail::SpawnDetachedPosixProcess(os::fs::from_utf8("xdg-open"), {url}, true, false);
    }
}

bool OpenLocalPath(const std::filesystem::path& path) {
    if (path.empty()) return false;
    namespace os = mcpplibs::os;
    const std::string path_utf8 = os::fs::utf8(path);
    if (path_utf8.empty()) return false;

    if constexpr (os::is_windows) {
        return process_detail::ShellExecuteOpenWindowsImpl<os::is_windows>(path_utf8);
    } else if constexpr (os::is_macos) {
        return process_detail::SpawnDetachedPosixProcess(os::fs::from_utf8("/usr/bin/open"), {path_utf8}, false, false);
    } else {
        return process_detail::SpawnDetachedPosixProcess(os::fs::from_utf8("xdg-open"), {path_utf8}, true, false);
    }
}

bool LaunchDetachedExecutable(
    const std::filesystem::path& exe_path,
    const std::vector<std::string>& args = {}
) {
    if (exe_path.empty()) return false;
    namespace os = mcpplibs::os;
    if constexpr (os::is_windows) {
        os::process::spawn_options opts;
        opts.program = exe_path;
        opts.args = args;
        opts.in = os::process::stdio::null;
        opts.out = os::process::stdio::null;
        opts.err = os::process::stdio::null;
        opts.own_unit = false;
        opts.bound_lifetime = false;
        auto spawned = os::process::process::spawn(opts);
        if (!spawned) return false;
        spawned->detach();
        return true;
    } else {
        return process_detail::SpawnDetachedPosixProcess(exe_path, args, os::is_linux, os::is_linux);
    }
}

} // namespace hxlings

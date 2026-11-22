export module xlings.process;

import std;
import mcpplibs.os;

namespace hxlings::detail {

namespace os = mcpplibs::os;

bool ContainsMcppRegistry(std::string_view value) {
    return value.find("/.mcpp/registry") != std::string_view::npos ||
           value.find("\\.mcpp\\registry") != std::string_view::npos;
}

bool IsExecutableFile(const std::filesystem::path& path) {
    if (path.empty()) return false;
    std::error_code ec;
    const std::filesystem::file_status st = std::filesystem::status(path, ec);
    if (ec || !std::filesystem::is_regular_file(st)) return false;
    if constexpr (os::is_posix) {
        constexpr auto exec_bits = std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::group_exec |
                                   std::filesystem::perms::others_exec;
        if ((st.permissions() & exec_bits) == std::filesystem::perms::none) {
            return false;
        }
    }
    return true;
}

bool ContainsPathEntry(const std::vector<std::filesystem::path>& dirs, const std::filesystem::path& target) {
    for (const auto& d : dirs) {
        if (os::fs::same(d, target)) return true;
    }
    return false;
}

std::vector<std::pair<std::string, std::optional<std::string>>> BuildSanitizedEnvOverrides() {
    std::vector<std::pair<std::string, std::optional<std::string>>> overrides;
    for (const char* var_name : {"XLINGS_HOME", "XLINGS_DATA", "XLINGS_SUBOS"}) {
        if (const auto val = os::env::get(var_name); val && ContainsMcppRegistry(*val)) {
            overrides.emplace_back(var_name, std::nullopt);
        }
    }

    if constexpr (os::is_macos) {
        const std::vector<std::filesystem::path> current_dirs = os::env::search_path();
        std::vector<std::filesystem::path> next_dirs;
        bool changed = false;

        auto append_if_valid_and_missing = [&](const std::filesystem::path& dir) {
            std::error_code ec;
            if (!dir.empty() && std::filesystem::is_directory(dir, ec) && !ContainsPathEntry(next_dirs, dir)) {
                if (!ContainsPathEntry(current_dirs, dir)) changed = true;
                next_dirs.push_back(dir);
            }
        };

        if (const auto home = os::dirs::home(); home.has_value() && !home->empty()) {
            append_if_valid_and_missing(*home / ".xlings" / "subos" / "current" / "bin");
            append_if_valid_and_missing(*home / ".xlings" / "bin");
        }

        for (const auto& dir : current_dirs) {
            if (dir.empty()) continue;
            if (ContainsMcppRegistry(os::fs::utf8(dir))) {
                changed = true;
                continue;
            }
            if (!ContainsPathEntry(next_dirs, dir)) {
                next_dirs.push_back(dir);
            }
        }

        for (const char* sys_bin : {"/opt/homebrew/bin", "/opt/homebrew/sbin", "/usr/local/bin"}) {
            append_if_valid_and_missing(std::filesystem::path(sys_bin));
        }

        if (changed && !next_dirs.empty()) {
            overrides.emplace_back("PATH", os::env::join_list(next_dirs));
        }
    }

    return overrides;
}

} // namespace hxlings::detail

export namespace hxlings {

struct ActiveProcessControl {
    std::atomic<std::int64_t> pid{-1};
    std::atomic<bool> cancel_requested{false};

    void Cancel() {
        cancel_requested.store(true, std::memory_order_release);
    }
};

struct ProcessResult {
    int exit_code = -1;
    std::string raw_output;
    std::string error_message;
    bool cancelled = false;
};

std::string ResolveXlingsBinary() {
    namespace os = mcpplibs::os;
    const std::string bin_name = std::string("xlings") + std::string(os::conventions().exe_suffix);

    if (const auto override_bin = os::env::get("HXLINGS_BIN"); override_bin && !override_bin->empty()) {
        const auto p = os::fs::from_utf8(*override_bin);
        if (detail::IsExecutableFile(p)) {
            return os::fs::native(p);
        }
    }

    for (const auto& dir : os::env::search_path()) {
        if (dir.empty() || detail::ContainsMcppRegistry(os::fs::utf8(dir))) continue;
        const std::filesystem::path candidate = dir / bin_name;
        if (detail::IsExecutableFile(candidate)) {
            return os::fs::native(candidate);
        }
    }

    if (const auto home = os::dirs::home(); home.has_value() && !home->empty()) {
        const std::filesystem::path current_bin = *home / ".xlings" / "subos" / "current" / "bin" / bin_name;
        if (detail::IsExecutableFile(current_bin)) {
            return os::fs::native(current_bin);
        }

        const std::filesystem::path direct_bin = *home / ".xlings" / "bin" / bin_name;
        if (detail::IsExecutableFile(direct_bin)) {
            return os::fs::native(direct_bin);
        }
    }

    if constexpr (os::is_macos) {
        for (const char* brew_bin : {"/opt/homebrew/bin/xlings", "/usr/local/bin/xlings"}) {
            const std::filesystem::path p(brew_bin);
            if (detail::IsExecutableFile(p)) {
                return os::fs::native(p);
            }
        }
    }

    return bin_name;
}

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

    os::process::spawn_options opts;
    opts.program = os::fs::from_utf8(args[0]);
    opts.args.assign(args.begin() + 1, args.end());
    opts.in = os::process::stdio::null;
    opts.out = os::process::stdio::pipe;
    opts.err = os::process::stdio::output;
    opts.own_unit = true;
    opts.bound_lifetime = true;
    opts.env = detail::BuildSanitizedEnvOverrides();

    auto spawned = os::process::process::spawn(opts);
    if (!spawned) {
        result.error_message = "spawn failed for '" + args[0] + "': " + spawned.error().message();
        return result;
    }

    os::process::process proc = std::move(*spawned);
    if (control) {
        control->pid.store(static_cast<std::int64_t>(proc.id()), std::memory_order_release);
    }

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
    if (control) {
        control->pid.store(-1, std::memory_order_release);
    }
    return result;
}

} // namespace hxlings

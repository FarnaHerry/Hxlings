export module xlings.process.env;

import std;
import mcpplibs.os;

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

} // namespace hxlings

export namespace hxlings::process_detail {

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

std::string NormalizePathKey(const std::filesystem::path& path) {
    if (path.empty()) return {};
    std::string key = os::fs::utf8(path.lexically_normal());
    if constexpr (os::is_windows) {
        for (char& ch : key) {
            if (ch == '/') {
                ch = '\\';
            } else if (ch >= 'A' && ch <= 'Z') {
                ch = static_cast<char>(ch - 'A' + 'a');
            }
        }
        while (key.size() > 3 && (key.back() == '\\' || key.back() == '/')) {
            key.pop_back();
        }
        if (key.size() > 1 && key[0] == '\\' && key.back() == '\\') {
            key.pop_back();
        }
    } else {
        while (key.size() > 1 && key.back() == '/') {
            key.pop_back();
        }
    }
    return key;
}

std::filesystem::path ExtractExecutableParentDir(std::string_view program) {
    if (program.empty()) return {};
    const std::filesystem::path exe = os::fs::from_utf8(program);
    const std::filesystem::path parent = exe.parent_path();
    if (parent.empty()) return {};
    std::error_code ec;
    if (!std::filesystem::is_directory(parent, ec)) {
        return {};
    }
    const std::filesystem::path abs_parent = std::filesystem::absolute(parent, ec);
    return ec ? parent : abs_parent;
}

template <class Fn>
void ForEachColonSegment(std::string_view list, Fn&& fn) {
    std::size_t start = 0;
    while (start <= list.size()) {
        const std::size_t sep = list.find(':', start);
        const std::size_t len = (sep == std::string_view::npos) ? (list.size() - start) : (sep - start);
        if (!fn(list.substr(start, len))) break;
        if (sep == std::string_view::npos) break;
        start = sep + 1;
    }
}

struct PidResetGuard {
    std::shared_ptr<ActiveProcessControl> control;

    explicit PidResetGuard(std::shared_ptr<ActiveProcessControl> ctrl) noexcept
        : control(std::move(ctrl)) {}

    ~PidResetGuard() {
        if (control) {
            control->pid.store(-1, std::memory_order_release);
        }
    }

    PidResetGuard(const PidResetGuard&) = delete;
    PidResetGuard& operator=(const PidResetGuard&) = delete;
};

} // namespace hxlings::process_detail

export namespace hxlings {

inline bool PathMatchesPrefixOrRegistry(std::string_view value, std::string_view bundle_dir) {
    if (value.empty()) return false;
    if (process_detail::ContainsMcppRegistry(value)) return true;
    while (!bundle_dir.empty() && (bundle_dir.back() == '/' || bundle_dir.back() == '\\')) {
        bundle_dir.remove_suffix(1);
    }
    if (bundle_dir.empty()) return false;
    if (!value.starts_with(bundle_dir)) return false;
    return value.size() == bundle_dir.size() ||
           value[bundle_dir.size()] == '/' ||
           value[bundle_dir.size()] == '\\';
}

inline std::optional<std::string> SanitizeColonPathList(std::string_view list, std::string_view bundle_dir) {
    std::string out;
    process_detail::ForEachColonSegment(list, [&](std::string_view token) {
        if (!token.empty() && !PathMatchesPrefixOrRegistry(token, bundle_dir)) {
            if (!out.empty()) out.push_back(':');
            out.append(token);
        }
        return true;
    });
    if (out.empty()) return std::nullopt;
    return out;
}

inline std::vector<std::filesystem::path> BuildSanitizedSearchPath(
    const std::vector<std::filesystem::path>& current_dirs,
    const std::vector<std::filesystem::path>& priority_dirs,
    const std::vector<std::filesystem::path>& fallback_dirs,
    std::string_view bundle_dir
) {
    namespace os = mcpplibs::os;
    std::vector<std::filesystem::path> result;
    std::vector<std::string> seen;

    auto try_append = [&](const std::filesystem::path& dir) {
        if (dir.empty()) return;
        const std::string utf8_dir = os::fs::utf8(dir);
        if (utf8_dir.empty() || PathMatchesPrefixOrRegistry(utf8_dir, bundle_dir)) return;
        const std::string key = process_detail::NormalizePathKey(dir);
        if (key.empty()) return;
        for (const auto& existing : seen) {
            if (existing == key) return;
        }
        seen.push_back(key);
        result.push_back(dir);
    };

    for (const auto& dir : priority_dirs) try_append(dir);
    for (const auto& dir : current_dirs) try_append(dir);
    for (const auto& dir : fallback_dirs) try_append(dir);
    return result;
}

inline std::string QuoteWindowsArg(std::string_view arg) {
    if (arg.empty()) {
        return "\"\"";
    }
    if (arg.find_first_of(" \t\n\v\"") == std::string_view::npos) {
        return std::string(arg);
    }
    std::string out;
    out.reserve(arg.size() + 4);
    out.push_back('"');
    std::size_t backslashes = 0;
    for (const char ch : arg) {
        if (ch == '\\') {
            ++backslashes;
            continue;
        }
        if (ch == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out.push_back('"');
            backslashes = 0;
            continue;
        }
        if (backslashes > 0) {
            out.append(backslashes, '\\');
            backslashes = 0;
        }
        out.push_back(ch);
    }
    if (backslashes > 0) {
        out.append(backslashes * 2, '\\');
    }
    out.push_back('"');
    return out;
}

inline std::string BuildWindowsCommandLine(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {};
    }
    auto has_batch_ext = [](std::string_view path) {
        if (path.size() < 4) return false;
        const std::string_view tail = path.substr(path.size() - 4);
        auto ascii_eq = [](std::string_view a, std::string_view b) {
            if (a.size() != b.size()) return false;
            for (std::size_t i = 0; i < a.size(); ++i) {
                const char ca = (a[i] >= 'A' && a[i] <= 'Z') ? static_cast<char>(a[i] - 'A' + 'a') : a[i];
                const char cb = (b[i] >= 'A' && b[i] <= 'Z') ? static_cast<char>(b[i] - 'A' + 'a') : b[i];
                if (ca != cb) return false;
            }
            return true;
        };
        return ascii_eq(tail, ".cmd") || ascii_eq(tail, ".bat");
    };

    if (has_batch_ext(args[0])) {
        std::string batch = QuoteWindowsArg(args[0]);
        for (std::size_t i = 1; i < args.size(); ++i) {
            batch.push_back(' ');
            batch += QuoteWindowsArg(args[i]);
        }
        return "cmd.exe /d /v:off /s /c \"" + batch + "\"";
    }

    std::string cmd = QuoteWindowsArg(args[0]);
    for (std::size_t i = 1; i < args.size(); ++i) {
        cmd.push_back(' ');
        cmd += QuoteWindowsArg(args[i]);
    }
    return cmd;
}

inline std::vector<std::string> MergeWindowsEnvEntries(
    std::vector<std::string> inherited,
    const std::vector<std::pair<std::string, std::optional<std::string>>>& overrides
) {
    auto ascii_lower = [](unsigned char ch) -> unsigned char {
        return (ch >= 'A' && ch <= 'Z') ? static_cast<unsigned char>(ch - 'A' + 'a') : ch;
    };
    auto key_equals_ci = [&](std::string_view entry, std::string_view key) {
        const std::size_t eq = entry.find('=');
        if (eq == std::string_view::npos || eq == 0 || eq != key.size()) {
            return false;
        }
        for (std::size_t i = 0; i < eq; ++i) {
            if (ascii_lower(static_cast<unsigned char>(entry[i])) !=
                ascii_lower(static_cast<unsigned char>(key[i]))) {
                return false;
            }
        }
        return true;
    };

    for (const auto& [key, value] : overrides) {
        std::erase_if(inherited, [&](const std::string& entry) {
            return key_equals_ci(entry, key);
        });
        if (value.has_value()) {
            inherited.push_back(key + "=" + *value);
        }
    }

    std::sort(inherited.begin(), inherited.end(), [&](const std::string& a, const std::string& b) {
        const std::size_t n = std::min(a.size(), b.size());
        for (std::size_t i = 0; i < n; ++i) {
            const unsigned char ca = ascii_lower(static_cast<unsigned char>(a[i]));
            const unsigned char cb = ascii_lower(static_cast<unsigned char>(b[i]));
            if (ca != cb) {
                return ca < cb;
            }
        }
        return a.size() < b.size();
    });

    return inherited;
}

std::string ResolveXlingsBinary() {
    namespace os = mcpplibs::os;
    const std::string bin_name = std::string("xlings") + std::string(os::conventions().exe_suffix);
    const std::string bundle_dir = os::env::get("MCPP_BUNDLE_DIR").value_or(std::string{});

    if (const auto override_bin = os::env::get("HXLINGS_BIN"); override_bin && !override_bin->empty()) {
        const auto p = os::fs::from_utf8(*override_bin);
        if (process_detail::IsExecutableFile(p)) {
            return os::fs::native(p);
        }
    }

    for (const auto& dir : os::env::search_path()) {
        if (dir.empty() || PathMatchesPrefixOrRegistry(os::fs::utf8(dir), bundle_dir)) continue;
        const std::filesystem::path candidate = dir / bin_name;
        if (process_detail::IsExecutableFile(candidate)) {
            return os::fs::native(candidate);
        }
    }

    if (const auto xlings_home = os::env::get("XLINGS_HOME");
        xlings_home.has_value() && !xlings_home->empty() && !process_detail::ContainsMcppRegistry(*xlings_home)) {
        const std::filesystem::path xh = os::fs::from_utf8(*xlings_home);
        const std::filesystem::path current_bin = xh / "subos" / "current" / "bin" / bin_name;
        if (process_detail::IsExecutableFile(current_bin)) {
            return os::fs::native(current_bin);
        }
        const std::filesystem::path direct_bin = xh / "bin" / bin_name;
        if (process_detail::IsExecutableFile(direct_bin)) {
            return os::fs::native(direct_bin);
        }
    }

    if (const auto home = os::dirs::home(); home.has_value() && !home->empty()) {
        const std::filesystem::path current_bin = *home / ".xlings" / "subos" / "current" / "bin" / bin_name;
        if (process_detail::IsExecutableFile(current_bin)) {
            return os::fs::native(current_bin);
        }

        const std::filesystem::path direct_bin = *home / ".xlings" / "bin" / bin_name;
        if (process_detail::IsExecutableFile(direct_bin)) {
            return os::fs::native(direct_bin);
        }
    }

    if constexpr (os::is_macos) {
        for (const char* brew_bin : {"/opt/homebrew/bin/xlings", "/usr/local/bin/xlings"}) {
            const std::filesystem::path p(brew_bin);
            if (process_detail::IsExecutableFile(p)) {
                return os::fs::native(p);
            }
        }
    }

    return bin_name;
}

} // namespace hxlings

export namespace hxlings::process_detail {

std::vector<std::pair<std::string, std::optional<std::string>>> BuildSanitizedEnvOverrides(
    const std::filesystem::path& program_dir = {}
) {
    std::vector<std::pair<std::string, std::optional<std::string>>> overrides;
    for (const char* var_name : {"XLINGS_HOME", "XLINGS_DATA", "XLINGS_SUBOS"}) {
        if (const auto val = os::env::get(var_name); val && ContainsMcppRegistry(*val)) {
            overrides.emplace_back(var_name, std::nullopt);
        }
    }

    const std::string bundle_dir = os::env::get("MCPP_BUNDLE_DIR").value_or(std::string{});
    if (!bundle_dir.empty()) {
        overrides.emplace_back("MCPP_BUNDLE_DIR", std::nullopt);
    }

    if constexpr (os::is_posix) {
        for (const char* path_var : {"GCONV_PATH", "LOCPATH", "LD_LIBRARY_PATH"}) {
            if (const auto val = os::env::get(path_var); val && !val->empty()) {
                bool contaminated = false;
                ForEachColonSegment(*val, [&](std::string_view token) {
                    if (hxlings::PathMatchesPrefixOrRegistry(token, bundle_dir)) {
                        contaminated = true;
                        return false;
                    }
                    return true;
                });
                if (contaminated) {
                    overrides.emplace_back(path_var, hxlings::SanitizeColonPathList(*val, bundle_dir));
                }
            }
        }
    }

    const std::vector<std::filesystem::path> current_dirs = os::env::search_path();
    std::vector<std::filesystem::path> priority_dirs;
    std::vector<std::filesystem::path> fallback_dirs;

    auto append_existing_dir = [](std::vector<std::filesystem::path>& out, const std::filesystem::path& dir) {
        if (dir.empty()) return;
        std::error_code ec;
        if (std::filesystem::is_directory(dir, ec)) {
            out.push_back(dir);
        }
    };

    append_existing_dir(priority_dirs, program_dir);
    if (const auto xlings_home = os::env::get("XLINGS_HOME");
        xlings_home.has_value() && !xlings_home->empty() && !ContainsMcppRegistry(*xlings_home)) {
        const std::filesystem::path xh = os::fs::from_utf8(*xlings_home);
        append_existing_dir(priority_dirs, xh / "subos" / "current" / "bin");
        append_existing_dir(priority_dirs, xh / "bin");
    }
    if (const auto home = os::dirs::home(); home.has_value() && !home->empty()) {
        append_existing_dir(priority_dirs, *home / ".xlings" / "subos" / "current" / "bin");
        append_existing_dir(priority_dirs, *home / ".xlings" / "bin");
    }

    if constexpr (os::is_macos) {
        for (const char* sys_bin : {"/opt/homebrew/bin", "/opt/homebrew/sbin", "/usr/local/bin", "/usr/bin", "/bin"}) {
            append_existing_dir(fallback_dirs, std::filesystem::path(sys_bin));
        }
    } else if constexpr (os::is_linux) {
        for (const char* sys_bin : {"/usr/local/bin", "/usr/bin", "/bin"}) {
            append_existing_dir(fallback_dirs, std::filesystem::path(sys_bin));
        }
    }

    const std::vector<std::filesystem::path> next_dirs =
        hxlings::BuildSanitizedSearchPath(current_dirs, priority_dirs, fallback_dirs, bundle_dir);

    bool path_changed = next_dirs.size() != current_dirs.size();
    if (!path_changed) {
        for (std::size_t i = 0; i < next_dirs.size(); ++i) {
            if (NormalizePathKey(next_dirs[i]) != NormalizePathKey(current_dirs[i])) {
                path_changed = true;
                break;
            }
        }
    }
    if (path_changed && !next_dirs.empty()) {
        overrides.emplace_back("PATH", os::env::join_list(next_dirs));
    }

    return overrides;
}

} // namespace hxlings::process_detail

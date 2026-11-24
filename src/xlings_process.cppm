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

} // namespace hxlings::detail

export namespace hxlings {

inline bool PathMatchesPrefixOrRegistry(std::string_view value, std::string_view bundle_dir) {
    if (value.empty()) return false;
    if (detail::ContainsMcppRegistry(value)) return true;
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
    detail::ForEachColonSegment(list, [&](std::string_view token) {
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
        const std::string key = detail::NormalizePathKey(dir);
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

} // namespace hxlings

namespace hxlings::detail {

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
    const std::string bundle_dir = os::env::get("MCPP_BUNDLE_DIR").value_or(std::string{});

    if (const auto override_bin = os::env::get("HXLINGS_BIN"); override_bin && !override_bin->empty()) {
        const auto p = os::fs::from_utf8(*override_bin);
        if (detail::IsExecutableFile(p)) {
            return os::fs::native(p);
        }
    }

    for (const auto& dir : os::env::search_path()) {
        if (dir.empty() || PathMatchesPrefixOrRegistry(os::fs::utf8(dir), bundle_dir)) continue;
        const std::filesystem::path candidate = dir / bin_name;
        if (detail::IsExecutableFile(candidate)) {
            return os::fs::native(candidate);
        }
    }

    if (const auto xlings_home = os::env::get("XLINGS_HOME");
        xlings_home.has_value() && !xlings_home->empty() && !detail::ContainsMcppRegistry(*xlings_home)) {
        const std::filesystem::path xh = os::fs::from_utf8(*xlings_home);
        const std::filesystem::path current_bin = xh / "subos" / "current" / "bin" / bin_name;
        if (detail::IsExecutableFile(current_bin)) {
            return os::fs::native(current_bin);
        }
        const std::filesystem::path direct_bin = xh / "bin" / bin_name;
        if (detail::IsExecutableFile(direct_bin)) {
            return os::fs::native(direct_bin);
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

} // namespace hxlings

namespace hxlings::detail {

struct Win32SecurityAttributes {
    std::uint32_t length = sizeof(Win32SecurityAttributes);
    void* security_descriptor = nullptr;
    int inherit_handle = 0;
};
static_assert(sizeof(Win32SecurityAttributes) == 24);

struct Win32StartupInfoW {
    std::uint32_t cb = sizeof(Win32StartupInfoW);
    char16_t* reserved = nullptr;
    char16_t* desktop = nullptr;
    char16_t* title = nullptr;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t x_size = 0;
    std::uint32_t y_size = 0;
    std::uint32_t x_count_chars = 0;
    std::uint32_t y_count_chars = 0;
    std::uint32_t fill_attribute = 0;
    std::uint32_t flags = 0;
    std::uint16_t show_window = 0;
    std::uint16_t cb_reserved2 = 0;
    void* reserved2 = nullptr;
    void* std_input = nullptr;
    void* std_output = nullptr;
    void* std_error = nullptr;
};
static_assert(sizeof(Win32StartupInfoW) == 104);

struct Win32ProcessInformation {
    void* process_handle = nullptr;
    void* thread_handle = nullptr;
    std::uint32_t process_id = 0;
    std::uint32_t thread_id = 0;
};
static_assert(sizeof(Win32ProcessInformation) == 24);

struct Win32JobBasicLimit {
    std::int64_t per_proc_cpu_limit = 0;
    std::int64_t per_job_cpu_limit = 0;
    std::uint32_t limit_flags = 0;
    std::size_t min_working_set = 0;
    std::size_t max_working_set = 0;
    std::uint32_t active_process_limit = 0;
    std::uintptr_t affinity = 0;
    std::uint32_t priority_class = 0;
    std::uint32_t scheduling_class = 0;
};
static_assert(sizeof(Win32JobBasicLimit) == 64);

struct Win32JobExtendedLimit {
    Win32JobBasicLimit basic{};
    std::uint64_t io_counters[6]{};
    std::size_t proc_memory_limit = 0;
    std::size_t job_memory_limit = 0;
    std::size_t peak_proc_mem = 0;
    std::size_t peak_job_mem = 0;
};
static_assert(sizeof(Win32JobExtendedLimit) == 144);

inline bool IsValidWin32Handle(void* handle) {
    return handle != nullptr && handle != reinterpret_cast<void*>(static_cast<std::intptr_t>(-1));
}

struct Win32EnvGuard {
    using FreeFn = int (*)(char16_t*);
    char16_t* ptr = nullptr;
    FreeFn free_fn = nullptr;

    Win32EnvGuard(char16_t* p, FreeFn fn) noexcept : ptr(p), free_fn(fn) {}

    ~Win32EnvGuard() {
        if (ptr != nullptr && free_fn != nullptr) {
            free_fn(ptr);
        }
    }

    Win32EnvGuard(const Win32EnvGuard&) = delete;
    Win32EnvGuard& operator=(const Win32EnvGuard&) = delete;
};

class Win32HandleGuard {
public:
    using CloseFn = int (*)(void*);

    Win32HandleGuard(void* handle, CloseFn close_fn) noexcept
        : handle_(handle), close_fn_(close_fn) {}

    Win32HandleGuard(Win32HandleGuard&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)), close_fn_(other.close_fn_) {}

    Win32HandleGuard& operator=(Win32HandleGuard&& other) noexcept {
        if (this != &other) {
            Reset();
            handle_ = std::exchange(other.handle_, nullptr);
            close_fn_ = other.close_fn_;
        }
        return *this;
    }

    Win32HandleGuard(const Win32HandleGuard&) = delete;
    Win32HandleGuard& operator=(const Win32HandleGuard&) = delete;

    ~Win32HandleGuard() {
        Reset();
    }

    void Reset(void* next = nullptr) noexcept {
        if (IsValidWin32Handle(handle_) && close_fn_ != nullptr) {
            close_fn_(handle_);
        }
        handle_ = next;
    }

    [[nodiscard]] void* Get() const noexcept {
        return handle_;
    }

    [[nodiscard]] bool Valid() const noexcept {
        return IsValidWin32Handle(handle_);
    }

private:
    void* handle_ = nullptr;
    CloseFn close_fn_ = nullptr;
};

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

template <bool Enabled>
ProcessResult RunWindowsProcessStreamingImpl(
    const std::vector<std::string>& args,
    std::function<void(const std::string&)> on_line,
    std::shared_ptr<ActiveProcessControl> control
) {
    ProcessResult result;
    if constexpr (Enabled) {
        const auto kernel32 = os::library::loaded("kernel32.dll");
        if (!kernel32.has_value()) {
            result.error_message = "cannot access kernel32.dll for Windows process launch";
            return result;
        }

        const auto mb_to_wc = kernel32->function<int(unsigned int, std::uint32_t, const char*, int, char16_t*, int)>("MultiByteToWideChar");
        const auto wc_to_mb = kernel32->function<int(unsigned int, std::uint32_t, const char16_t*, int, char*, int, const char*, int*)>("WideCharToMultiByte");
        const auto create_pipe = kernel32->function<int(void**, void**, void*, std::uint32_t)>("CreatePipe");
        const auto set_handle_info = kernel32->function<int(void*, std::uint32_t, std::uint32_t)>("SetHandleInformation");
        const auto create_file_w = kernel32->function<void*(const char16_t*, std::uint32_t, std::uint32_t, void*, std::uint32_t, std::uint32_t, void*)>("CreateFileW");
        const auto get_current_process = kernel32->function<void*()>("GetCurrentProcess");
        const auto duplicate_handle = kernel32->function<int(void*, void*, void*, void**, std::uint32_t, int, std::uint32_t)>("DuplicateHandle");
        const auto create_process_w = kernel32->function<int(const char16_t*, char16_t*, void*, void*, int, std::uint32_t, void*, const char16_t*, const Win32StartupInfoW*, Win32ProcessInformation*)>("CreateProcessW");
        const auto peek_named_pipe = kernel32->function<int(void*, void*, std::uint32_t, std::uint32_t*, std::uint32_t*, std::uint32_t*)>("PeekNamedPipe");
        const auto read_file = kernel32->function<int(void*, void*, std::uint32_t, std::uint32_t*, void*)>("ReadFile");
        const auto wait_single = kernel32->function<std::uint32_t(void*, std::uint32_t)>("WaitForSingleObject");
        const auto get_exit_code = kernel32->function<int(void*, std::uint32_t*)>("GetExitCodeProcess");
        const auto terminate_proc = kernel32->function<int(void*, std::uint32_t)>("TerminateProcess");
        const auto close_handle = kernel32->function<int(void*)>("CloseHandle");
        const auto get_last_error = kernel32->function<std::uint32_t()>("GetLastError");
        const auto get_env_w = kernel32->function<char16_t*()>("GetEnvironmentStringsW");
        const auto free_env_w = kernel32->function<int(char16_t*)>("FreeEnvironmentStringsW");
        const auto create_job_w = kernel32->function<void*(void*, const char16_t*)>("CreateJobObjectW");
        const auto set_job_info = kernel32->function<int(void*, int, void*, std::uint32_t)>("SetInformationJobObject");
        const auto assign_job = kernel32->function<int(void*, void*)>("AssignProcessToJobObject");
        const auto terminate_job = kernel32->function<int(void*, std::uint32_t)>("TerminateJobObject");
        const auto resume_thread = kernel32->function<std::uint32_t(void*)>("ResumeThread");

        if (mb_to_wc == nullptr || wc_to_mb == nullptr || create_pipe == nullptr ||
            set_handle_info == nullptr || create_file_w == nullptr || get_current_process == nullptr ||
            duplicate_handle == nullptr || create_process_w == nullptr || peek_named_pipe == nullptr ||
            read_file == nullptr || wait_single == nullptr || get_exit_code == nullptr ||
            terminate_proc == nullptr || close_handle == nullptr || get_last_error == nullptr ||
            resume_thread == nullptr) {
            result.error_message = "required kernel32 process APIs unavailable";
            return result;
        }

        constexpr unsigned int kCodePageUtf8 = 65001u;
        constexpr std::uint32_t kGenericRead = 0x80000000u;
        constexpr std::uint32_t kFileShareReadWrite = 0x00000003u;
        constexpr std::uint32_t kOpenExisting = 3u;
        constexpr std::uint32_t kFileAttrNormal = 0x00000080u;
        constexpr std::uint32_t kHandleFlagInherit = 0x00000001u;
        constexpr std::uint32_t kDuplicateSameAccess = 0x00000002u;
        constexpr std::uint32_t kStartfShowWindow = 0x00000001u;
        constexpr std::uint32_t kStartfStdHandles = 0x00000100u;
        constexpr std::uint16_t kSwHide = 0;
        constexpr std::uint32_t kCreateSuspended = 0x00000004u;
        constexpr std::uint32_t kCreateUnicodeEnv = 0x00000400u;
        constexpr std::uint32_t kCreateNoWindow = 0x08000000u;
        constexpr int kJobObjectExtLimitInfo = 9;
        constexpr std::uint32_t kJobLimitKillOnClose = 0x00002000u;
        constexpr std::uint32_t kWaitObject0 = 0x00000000u;

        auto utf8_to_u16 = [&](std::string_view utf8) -> std::u16string {
            if (utf8.empty()) return {};
            const int len = mb_to_wc(kCodePageUtf8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            if (len <= 0) return {};
            std::u16string out(static_cast<std::size_t>(len), u'\0');
            mb_to_wc(kCodePageUtf8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), len);
            return out;
        };

        auto u16_to_utf8 = [&](const char16_t* str, std::size_t len) -> std::string {
            if (str == nullptr || len == 0) return {};
            const int out_len = wc_to_mb(kCodePageUtf8, 0, str, static_cast<int>(len), nullptr, 0, nullptr, nullptr);
            if (out_len <= 0) return {};
            std::string out(static_cast<std::size_t>(out_len), '\0');
            wc_to_mb(kCodePageUtf8, 0, str, static_cast<int>(len), out.data(), out_len, nullptr, nullptr);
            return out;
        };

        std::string cmd_utf8 = hxlings::BuildWindowsCommandLine(args);
        std::u16string cmd_u16 = utf8_to_u16(cmd_utf8);
        cmd_u16.push_back(u'\0');
        if (cmd_u16.size() <= 1) {
            result.error_message = "failed to encode Windows command line as UTF-16";
            return result;
        }

        const std::filesystem::path program_dir = ExtractExecutableParentDir(args[0]);
        const auto env_overrides = BuildSanitizedEnvOverrides(program_dir);
        std::vector<char16_t> env_block;
        void* env_ptr = nullptr;
        std::uint32_t creation_flags = kCreateNoWindow | kCreateSuspended;

        if (!env_overrides.empty() && get_env_w != nullptr && free_env_w != nullptr) {
            std::vector<std::string> inherited;
            if (Win32EnvGuard raw_env{get_env_w(), free_env_w}; raw_env.ptr != nullptr) {
                for (const char16_t* cur = raw_env.ptr; *cur != u'\0'; ) {
                    std::size_t entry_len = 0;
                    while (cur[entry_len] != u'\0') ++entry_len;
                    inherited.push_back(u16_to_utf8(cur, entry_len));
                    cur += entry_len + 1;
                }
            }
            const auto merged = hxlings::MergeWindowsEnvEntries(std::move(inherited), env_overrides);
            for (const std::string& entry : merged) {
                const std::u16string w = utf8_to_u16(entry);
                env_block.insert(env_block.end(), w.begin(), w.end());
                env_block.push_back(u'\0');
            }
            if (env_block.empty()) {
                env_block.push_back(u'\0');
            }
            env_block.push_back(u'\0');
            env_ptr = env_block.data();
            creation_flags |= kCreateUnicodeEnv;
        }

        Win32HandleGuard pipe_read{nullptr, close_handle};
        Win32ProcessInformation pi{};
        std::uint32_t spawn_err = 0;
        int created = 0;

        {
            static std::mutex spawn_mutex;
            std::lock_guard<std::mutex> lock(spawn_mutex);

            Win32SecurityAttributes sa_inherit{};
            sa_inherit.length = sizeof(Win32SecurityAttributes);
            sa_inherit.inherit_handle = 1;

            void* raw_read = nullptr;
            void* raw_write = nullptr;
            if (create_pipe(&raw_read, &raw_write, &sa_inherit, 0) == 0) {
                result.error_message = "CreatePipe failed for '" + args[0] + "' (win32=" +
                                       std::to_string(get_last_error()) + ")";
                return result;
            }
            pipe_read.Reset(raw_read);
            Win32HandleGuard pipe_write{raw_write, close_handle};
            set_handle_info(pipe_read.Get(), kHandleFlagInherit, 0);

            void* self_proc = get_current_process();
            void* raw_err = nullptr;
            if (duplicate_handle(self_proc, pipe_write.Get(), self_proc, &raw_err, 0, 1, kDuplicateSameAccess) == 0) {
                raw_err = nullptr;
            }
            Win32HandleGuard err_write{raw_err, close_handle};

            Win32HandleGuard nul_in{
                create_file_w(u"NUL", kGenericRead, kFileShareReadWrite, &sa_inherit, kOpenExisting, kFileAttrNormal, nullptr),
                close_handle
            };

            Win32StartupInfoW startup{};
            startup.cb = sizeof(Win32StartupInfoW);
            startup.flags = kStartfStdHandles | kStartfShowWindow;
            startup.show_window = kSwHide;
            startup.std_input = nul_in.Valid() ? nul_in.Get() : nullptr;
            startup.std_output = pipe_write.Get();
            startup.std_error = err_write.Valid() ? err_write.Get() : pipe_write.Get();

            created = create_process_w(
                nullptr,
                cmd_u16.data(),
                nullptr,
                nullptr,
                1,
                creation_flags,
                env_ptr,
                nullptr,
                &startup,
                &pi
            );
            if (created == 0) {
                spawn_err = get_last_error();
            }
        }

        if (created == 0) {
            result.error_message = "spawn failed for '" + args[0] + "' (win32=" + std::to_string(spawn_err) + ")";
            return result;
        }

        Win32HandleGuard proc_handle{pi.process_handle, close_handle};
        Win32HandleGuard thread_handle{pi.thread_handle, close_handle};
        Win32HandleGuard job{nullptr, close_handle};

        if (create_job_w != nullptr && set_job_info != nullptr && assign_job != nullptr) {
            job.Reset(create_job_w(nullptr, nullptr));
            if (job.Valid()) {
                Win32JobExtendedLimit ext{};
                ext.basic.limit_flags = kJobLimitKillOnClose;
                set_job_info(job.Get(), kJobObjectExtLimitInfo, &ext, sizeof(ext));
                if (assign_job(job.Get(), proc_handle.Get()) == 0) {
                    job.Reset();
                }
            } else {
                job.Reset();
            }
        }

        resume_thread(thread_handle.Get());
        if (control) {
            control->pid.store(static_cast<std::int64_t>(pi.process_id), std::memory_order_release);
        }
        PidResetGuard pid_guard{control};

        std::string pending;
        bool terminated = false;

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
            if (!terminated) {
                terminated = true;
                if (job.Valid() && terminate_job != nullptr) {
                    terminate_job(job.Get(), 1u);
                }
                terminate_proc(proc_handle.Get(), 1u);
            }
        };

        check_cancel();

        char buf[16384];
        while (true) {
            check_cancel();
            std::uint32_t avail = 0;
            if (peek_named_pipe(pipe_read.Get(), nullptr, 0, nullptr, &avail, nullptr) == 0) {
                break;
            }
            if (avail > 0) {
                const std::uint32_t to_read = avail < sizeof(buf) ? avail : static_cast<std::uint32_t>(sizeof(buf));
                std::uint32_t bytes_read = 0;
                if (read_file(pipe_read.Get(), buf, to_read, &bytes_read, nullptr) == 0 || bytes_read == 0) {
                    break;
                }
                emit_chunk(std::string_view(buf, bytes_read));
                continue;
            }
            if (wait_single(proc_handle.Get(), 20u) == kWaitObject0) {
                while (peek_named_pipe(pipe_read.Get(), nullptr, 0, nullptr, &avail, nullptr) != 0 && avail > 0) {
                    const std::uint32_t to_read = avail < sizeof(buf) ? avail : static_cast<std::uint32_t>(sizeof(buf));
                    std::uint32_t bytes_read = 0;
                    if (read_file(pipe_read.Get(), buf, to_read, &bytes_read, nullptr) == 0 || bytes_read == 0) {
                        break;
                    }
                    emit_chunk(std::string_view(buf, bytes_read));
                }
                break;
            }
        }

        if (!pending.empty()) {
            if (pending.back() == '\r') pending.pop_back();
            if (!pending.empty() && on_line) on_line(pending);
        }

        while (wait_single(proc_handle.Get(), 50u) != kWaitObject0) {
            check_cancel();
        }

        std::uint32_t exit_code = 1;
        if (get_exit_code(proc_handle.Get(), &exit_code) != 0) {
            result.exit_code = static_cast<int>(exit_code);
        }

        if (result.cancelled && result.error_message.empty()) {
            result.error_message = "Operation cancelled";
        }
    }
    return result;
}

template <bool Enabled>
bool ShellExecuteOpenWindowsImpl(const std::string& utf8_target) {
    if constexpr (Enabled) {
        const auto kernel32 = os::library::loaded("kernel32.dll");
        if (!kernel32.has_value()) return false;
        const auto mb_to_wc = kernel32->function<int(unsigned int, std::uint32_t, const char*, int, char16_t*, int)>(
            "MultiByteToWideChar");
        if (mb_to_wc == nullptr) return false;

        auto to_wide = [&](std::string_view utf8) -> std::u16string {
            if (utf8.empty()) return u"";
            const int needed = mb_to_wc(65001u, 0u, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            if (needed <= 0) return {};
            std::u16string out(static_cast<std::size_t>(needed) + 1u, u'\0');
            if (mb_to_wc(65001u, 0u, utf8.data(), static_cast<int>(utf8.size()), out.data(), needed) <= 0) {
                return {};
            }
            out[static_cast<std::size_t>(needed)] = u'\0';
            return out;
        };

        using ShellExecuteWFn = void*(void*, const char16_t*, const char16_t*, const char16_t*, const char16_t*, int);
        ShellExecuteWFn* shell_execute_w = nullptr;
        std::optional<os::library::handle> loaded_shell32 = os::library::loaded("shell32.dll");
        os::library::handle opened_shell32;
        if (loaded_shell32.has_value()) {
            shell_execute_w = loaded_shell32->function<ShellExecuteWFn>("ShellExecuteW");
        } else {
            auto opened = os::library::open(os::fs::from_utf8("shell32.dll"));
            if (!opened) return false;
            opened_shell32 = std::move(*opened);
            shell_execute_w = opened_shell32.function<ShellExecuteWFn>("ShellExecuteW");
        }
        if (shell_execute_w == nullptr) return false;

        const std::u16string wop = u"open";
        const std::u16string wtarget = to_wide(utf8_target);
        if (wtarget.empty()) return false;

        constexpr int kSwShowNormal = 1;
        void* ret = shell_execute_w(nullptr, wop.c_str(), wtarget.c_str(), nullptr, nullptr, kSwShowNormal);
        return reinterpret_cast<std::intptr_t>(ret) > 32;
    } else {
        (void)utf8_target;
        return false;
    }
}

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

} // namespace hxlings::detail

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
        return detail::RunWindowsProcessStreamingImpl<os::is_windows>(
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
    const std::filesystem::path program_dir = detail::ExtractExecutableParentDir(args[0]);
    opts.env = detail::BuildSanitizedEnvOverrides(program_dir);

    auto spawned = os::process::process::spawn(opts);
    if (!spawned) {
        result.error_message = "spawn failed for '" + args[0] + "': " + spawned.error().message();
        return result;
    }

    os::process::process proc = std::move(*spawned);
    if (control) {
        control->pid.store(static_cast<std::int64_t>(proc.id()), std::memory_order_release);
    }
    detail::PidResetGuard pid_guard{control};

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
        return detail::ShellExecuteOpenWindowsImpl<os::is_windows>(url);
    } else if constexpr (os::is_macos) {
        return detail::SpawnDetachedPosixProcess(os::fs::from_utf8("/usr/bin/open"), {url}, false, false);
    } else {
        return detail::SpawnDetachedPosixProcess(os::fs::from_utf8("xdg-open"), {url}, true, false);
    }
}

bool OpenLocalPath(const std::filesystem::path& path) {
    if (path.empty()) return false;
    namespace os = mcpplibs::os;
    const std::string path_utf8 = os::fs::utf8(path);
    if (path_utf8.empty()) return false;

    if constexpr (os::is_windows) {
        return detail::ShellExecuteOpenWindowsImpl<os::is_windows>(path_utf8);
    } else if constexpr (os::is_macos) {
        return detail::SpawnDetachedPosixProcess(os::fs::from_utf8("/usr/bin/open"), {path_utf8}, false, false);
    } else {
        return detail::SpawnDetachedPosixProcess(os::fs::from_utf8("xdg-open"), {path_utf8}, true, false);
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
        return detail::SpawnDetachedPosixProcess(exe_path, args, os::is_linux, os::is_linux);
    }
}

} // namespace hxlings

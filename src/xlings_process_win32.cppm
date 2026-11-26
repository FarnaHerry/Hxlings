export module xlings.process.win32;

import std;
import mcpplibs.os;
import xlings.process.env;

namespace hxlings::process_detail {

namespace os = mcpplibs::os;

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

} // namespace hxlings::process_detail

export namespace hxlings::process_detail {

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

} // namespace hxlings::process_detail

module;

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

export module xlings.process;

import std;

namespace hxlings::detail {

bool ContainsMcppRegistry(std::string_view value) {
    return value.find("/.mcpp/registry") != std::string_view::npos ||
           value.find("\\.mcpp\\registry") != std::string_view::npos;
}

#if defined(_WIN32)

bool IsExecutableFile(const std::string& path) {
    if (path.empty()) return false;
    const DWORD attrs = ::GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string GetEnvVar(const char* name) {
    const DWORD needed = ::GetEnvironmentVariableA(name, nullptr, 0);
    if (needed == 0) return {};
    std::string value(needed, '\0');
    const DWORD written = ::GetEnvironmentVariableA(name, value.data(), needed);
    if (written == 0) return {};
    value.resize(written);
    return value;
}

bool EqualsIgnoreCaseAscii(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto ca = static_cast<unsigned char>(a[i]);
        const auto cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) return false;
    }
    return true;
}

std::string QuoteWinArg(std::string_view arg) {
    if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string_view::npos) {
        return std::string(arg);
    }
    std::string out;
    out.push_back('"');
    for (std::size_t i = 0; i < arg.size(); ++i) {
        std::size_t bs = 0;
        while (i < arg.size() && arg[i] == '\\') {
            ++bs;
            ++i;
        }
        if (i == arg.size()) {
            out.append(bs * 2, '\\');
            break;
        }
        if (arg[i] == '"') {
            out.append(bs * 2 + 1, '\\');
            out.push_back('"');
        } else {
            out.append(bs, '\\');
            out.push_back(arg[i]);
        }
    }
    out.push_back('"');
    return out;
}

std::string BuildWinCommandLine(const std::vector<std::string>& args) {
    std::string cmdline;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmdline.push_back(' ');
        cmdline += QuoteWinArg(args[i]);
    }
    return cmdline;
}

std::vector<char> BuildSanitizedWinEnvBlock() {
    std::vector<char> block;
    LPCH raw_env = ::GetEnvironmentStringsA();
    if (raw_env == nullptr) return block;

    for (const char* p = raw_env; *p != '\0';) {
        const std::string_view entry(p);
        p += entry.size() + 1;

        bool skip = false;
        const std::size_t eq = entry.find('=', 1);
        if (eq != std::string_view::npos) {
            const std::string_view key = entry.substr(0, eq);
            const std::string_view val = entry.substr(eq + 1);
            if ((EqualsIgnoreCaseAscii(key, "XLINGS_HOME") ||
                 EqualsIgnoreCaseAscii(key, "XLINGS_DATA") ||
                 EqualsIgnoreCaseAscii(key, "XLINGS_SUBOS")) &&
                ContainsMcppRegistry(val)) {
                skip = true;
            }
        }
        if (!skip) {
            block.insert(block.end(), entry.begin(), entry.end());
            block.push_back('\0');
        }
    }

    ::FreeEnvironmentStringsA(raw_env);
    if (block.empty()) block.push_back('\0');
    block.push_back('\0');
    return block;
}

std::string FormatWinError(DWORD err) {
    LPSTR msg_buf = nullptr;
    const DWORD len = ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&msg_buf),
        0,
        nullptr
    );
    std::string out = (len > 0 && msg_buf != nullptr) ? std::string(msg_buf, len) : ("error " + std::to_string(err));
    if (msg_buf != nullptr) ::LocalFree(msg_buf);
    while (!out.empty() && (out.back() == '\r' || out.back() == '\n')) out.pop_back();
    return out;
}

#else

bool IsExecutableFile(const std::string& path) {
    if (path.empty()) return false;
    return ::access(path.c_str(), X_OK) == 0;
}

void UnsetMcppSandboxVar(const char* name) {
    const char* val = ::getenv(name);
    if (val != nullptr && ContainsMcppRegistry(val)) {
        ::unsetenv(name);
    }
}

#endif

} // namespace hxlings::detail

export namespace hxlings {

struct ActiveProcessControl {
    std::atomic<std::int64_t> pid{-1};
    std::atomic<bool> cancel_requested{false};

    void Cancel() {
        cancel_requested.store(true, std::memory_order_release);
        const std::int64_t target_pid = pid.load(std::memory_order_acquire);
        if (target_pid <= 0) return;
#if defined(_WIN32)
        HANDLE h = ::OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(target_pid));
        if (h != nullptr) {
            ::TerminateProcess(h, 1);
            ::CloseHandle(h);
        }
#else
        const pid_t p = static_cast<pid_t>(target_pid);
        if (::kill(-p, SIGTERM) != 0) {
            ::kill(p, SIGTERM);
        }
#endif
    }
};

struct ProcessResult {
    int exit_code = -1;
    std::string raw_output;
    std::string error_message;
    bool cancelled = false;
};

std::string ResolveXlingsBinary() {
#if defined(_WIN32)
    if (const std::string override_bin = detail::GetEnvVar("HXLINGS_BIN"); !override_bin.empty()) {
        if (detail::IsExecutableFile(override_bin)) return override_bin;
    }

    if (const std::string path_env = detail::GetEnvVar("PATH"); !path_env.empty()) {
        std::string_view remaining(path_env);
        while (!remaining.empty()) {
            const std::size_t sep = remaining.find(';');
            const std::string_view dir = (sep == std::string_view::npos) ? remaining : remaining.substr(0, sep);
            if (!dir.empty() && !detail::ContainsMcppRegistry(dir)) {
                std::string candidate(dir);
                if (candidate.back() != '\\' && candidate.back() != '/') candidate.push_back('\\');
                candidate += "xlings.exe";
                if (detail::IsExecutableFile(candidate)) return candidate;
            }
            if (sep == std::string_view::npos) break;
            remaining.remove_prefix(sep + 1);
        }
    }

    if (const std::string profile = detail::GetEnvVar("USERPROFILE"); !profile.empty()) {
        const std::string current_bin = profile + "\\.xlings\\subos\\current\\bin\\xlings.exe";
        if (detail::IsExecutableFile(current_bin)) return current_bin;

        const std::string direct_bin = profile + "\\.xlings\\bin\\xlings.exe";
        if (detail::IsExecutableFile(direct_bin)) return direct_bin;
    }

    return "xlings.exe";
#else
    if (const char* override_bin = ::getenv("HXLINGS_BIN"); override_bin != nullptr && *override_bin != '\0') {
        if (detail::IsExecutableFile(override_bin)) {
            return std::string(override_bin);
        }
    }

    if (const char* path_env = ::getenv("PATH"); path_env != nullptr) {
        std::string_view remaining(path_env);
        while (!remaining.empty()) {
            const std::size_t colon = remaining.find(':');
            const std::string_view dir =
                (colon == std::string_view::npos) ? remaining : remaining.substr(0, colon);
            if (!dir.empty() && !detail::ContainsMcppRegistry(dir)) {
                std::string candidate = std::string(dir);
                if (candidate.back() != '/') candidate.push_back('/');
                candidate += "xlings";
                if (detail::IsExecutableFile(candidate)) {
                    return candidate;
                }
            }
            if (colon == std::string_view::npos) break;
            remaining.remove_prefix(colon + 1);
        }
    }

    if (const char* home = ::getenv("HOME"); home != nullptr && *home != '\0') {
        const std::string home_str(home);
        const std::string current_bin = home_str + "/.xlings/subos/current/bin/xlings";
        if (detail::IsExecutableFile(current_bin)) return current_bin;

        const std::string direct_bin = home_str + "/.xlings/bin/xlings";
        if (detail::IsExecutableFile(direct_bin)) return direct_bin;
    }

    return "xlings";
#endif
}

ProcessResult RunProcessStreaming(
    const std::vector<std::string>& args,
    std::function<void(const std::string&)> on_line = {},
    std::shared_ptr<ActiveProcessControl> control = nullptr
) {
    ProcessResult result;
    if (args.empty()) {
        result.error_message = "empty command arguments";
        return result;
    }

#if defined(_WIN32)
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;
    if (!::CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        result.error_message = "CreatePipe failed: " + detail::FormatWinError(::GetLastError());
        return result;
    }
    ::SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    HANDLE nul_in = ::CreateFileA(
        "NUL",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &sa,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = (nul_in != INVALID_HANDLE_VALUE) ? nul_in : nullptr;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;

    std::string cmdline = detail::BuildWinCommandLine(args);
    std::vector<char> env_block = detail::BuildSanitizedWinEnvBlock();

    PROCESS_INFORMATION pi{};
    const BOOL ok = ::CreateProcessA(
        nullptr,
        cmdline.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        env_block.empty() ? nullptr : env_block.data(),
        nullptr,
        &si,
        &pi
    );

    ::CloseHandle(write_pipe);
    if (nul_in != INVALID_HANDLE_VALUE) ::CloseHandle(nul_in);

    if (!ok) {
        result.error_message = "CreateProcess failed for '" + args[0] + "': " + detail::FormatWinError(::GetLastError());
        ::CloseHandle(read_pipe);
        return result;
    }

    if (control) {
        control->pid.store(static_cast<std::int64_t>(pi.dwProcessId), std::memory_order_release);
        if (control->cancel_requested.load(std::memory_order_acquire)) {
            result.cancelled = true;
            ::TerminateProcess(pi.hProcess, 1);
        }
    }

    std::string pending;
    std::array<char, 4096> buffer{};
    bool terminated = false;

    while (true) {
        if (control && control->cancel_requested.load(std::memory_order_acquire)) {
            result.cancelled = true;
            if (!terminated) {
                terminated = true;
                ::TerminateProcess(pi.hProcess, 1);
            }
        }

        DWORD avail = 0;
        if (!::PeekNamedPipe(read_pipe, nullptr, 0, nullptr, &avail, nullptr)) {
            break;
        }
        if (avail > 0) {
            DWORD bytes_read = 0;
            if (!::ReadFile(read_pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, nullptr) ||
                bytes_read == 0) {
                break;
            }
            const std::string_view chunk(buffer.data(), static_cast<std::size_t>(bytes_read));
            result.raw_output.append(chunk);
            pending.append(chunk);

            std::size_t nl = std::string::npos;
            while ((nl = pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, nl);
                pending.erase(0, nl + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (on_line) on_line(line);
            }
            continue;
        }

        if (::WaitForSingleObject(pi.hProcess, 0) != WAIT_TIMEOUT) {
            break;
        }
        ::Sleep(25);
    }

    ::CloseHandle(read_pipe);

    if (!pending.empty()) {
        if (pending.back() == '\r') pending.pop_back();
        if (!pending.empty() && on_line) on_line(pending);
    }

    ::WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD win_exit = 1;
    if (::GetExitCodeProcess(pi.hProcess, &win_exit)) {
        result.exit_code = static_cast<int>(win_exit);
    }
    if (result.cancelled && result.error_message.empty()) {
        result.error_message = "Operation cancelled";
    }

    if (control) {
        control->pid.store(-1, std::memory_order_release);
    }

    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    return result;
#else
    int pipe_fds[2] = {-1, -1};
    if (::pipe(pipe_fds) != 0) {
        result.error_message = std::string("pipe failed: ") + std::strerror(errno);
        return result;
    }

    ::fcntl(pipe_fds[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(pipe_fds[1], F_SETFD, FD_CLOEXEC);

    const pid_t child_pid = ::fork();
    if (child_pid < 0) {
        result.error_message = std::string("fork failed: ") + std::strerror(errno);
        ::close(pipe_fds[0]);
        ::close(pipe_fds[1]);
        return result;
    }

    if (child_pid == 0) {
        ::setpgid(0, 0);
        ::close(pipe_fds[0]);

        const int devnull = ::open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            ::dup2(devnull, STDIN_FILENO);
            if (devnull > STDERR_FILENO) ::close(devnull);
        }

        ::dup2(pipe_fds[1], STDOUT_FILENO);
        ::dup2(pipe_fds[1], STDERR_FILENO);
        if (pipe_fds[1] > STDERR_FILENO) ::close(pipe_fds[1]);

        detail::UnsetMcppSandboxVar("XLINGS_HOME");
        detail::UnsetMcppSandboxVar("XLINGS_DATA");
        detail::UnsetMcppSandboxVar("XLINGS_SUBOS");

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const std::string& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        ::execvp(argv[0], argv.data());
        const int err = errno;
        const std::string msg = std::string("execvp failed for '") + args[0] + "': " + std::strerror(err) + "\n";
        const auto ignored = ::write(STDERR_FILENO, msg.data(), msg.size());
        (void)ignored;
        ::_exit(127);
    }

    ::close(pipe_fds[1]);

    if (control) {
        control->pid.store(static_cast<std::int64_t>(child_pid), std::memory_order_release);
        if (control->cancel_requested.load(std::memory_order_acquire)) {
            if (::kill(-child_pid, SIGTERM) != 0) ::kill(child_pid, SIGTERM);
        }
    }

    std::string pending;
    std::array<char, 4096> buffer{};
    bool term_sent = false;
    std::chrono::steady_clock::time_point cancel_time;

    while (true) {
        if (control && control->cancel_requested.load(std::memory_order_acquire)) {
            result.cancelled = true;
            if (!term_sent) {
                term_sent = true;
                cancel_time = std::chrono::steady_clock::now();
                if (::kill(-child_pid, SIGTERM) != 0) ::kill(child_pid, SIGTERM);
            } else if (std::chrono::steady_clock::now() - cancel_time > std::chrono::milliseconds(1500)) {
                if (::kill(-child_pid, SIGKILL) != 0) ::kill(child_pid, SIGKILL);
            }
        }

        struct pollfd pfd{};
        pfd.fd = pipe_fds[0];
        pfd.events = POLLIN | POLLHUP;
        const int poll_rc = ::poll(&pfd, 1, 100);
        if (poll_rc < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (poll_rc == 0) continue;

        if ((pfd.revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
            const ssize_t n = ::read(pipe_fds[0], buffer.data(), buffer.size());
            if (n > 0) {
                const std::string_view chunk(buffer.data(), static_cast<std::size_t>(n));
                result.raw_output.append(chunk);
                pending.append(chunk);

                std::size_t nl = std::string::npos;
                while ((nl = pending.find('\n')) != std::string::npos) {
                    std::string line = pending.substr(0, nl);
                    pending.erase(0, nl + 1);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (on_line) on_line(line);
                }
            } else if (n == 0) {
                break;
            } else {
                if (errno == EINTR || errno == EAGAIN) continue;
                break;
            }
        }
    }

    ::close(pipe_fds[0]);

    if (!pending.empty()) {
        if (pending.back() == '\r') pending.pop_back();
        if (!pending.empty() && on_line) on_line(pending);
    }

    int status = 0;
    while (::waitpid(child_pid, &status, 0) < 0) {
        if (errno != EINTR) {
            result.error_message = std::string("waitpid failed: ") + std::strerror(errno);
            break;
        }
    }

    if (control) {
        control->pid.store(-1, std::memory_order_release);
    }

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
        if (result.cancelled && result.error_message.empty()) {
            result.error_message = "Operation cancelled";
        }
    }

    return result;
#endif
}

} // namespace hxlings

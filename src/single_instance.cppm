export module hxlings.single_instance;

import std;
import mcpplibs.os;

namespace hxlings::detail {

namespace os = mcpplibs::os;

inline void AllowPrimaryForegroundOnWindows() {
    if constexpr (os::is_windows) {
        using AllowSetForegroundWindowFn = int (*)(std::uint32_t);
        constexpr std::uint32_t kAsfwAny = 0xFFFFFFFFu;
        if (const auto loaded_lib = os::library::loaded("user32.dll"); loaded_lib.has_value()) {
            if (const auto allow_fn = loaded_lib->function<int(std::uint32_t)>("AllowSetForegroundWindow");
                allow_fn != nullptr) {
                (void)allow_fn(kAsfwAny);
                return;
            }
        }
        if (auto opened = os::library::open("user32.dll"); opened.has_value()) {
            if (const auto allow_fn = opened->function<int(std::uint32_t)>("AllowSetForegroundWindow");
                allow_fn != nullptr) {
                (void)allow_fn(kAsfwAny);
            }
        }
        (void)sizeof(AllowSetForegroundWindowFn);
    }
}

} // namespace hxlings::detail

export namespace hxlings {

inline constexpr std::string_view kDefaultSingleInstanceName = "hxlings.sock";
inline constexpr std::string_view kSingleInstanceActivateCmd = "activate\n";
inline constexpr std::string_view kSingleInstanceAck = "ok\n";

struct SingleInstanceEndpoint {
    std::filesystem::path directory;
    std::string name;
};

enum class SingleInstanceStatus {
    Primary,
    SecondaryNotified,
    Unavailable,
};

inline std::string SanitizeSingleInstanceToken(std::string_view raw, std::size_t max_len = 20) {
    std::string out;
    out.reserve(std::min(raw.size(), max_len));
    for (const char ch : raw) {
        if (out.size() >= max_len) break;
        const bool is_alnum = (ch >= 'a' && ch <= 'z') ||
                              (ch >= 'A' && ch <= 'Z') ||
                              (ch >= '0' && ch <= '9');
        if (is_alnum || ch == '-' || ch == '_') {
            out.push_back(ch);
        }
    }
    return out;
}

inline SingleInstanceEndpoint ResolveDefaultSingleInstanceEndpoint() {
    namespace os = mcpplibs::os;
    std::vector<std::filesystem::path> candidates;
    if (const auto rt = os::dirs::runtime(); rt.has_value() && !rt->empty()) {
        candidates.push_back(*rt);
    }
    if (const std::filesystem::path tmp = os::dirs::temp(); !tmp.empty()) {
        if (candidates.empty() || !os::fs::same(candidates.front(), tmp)) {
            candidates.push_back(tmp);
        }
    }

    std::filesystem::path chosen_dir;
    for (const auto& dir : candidates) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec && !std::filesystem::is_directory(dir, ec)) {
            continue;
        }
        const auto caps = os::fs::caps(dir);
        if (caps && caps->local_endpoint) {
            chosen_dir = dir;
            break;
        }
    }
    if (chosen_dir.empty() && !candidates.empty()) {
        chosen_dir = candidates.front();
    }

    std::string sock_name(kDefaultSingleInstanceName);
    if constexpr (os::is_posix) {
        if (!chosen_dir.empty() && os::fs::same(chosen_dir, std::filesystem::path("/tmp"))) {
            std::optional<std::string> account = os::env::get("USER");
            if (!account || account->empty()) {
                account = os::env::get("LOGNAME");
            }
            if (account && !account->empty()) {
                const std::string token = SanitizeSingleInstanceToken(*account, 20);
                if (!token.empty()) {
                    sock_name = "hxlings-" + token + ".sock";
                }
            }
        }
    }

    return SingleInstanceEndpoint{
        .directory = std::move(chosen_dir),
        .name = std::move(sock_name),
    };
}

class SingleInstanceGuard {
public:
    SingleInstanceGuard() = default;
    SingleInstanceGuard(const SingleInstanceGuard&) = delete;
    SingleInstanceGuard& operator=(const SingleInstanceGuard&) = delete;
    SingleInstanceGuard(SingleInstanceGuard&&) = delete;
    SingleInstanceGuard& operator=(SingleInstanceGuard&&) = delete;

    ~SingleInstanceGuard() {
        Release();
    }

    SingleInstanceStatus Acquire(
        SingleInstanceEndpoint endpoint = ResolveDefaultSingleInstanceEndpoint(),
        std::function<void()> on_activate = {}
    ) {
        namespace os = mcpplibs::os;
        Release();

        if (endpoint.directory.empty() || endpoint.name.empty()) {
            std::lock_guard lock(mutex_);
            status_ = SingleInstanceStatus::Unavailable;
            endpoint_ = std::move(endpoint);
            on_activate_ = std::move(on_activate);
            pending_activation_ = false;
            return status_;
        }

        std::error_code ec;
        std::filesystem::create_directories(endpoint.directory, ec);

        for (int attempt = 0; attempt < 3; ++attempt) {
            auto res = os::ipc::single_instance(endpoint.name, endpoint.directory);
            if (!res) {
                if (res.error().code == os::errc::again && attempt + 1 < 3) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    continue;
                }
                std::lock_guard lock(mutex_);
                status_ = SingleInstanceStatus::Unavailable;
                endpoint_ = std::move(endpoint);
                on_activate_ = std::move(on_activate);
                pending_activation_ = false;
                return status_;
            }

            if (auto* peer = std::get_if<os::ipc::connection>(&*res)) {
                detail::AllowPrimaryForegroundOnWindows();
                const auto wrote = peer->write(kSingleInstanceActivateCmd);
                peer->shutdown_write();
                if (wrote) {
                    (void)peer->read_for(std::chrono::milliseconds(1500), 64);
                }
                peer->close();

                std::lock_guard lock(mutex_);
                status_ = SingleInstanceStatus::SecondaryNotified;
                endpoint_ = std::move(endpoint);
                on_activate_ = {};
                pending_activation_ = false;
                return status_;
            }

            if (auto* srv = std::get_if<os::ipc::listener>(&*res)) {
                std::lock_guard lock(mutex_);
                stop_requested_.store(false, std::memory_order_release);
                pending_activation_ = false;
                on_activate_ = std::move(on_activate);
                endpoint_ = std::move(endpoint);
                status_ = SingleInstanceStatus::Primary;
                worker_ = std::thread([this, listener = std::move(*srv)]() mutable {
                    AcceptLoop(std::move(listener));
                });
                return status_;
            }
        }

        std::lock_guard lock(mutex_);
        status_ = SingleInstanceStatus::Unavailable;
        return status_;
    }

    void SetActivationHandler(std::function<void()> on_activate) {
        std::function<void()> pending_cb;
        {
            std::lock_guard lock(mutex_);
            on_activate_ = std::move(on_activate);
            if (on_activate_ && pending_activation_ &&
                !stop_requested_.load(std::memory_order_acquire)) {
                pending_activation_ = false;
                pending_cb = on_activate_;
            }
        }
        if (pending_cb) {
            pending_cb();
        }
    }

    void Release() {
        namespace os = mcpplibs::os;
        std::thread worker_to_join;
        SingleInstanceEndpoint ep;
        bool was_primary = false;
        {
            std::lock_guard lock(mutex_);
            stop_requested_.store(true, std::memory_order_release);
            worker_to_join = std::move(worker_);
            ep = endpoint_;
            was_primary = (status_ == SingleInstanceStatus::Primary);
            on_activate_ = {};
            pending_activation_ = false;
            status_ = SingleInstanceStatus::Unavailable;
            endpoint_ = {};
        }

        if (worker_to_join.joinable()) {
            if (was_primary && !ep.directory.empty() && !ep.name.empty()) {
                if (auto wake = os::ipc::connection::connect(ep.directory, ep.name)) {
                    wake->close();
                }
            }
            worker_to_join.join();
        }
    }

    [[nodiscard]] SingleInstanceStatus Status() const {
        std::lock_guard lock(mutex_);
        return status_;
    }

    [[nodiscard]] bool ShouldContinueLaunch() const {
        return Status() != SingleInstanceStatus::SecondaryNotified;
    }

    [[nodiscard]] SingleInstanceEndpoint Endpoint() const {
        std::lock_guard lock(mutex_);
        return endpoint_;
    }

private:
    void DispatchActivation() {
        std::function<void()> cb;
        {
            std::lock_guard lock(mutex_);
            if (stop_requested_.load(std::memory_order_acquire)) {
                return;
            }
            if (on_activate_) {
                cb = on_activate_;
            } else {
                pending_activation_ = true;
                return;
            }
        }
        if (cb) {
            cb();
        }
    }

    void AcceptLoop(mcpplibs::os::ipc::listener srv) {
        while (!stop_requested_.load(std::memory_order_acquire)) {
            auto accepted = srv.accept_for(std::chrono::milliseconds(100));
            if (stop_requested_.load(std::memory_order_acquire)) {
                if (accepted && *accepted) {
                    (*accepted)->close();
                }
                break;
            }
            if (!accepted) {
                break;
            }
            if (!*accepted) {
                continue;
            }

            mcpplibs::os::ipc::connection peer = std::move(**accepted);
            std::string payload;
            while (payload.size() < 256 && !stop_requested_.load(std::memory_order_acquire)) {
                const auto chunk = peer.read_for(std::chrono::milliseconds(500), 256 - payload.size());
                if (!chunk || !*chunk || (*chunk)->empty()) {
                    break;
                }
                payload.append(**chunk);
                if (payload.find('\n') != std::string::npos) {
                    break;
                }
            }

            if (stop_requested_.load(std::memory_order_acquire)) {
                peer.close();
                break;
            }

            if (!payload.empty() &&
                std::string_view(payload).find("activate") != std::string_view::npos) {
                DispatchActivation();
                (void)peer.write(kSingleInstanceAck);
                peer.shutdown_write();
            }
            peer.close();
        }
        srv.close();
    }

    mutable std::mutex mutex_;
    std::atomic<bool> stop_requested_{false};
    SingleInstanceStatus status_ = SingleInstanceStatus::Unavailable;
    SingleInstanceEndpoint endpoint_;
    std::function<void()> on_activate_;
    bool pending_activation_ = false;
    std::thread worker_;
};

} // namespace hxlings

namespace hxlings::detail {

struct ProcessSingleInstanceState {
    std::mutex mutex;
    SingleInstanceGuard guard;
    std::function<void()> activation_handler;
};

inline ProcessSingleInstanceState& GlobalSingleInstanceState() {
    static ProcessSingleInstanceState state;
    return state;
}

} // namespace hxlings::detail

export namespace hxlings {

inline SingleInstanceStatus AcquireProcessSingleInstance() {
    auto& state = detail::GlobalSingleInstanceState();
    std::lock_guard lock(state.mutex);
    return state.guard.Acquire(ResolveDefaultSingleInstanceEndpoint(), state.activation_handler);
}

inline void SetProcessSingleInstanceActivationHandler(std::function<void()> on_activate) {
    auto& state = detail::GlobalSingleInstanceState();
    std::lock_guard lock(state.mutex);
    state.activation_handler = on_activate;
    state.guard.SetActivationHandler(std::move(on_activate));
}

inline void ReleaseProcessSingleInstance(bool clear_handler = true) {
    auto& state = detail::GlobalSingleInstanceState();
    std::lock_guard lock(state.mutex);
    if (clear_handler) {
        state.activation_handler = {};
    }
    state.guard.Release();
}

class ProcessSingleInstanceScope {
public:
    ProcessSingleInstanceScope(const ProcessSingleInstanceScope&) = delete;
    ProcessSingleInstanceScope& operator=(const ProcessSingleInstanceScope&) = delete;
    ProcessSingleInstanceScope(ProcessSingleInstanceScope&& other) noexcept
        : status_(other.status_), active_(std::exchange(other.active_, false)) {}
    ProcessSingleInstanceScope& operator=(ProcessSingleInstanceScope&&) = delete;

    ~ProcessSingleInstanceScope() {
        if (active_) {
            ReleaseProcessSingleInstance(true);
        }
    }

    static ProcessSingleInstanceScope AcquireDefault() {
        return ProcessSingleInstanceScope(AcquireProcessSingleInstance());
    }

    [[nodiscard]] SingleInstanceStatus Status() const noexcept {
        return status_;
    }

    [[nodiscard]] bool ShouldContinueLaunch() const noexcept {
        return status_ != SingleInstanceStatus::SecondaryNotified;
    }

private:
    explicit ProcessSingleInstanceScope(SingleInstanceStatus status) noexcept
        : status_(status), active_(true) {}

    SingleInstanceStatus status_ = SingleInstanceStatus::Unavailable;
    bool active_ = false;
};

} // namespace hxlings

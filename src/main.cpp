import std;
import mcpplibs.os;
import huxerui;
import app;

namespace {

void ConfigureLinuxGraphicsEnv() {
    namespace os = mcpplibs::os;
    if constexpr (os::is_linux) {
        using SetenvFn = int (*)(const char*, const char*, int);
        using UnsetenvFn = int (*)(const char*);
        const auto set_env = reinterpret_cast<SetenvFn>(os::library::process_symbol("setenv"));
        const auto unset_env = reinterpret_cast<UnsetenvFn>(os::library::process_symbol("unsetenv"));
        if (set_env == nullptr) return;

        const auto non_empty = [](const std::optional<std::string>& v) {
            return v.has_value() && !v->empty();
        };

        if (!non_empty(os::env::get("GDK_BACKEND"))) {
            if (non_empty(os::env::get("WAYLAND_DISPLAY")) && non_empty(os::env::get("DISPLAY"))) {
                set_env("GDK_BACKEND", "x11", 1);
                if (unset_env != nullptr &&
                    os::env::get("EGL_PLATFORM") == std::optional<std::string>("wayland")) {
                    unset_env("EGL_PLATFORM");
                }
            }
        }
        const auto probe = os::env::get("INTEL_FORCE_PROBE");
        if (!non_empty(probe) || *probe == "1") {
            set_env("INTEL_FORCE_PROBE", "*", 1);
        }
    }
}

} // namespace

int main() {
    ConfigureLinuxGraphicsEnv();
    return huxerui::RunApplication();
}

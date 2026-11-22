export module hxlings.platform_icon;

import std;
import mcpplibs.os;

export namespace hxlings {

inline std::filesystem::path ResolveBundledResourcesDir(const std::filesystem::path& bundle_dir) {
    if (bundle_dir.empty()) {
        return {};
    }
    std::error_code ec;
    for (const std::filesystem::path& candidate : {
             bundle_dir / "bin" / "hxlings.resources",
             bundle_dir / "hxlings.resources",
         }) {
        if (std::filesystem::is_regular_file(candidate / "huxerui" / "resources.bin", ec)) {
            return candidate;
        }
    }
    return {};
}

} // namespace hxlings

namespace hxlings::detail {

namespace os = mcpplibs::os;

inline std::filesystem::path ResolveAppIconPngPath() {
    std::vector<std::filesystem::path> candidates;
    if (const auto res_dir = os::env::get("HUXERUI_RESOURCES_DIR");
        res_dir.has_value() && !res_dir->empty()) {
        candidates.push_back(std::filesystem::path(*res_dir) / "huxerui" / "app" / "images" / "hxlings.png");
    }
    if (const auto bundle_dir = os::env::get("MCPP_BUNDLE_DIR");
        bundle_dir.has_value() && !bundle_dir->empty()) {
        if (const auto bundled_res = hxlings::ResolveBundledResourcesDir(os::fs::from_utf8(*bundle_dir));
            !bundled_res.empty()) {
            candidates.push_back(bundled_res / "huxerui" / "app" / "images" / "hxlings.png");
        }
    }
    if (const auto exe = os::env::image(); exe.has_value() && !exe->empty()) {
        const auto exe_dir = exe->parent_path();
        candidates.push_back(exe_dir / "hxlings.resources" / "huxerui" / "app" / "images" / "hxlings.png");
        candidates.push_back(exe_dir.parent_path() / "Resources" / "HuxerUI" / "huxerui" / "app" / "images" / "hxlings.png");
        candidates.push_back(exe_dir / "resources" / "images" / "hxlings.png");
    }
    candidates.push_back(std::filesystem::path("resources") / "images" / "hxlings.png");

    std::error_code ec;
    for (const auto& candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

inline void ConfigureLinuxBundledResourcesEnv() {
    if constexpr (os::is_linux) {
        const auto non_empty = [](const std::optional<std::string>& value) {
            return value.has_value() && !value->empty();
        };
        if (non_empty(os::env::get("HUXERUI_RESOURCES_DIR"))) {
            return;
        }
        const auto bundle_dir = os::env::get("MCPP_BUNDLE_DIR");
        if (!non_empty(bundle_dir)) {
            return;
        }
        const auto res_dir = hxlings::ResolveBundledResourcesDir(os::fs::from_utf8(*bundle_dir));
        if (res_dir.empty()) {
            return;
        }
        using SetenvFn = int (*)(const char*, const char*, int);
        if (const auto set_env = reinterpret_cast<SetenvFn>(os::library::process_symbol("setenv"));
            set_env != nullptr) {
            const std::string res_utf8 = os::fs::utf8(res_dir);
            if (!res_utf8.empty()) {
                set_env("HUXERUI_RESOURCES_DIR", res_utf8.c_str(), 1);
            }
        }
    }
}

inline void ConfigureLinuxGraphicsEnv() {
    if constexpr (os::is_linux) {
        using SetenvFn = int (*)(const char*, const char*, int);
        using UnsetenvFn = int (*)(const char*);
        const auto set_env = reinterpret_cast<SetenvFn>(os::library::process_symbol("setenv"));
        const auto unset_env = reinterpret_cast<UnsetenvFn>(os::library::process_symbol("unsetenv"));
        if (set_env == nullptr) {
            return;
        }

        const auto non_empty = [](const std::optional<std::string>& value) {
            return value.has_value() && !value->empty();
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

template <bool Enabled>
void ApplyMacOsApplicationIconImpl() {
    if constexpr (Enabled) {
        namespace objc = os::macos::objc;
        if (!objc::available().has_value()) {
            return;
        }
        (void)objc::load_framework("AppKit");
        objc::autorelease_pool pool;

        const auto ns_app_cls = objc::class_named("NSApplication");
        const auto ns_image_cls = objc::class_named("NSImage");
        if (ns_app_cls == nullptr || ns_image_cls == nullptr) {
            return;
        }
        const auto ns_app = objc::send<void*>(ns_app_cls, "sharedApplication");
        if (ns_app == nullptr) {
            return;
        }
        const auto icon_path = ResolveAppIconPngPath();
        if (icon_path.empty()) {
            return;
        }
        const auto ns_path = objc::string(os::fs::utf8(icon_path));
        if (!ns_path) {
            return;
        }
        const auto alloc_img = objc::send<void*>(ns_image_cls, "alloc");
        const auto ns_image = objc::id::adopt(
            objc::send<void*>(alloc_img, "initWithContentsOfFile:", ns_path.raw()));
        if (ns_image) {
            objc::send<void>(ns_app, "setApplicationIconImage:", ns_image.raw());
        }
    }
}

template <bool Enabled>
void ApplyLinuxApplicationIconImpl() {
    if constexpr (Enabled) {
        using GetDisplayFn = void* (*)();
        using GetThemeFn = void* (*)(void*);
        using AddSearchPathFn = void (*)(void*, const char*);
        using SetDefaultIconFn = void (*)(const char*);

        const auto get_display = reinterpret_cast<GetDisplayFn>(
            os::library::process_symbol("gdk_display_get_default"));
        const auto get_theme = reinterpret_cast<GetThemeFn>(
            os::library::process_symbol("gtk_icon_theme_get_for_display"));
        const auto add_search_path = reinterpret_cast<AddSearchPathFn>(
            os::library::process_symbol("gtk_icon_theme_add_search_path"));
        const auto set_default_icon = reinterpret_cast<SetDefaultIconFn>(
            os::library::process_symbol("gtk_window_set_default_icon_name"));
        if (get_display == nullptr || get_theme == nullptr ||
            add_search_path == nullptr || set_default_icon == nullptr) {
            return;
        }

        void* display = get_display();
        if (display == nullptr) {
            return;
        }
        void* theme = get_theme(display);
        if (theme == nullptr) {
            return;
        }
        const auto icon_path = ResolveAppIconPngPath();
        if (!icon_path.empty()) {
            const std::string dir_utf8 = os::fs::utf8(icon_path.parent_path());
            if (!dir_utf8.empty()) {
                add_search_path(theme, dir_utf8.c_str());
            }
        }
        set_default_icon("hxlings");
    }
}

struct Win32IconPair {
    void* small_icon = nullptr;
    void* big_icon = nullptr;
    std::intptr_t (*send_message)(void*, std::uint32_t, std::uintptr_t, std::intptr_t) = nullptr;
};

template <bool Enabled>
void ApplyWin32WindowIconImpl() {
    if constexpr (Enabled) {
        using GetModuleHandleWFn = void* (*)(const wchar_t*);
        using GetCurrentThreadIdFn = std::uint32_t (*)();
        using LoadImageWFn = void* (*)(void*, const wchar_t*, std::uint32_t, int, int, std::uint32_t);
        using GetSystemMetricsFn = int (*)(int);
        using EnumWindowsProcFn = int (*)(void*, std::intptr_t);
        using EnumThreadWindowsFn = int (*)(std::uint32_t, EnumWindowsProcFn, std::intptr_t);
        using SendMessageWFn = std::intptr_t (*)(void*, std::uint32_t, std::uintptr_t, std::intptr_t);

        const auto kernel32 = os::library::loaded("kernel32.dll");
        const auto user32 = os::library::loaded("user32.dll");
        if (!kernel32.has_value() || !user32.has_value()) {
            return;
        }
        const auto get_module = kernel32->function<void*(const wchar_t*)>("GetModuleHandleW");
        const auto get_tid = kernel32->function<std::uint32_t()>("GetCurrentThreadId");
        const auto load_image = user32->function<void*(void*, const wchar_t*, std::uint32_t, int, int, std::uint32_t)>("LoadImageW");
        const auto get_metric = user32->function<int(int)>("GetSystemMetrics");
        const auto enum_thread_windows = user32->function<int(std::uint32_t, EnumWindowsProcFn, std::intptr_t)>("EnumThreadWindows");
        const auto send_message = user32->function<std::intptr_t(void*, std::uint32_t, std::uintptr_t, std::intptr_t)>("SendMessageW");

        (void)sizeof(GetModuleHandleWFn);
        (void)sizeof(GetCurrentThreadIdFn);
        (void)sizeof(LoadImageWFn);
        (void)sizeof(GetSystemMetricsFn);
        (void)sizeof(EnumThreadWindowsFn);
        (void)sizeof(SendMessageWFn);

        if (get_module == nullptr || get_tid == nullptr || load_image == nullptr ||
            enum_thread_windows == nullptr || send_message == nullptr) {
            return;
        }

        void* hinst = get_module(nullptr);
        if (hinst == nullptr) {
            return;
        }
        const int big_cx = get_metric != nullptr ? get_metric(11) : 32;
        const int big_cy = get_metric != nullptr ? get_metric(12) : 32;
        const int small_cx = get_metric != nullptr ? get_metric(49) : 16;
        const int small_cy = get_metric != nullptr ? get_metric(50) : 16;

        constexpr std::uint32_t kImageIcon = 1;
        constexpr std::uint32_t kLrShared = 0x8000;
        const auto* resource_id = reinterpret_cast<const wchar_t*>(static_cast<std::uintptr_t>(1));

        Win32IconPair icons{
            .small_icon = load_image(hinst, resource_id, kImageIcon, small_cx, small_cy, kLrShared),
            .big_icon = load_image(hinst, resource_id, kImageIcon, big_cx, big_cy, kLrShared),
            .send_message = send_message,
        };
        if (icons.small_icon == nullptr && icons.big_icon == nullptr) {
            return;
        }

        const auto enum_cb = [](void* hwnd, std::intptr_t lparam) -> int {
            const auto* pair = reinterpret_cast<const Win32IconPair*>(lparam);
            if (hwnd != nullptr && pair != nullptr && pair->send_message != nullptr) {
                constexpr std::uint32_t kWmSetIcon = 0x0080;
                if (pair->small_icon != nullptr) {
                    pair->send_message(hwnd, kWmSetIcon, 0, reinterpret_cast<std::intptr_t>(pair->small_icon));
                }
                if (pair->big_icon != nullptr) {
                    pair->send_message(hwnd, kWmSetIcon, 1, reinterpret_cast<std::intptr_t>(pair->big_icon));
                }
            }
            return 1;
        };
        enum_thread_windows(get_tid(), enum_cb, reinterpret_cast<std::intptr_t>(&icons));
    }
}

} // namespace hxlings::detail

export namespace hxlings {

inline void PreparePlatformStartup() {
    namespace os = mcpplibs::os;
    detail::ConfigureLinuxBundledResourcesEnv();
    detail::ConfigureLinuxGraphicsEnv();
    if constexpr (os::is_linux) {
        using SetPrgnameFn = void (*)(const char*);
        if (const auto set_prgname = reinterpret_cast<SetPrgnameFn>(
                os::library::process_symbol("g_set_prgname"));
            set_prgname != nullptr) {
            set_prgname("hxlings");
        }
    }
}

inline void ApplyPlatformApplicationIcon() {
    namespace os = mcpplibs::os;
    detail::ApplyMacOsApplicationIconImpl<os::is_macos>();
    detail::ApplyLinuxApplicationIconImpl<os::is_linux>();
}

inline void ApplyPlatformWindowIcon() {
    namespace os = mcpplibs::os;
    detail::ApplyWin32WindowIconImpl<os::is_windows>();
}

} // namespace hxlings

#if defined(__linux__)
#include <stdlib.h>
#include <string.h>
#endif

import huxerui;
import app;

#if defined(_WIN32) && defined(_MSC_VER)
#pragma comment(linker, "/subsystem:windows")
#pragma comment(linker, "/entry:mainCRTStartup")
#endif

int main() {
#if defined(__linux__)
    if (const char* backend = ::getenv("GDK_BACKEND"); backend == nullptr || *backend == '\0') {
        const char* wayland = ::getenv("WAYLAND_DISPLAY");
        const char* x11 = ::getenv("DISPLAY");
        if (wayland != nullptr && *wayland != '\0' && x11 != nullptr && *x11 != '\0') {
            ::setenv("GDK_BACKEND", "x11", 1);
            if (const char* egl_platform = ::getenv("EGL_PLATFORM");
                egl_platform != nullptr && ::strcmp(egl_platform, "wayland") == 0) {
                ::unsetenv("EGL_PLATFORM");
            }
        }
    }
    if (const char* probe = ::getenv("INTEL_FORCE_PROBE");
        probe == nullptr || *probe == '\0' || ::strcmp(probe, "1") == 0) {
        ::setenv("INTEL_FORCE_PROBE", "*", 1);
    }
#endif
    return huxerui::RunApplication();
}

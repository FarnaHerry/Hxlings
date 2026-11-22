#if defined(__linux__)
#include <stdlib.h>
#endif

import huxerui;
import app;

#if defined(_WIN32) && defined(_MSC_VER)
#pragma comment(linker, "/subsystem:windows")
#pragma comment(linker, "/entry:mainCRTStartup")
#endif

int main() {
#if defined(__linux__)
    if (::getenv("GDK_BACKEND") == nullptr) {
        const char* wayland = ::getenv("WAYLAND_DISPLAY");
        const char* x11 = ::getenv("DISPLAY");
        if (wayland != nullptr && *wayland != '\0' && x11 != nullptr && *x11 != '\0') {
            ::setenv("GDK_BACKEND", "x11", 0);
        }
    }
#endif
    return huxerui::RunApplication();
}

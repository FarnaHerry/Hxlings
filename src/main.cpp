import huxerui;
import hxlings.platform_icon;
import hxlings.single_instance;
import app;

int main() {
    const auto instance_guard = hxlings::ProcessSingleInstanceScope::AcquireDefault();
    if (!instance_guard.ShouldContinueLaunch()) {
        return 0;
    }
    hxlings::PreparePlatformStartup();
    return huxerui::RunApplication();
}

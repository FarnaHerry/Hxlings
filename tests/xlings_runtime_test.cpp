import std;
import mcpplibs.os;
import nlohmann.json;
import xlings.types;
import xlings.parser;
import xlings.process;
import xlings.client;
import hxlings.settings;
import hxlings.platform_icon;
import hxlings.updater;
import hxlings.single_instance;
import hxlings.luban.types;
import hxlings.luban.client;
import hxlings.luban.stack;

using namespace hxlings;

namespace {

int failures = 0;

void Check(bool cond, std::string_view msg) {
    if (!cond) {
        std::println(std::cerr, "FAIL: {}", msg);
        ++failures;
    }
}

void TestProcessStreamingAndCancel() {
    const std::string bin = ResolveXlingsBinary();
    Check(!bin.empty(), "ResolveXlingsBinary returns non-empty path");

    std::vector<std::string> streamed_lines;
    const ProcessResult ver = RunProcessStreaming(
        {bin, "interface", "--version"},
        [&](const std::string& line) {
            if (!line.empty()) streamed_lines.push_back(line);
        }
    );
    Check(ver.exit_code == 0, "RunProcessStreaming interface --version exit 0");
    Check(!ver.raw_output.empty(), "RunProcessStreaming captures raw_output");
    Check(!streamed_lines.empty(), "RunProcessStreaming streams non-empty lines via callback");
    const XlingsVersionInfo parsed_ver = ParseVersionJson(ver.raw_output);
    Check(!parsed_ver.version.empty(), "Live interface --version parses version cleanly");

    auto cancelled_ctrl = std::make_shared<ActiveProcessControl>();
    cancelled_ctrl->Cancel();
    const ProcessResult aborted = RunProcessStreaming({bin, "interface", "--version"}, {}, cancelled_ctrl);
    Check(aborted.cancelled && aborted.exit_code < 0,
          "RunProcessStreaming respects pre-cancelled ActiveProcessControl");
}

void TestProcessStreamingExceptionSafety() {
    const std::string bin = ResolveXlingsBinary();
    auto ctrl = std::make_shared<ActiveProcessControl>();
    bool caught = false;
    try {
        (void)RunProcessStreaming(
            {bin, "interface", "--version"},
            [](const std::string&) {
                throw std::runtime_error("simulated callback exception");
            },
            ctrl
        );
    } catch (const std::runtime_error&) {
        caught = true;
    }
    Check(caught, "RunProcessStreaming propagates callback exception cleanly");
    Check(ctrl->pid.load(std::memory_order_acquire) == -1,
          "PidResetGuard resets active pid to -1 even on callback exception");
}

void TestAppSettingsSerializationAndPersistence() {
    {
        const AppSettings def = ParseAppSettingsJson("{}");
        Check(def.theme_mode == ThemeMode::Auto && def.brand_preset_index == 0 &&
                  def.tray_enabled && def.close_to_tray && !def.lightweight_mode,
              "ParseAppSettingsJson uses expected defaults on empty object");
    }
    {
        const AppSettings migrated_dark = ParseAppSettingsJson("{\"dark_mode\": true}");
        Check(migrated_dark.theme_mode == ThemeMode::Dark,
              "ParseAppSettingsJson migrates legacy dark_mode=true to ThemeMode::Dark");
        const AppSettings migrated_light = ParseAppSettingsJson("{\"dark_mode\": false}");
        Check(migrated_light.theme_mode == ThemeMode::Light,
              "ParseAppSettingsJson migrates legacy dark_mode=false to ThemeMode::Light");
    }
    {
        const AppSettings invalid = ParseAppSettingsJson("not json");
        Check(invalid == AppSettings{}, "ParseAppSettingsJson falls back to defaults on invalid JSON");
    }
    {
        const AppSettings clamped = ParseAppSettingsJson("{\"brand_preset_index\": -3}");
        Check(clamped.brand_preset_index == 0, "ParseAppSettingsJson clamps negative brand_preset_index");
    }
    {
        const AppSettings expected{
            .theme_mode = ThemeMode::Dark,
            .brand_preset_index = 4,
            .tray_enabled = false,
            .close_to_tray = false,
            .lightweight_mode = true,
        };
        const std::string encoded = SerializeAppSettingsJson(expected);
        const AppSettings decoded = ParseAppSettingsJson(encoded);
        Check(decoded == expected, "SerializeAppSettingsJson round-trips all fields");

        std::error_code ec;
        const auto dir = std::filesystem::temp_directory_path(ec) / "hxlings-settings-test";
        std::filesystem::remove_all(dir, ec);
        const auto file = dir / "settings.json";
        Check(SaveAppSettings(expected, file), "SaveAppSettings persists settings file");
        Check(LoadAppSettings(file) == expected, "LoadAppSettings reads persisted settings cleanly");
        std::filesystem::remove_all(dir, ec);
    }
}

void TestBundledResourcesAndChildEnvSanitization() {
    {
        std::error_code ec;
        const auto root = std::filesystem::temp_directory_path(ec) / "hxlings-bundle-res-test";
        std::filesystem::remove_all(root, ec);

        Check(ResolveBundledResourcesDir(root).empty(),
              "ResolveBundledResourcesDir returns empty when resources.bin is absent");

        const auto flat_root = root / "flat";
        const auto flat_res = flat_root / "hxlings.resources";
        std::filesystem::create_directories(flat_res / "huxerui", ec);
        {
            std::ofstream out(flat_res / "huxerui" / "resources.bin", std::ios::binary);
            out << "bin";
        }
        Check(ResolveBundledResourcesDir(flat_root) == flat_res,
              "ResolveBundledResourcesDir falls back to <bundle>/hxlings.resources");

        const auto nested_root = root / "nested";
        const auto nested_res = nested_root / "bin" / "hxlings.resources";
        std::filesystem::create_directories(nested_res / "huxerui", ec);
        {
            std::ofstream out(nested_res / "huxerui" / "resources.bin", std::ios::binary);
            out << "bin";
        }
        Check(ResolveBundledResourcesDir(nested_root) == nested_res,
              "ResolveBundledResourcesDir prefers <bundle>/bin/hxlings.resources");

        std::filesystem::remove_all(root, ec);
    }
    {
        Check(PathMatchesPrefixOrRegistry("/opt/hxlings/lib/gconv", "/opt/hxlings"),
              "PathMatchesPrefixOrRegistry matches exact prefix subpath");
        Check(PathMatchesPrefixOrRegistry("/opt/hxlings/lib", "/opt/hxlings/"),
              "PathMatchesPrefixOrRegistry normalizes trailing slash");
        Check(!PathMatchesPrefixOrRegistry("/opt/hxlings-custom/lib", "/opt/hxlings"),
              "PathMatchesPrefixOrRegistry ignores sibling prefix with shared stem");
        Check(PathMatchesPrefixOrRegistry("/home/u/.mcpp/registry/data/xpkgs/xim-x-glibc/2.44/lib64", ""),
              "PathMatchesPrefixOrRegistry matches .mcpp/registry");

        const auto g1 = SanitizeColonPathList("/opt/hxlings/lib/gconv", "/opt/hxlings");
        Check(!g1.has_value(), "SanitizeColonPathList returns nullopt when all entries stripped");

        const auto g2 = SanitizeColonPathList("/usr/lib64/gconv:/opt/hxlings/lib/gconv", "/opt/hxlings");
        Check(g2 == std::optional<std::string>("/usr/lib64/gconv"),
              "SanitizeColonPathList keeps host gconv path while removing bundled path");

        const auto g3 = SanitizeColonPathList("/opt/hxlings-custom/lib:/opt/hxlings/lib", "/opt/hxlings");
        Check(g3 == std::optional<std::string>("/opt/hxlings-custom/lib"),
              "SanitizeColonPathList preserves /opt/hxlings-custom/lib");

        const auto g4 = SanitizeColonPathList(
            "/home/u/.mcpp/registry/data/xpkgs/xim-x-glibc/2.44/lib64:/usr/local/lib", "");
        Check(g4 == std::optional<std::string>("/usr/local/lib"),
              "SanitizeColonPathList strips .mcpp/registry paths");
    }
    {
        const auto desktop_path = BuildSanitizedSearchPath(
            {"/usr/local/bin", "/usr/bin", "/bin"},
            {"/home/demo/.xlings/subos/current/bin", "/home/demo/.xlings/bin"},
            {"/usr/local/bin", "/usr/bin", "/bin"},
            ""
        );
        Check(desktop_path.size() == 5 &&
                  desktop_path[0] == std::filesystem::path("/home/demo/.xlings/subos/current/bin") &&
                  desktop_path[1] == std::filesystem::path("/home/demo/.xlings/bin") &&
                  desktop_path[2] == std::filesystem::path("/usr/local/bin") &&
                  desktop_path[3] == std::filesystem::path("/usr/bin") &&
                  desktop_path[4] == std::filesystem::path("/bin"),
              "BuildSanitizedSearchPath prepends priority xlings dirs and deduplicates fallback host dirs");

        const auto cleaned_path = BuildSanitizedSearchPath(
            {"/home/demo/.mcpp/registry/bin", "/opt/hxlings/bin", "/opt/hxlings-custom/bin", "/usr/bin"},
            {"/home/demo/.xlings/subos/current/bin"},
            {"/usr/bin", "/usr/local/bin"},
            "/opt/hxlings"
        );
        Check(cleaned_path.size() == 4 &&
                  cleaned_path[0] == std::filesystem::path("/home/demo/.xlings/subos/current/bin") &&
                  cleaned_path[1] == std::filesystem::path("/opt/hxlings-custom/bin") &&
                  cleaned_path[2] == std::filesystem::path("/usr/bin") &&
                  cleaned_path[3] == std::filesystem::path("/usr/local/bin"),
              "BuildSanitizedSearchPath strips registry and bundle dirs while preserving sibling custom paths");
    }
    if constexpr (mcpplibs::os::is_posix) {
        std::error_code ec;
        std::filesystem::path temp_base = mcpplibs::os::dirs::temp();
        if (temp_base.empty()) {
            temp_base = std::filesystem::temp_directory_path(ec);
        }
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto tmp_dir = temp_base / ("hxlings-path-spawn-test-" + std::to_string(stamp));
        std::filesystem::remove_all(tmp_dir, ec);
        std::filesystem::create_directories(tmp_dir, ec);

        const auto subcmd = tmp_dir / "stub-subcmd";
        const auto driver = tmp_dir / "stub-driver";
        {
            std::ofstream out(subcmd, std::ios::binary);
            out << "#!/bin/sh\necho sibling-found-ok\n";
        }
        {
            std::ofstream out(driver, std::ios::binary);
            out << "#!/bin/sh\nstub-subcmd\n";
        }
        constexpr auto exec_perms = std::filesystem::perms::owner_all |
                                    std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
                                    std::filesystem::perms::others_read | std::filesystem::perms::others_exec;
        std::filesystem::permissions(subcmd, exec_perms, std::filesystem::perm_options::replace, ec);
        std::filesystem::permissions(driver, exec_perms, std::filesystem::perm_options::replace, ec);

        const ProcessResult res = RunProcessStreaming({mcpplibs::os::fs::utf8(driver)});
        Check(res.exit_code == 0,
              "POSIX RunProcessStreaming prepends executable dir so sibling bare-command lookup avoids exit 127");
        Check(res.raw_output.find("sibling-found-ok") != std::string::npos,
              "POSIX RunProcessStreaming captures output from sibling bare-command invocation");

        std::filesystem::remove_all(tmp_dir, ec);
    }
}

void TestWindowsProcessHelpersAndSpawn() {
    Check(QuoteWindowsArg("plain") == "plain", "QuoteWindowsArg leaves plain token unquoted");
    Check(QuoteWindowsArg("") == "\"\"", "QuoteWindowsArg quotes empty arg");
    Check(QuoteWindowsArg("hello world") == "\"hello world\"", "QuoteWindowsArg wraps whitespace");
    Check(QuoteWindowsArg("say \"hi\"") == "\"say \\\"hi\\\"\"", "QuoteWindowsArg escapes quotes");
    Check(QuoteWindowsArg("C:\\Program Files\\") == "\"C:\\Program Files\\\\\"",
          "QuoteWindowsArg doubles trailing backslash before closing quote");
    Check(QuoteWindowsArg("a\\\\\"b") == "\"a\\\\\\\\\\\"b\"",
          "QuoteWindowsArg doubles backslashes before internal quote");

    {
        const std::string cmd = BuildWindowsCommandLine({
            "C:\\Program Files\\xlings.exe",
            "search",
            "hello \"world\"",
        });
        Check(cmd == "\"C:\\Program Files\\xlings.exe\" search \"hello \\\"world\\\"\"",
              "BuildWindowsCommandLine builds exe command line");

        const std::string bat = BuildWindowsCommandLine({
            "C:\\My Tools\\run.CMD",
            "interface",
            "list_packages",
        });
        Check(bat == "cmd.exe /d /v:off /s /c \"\"C:\\My Tools\\run.CMD\" interface list_packages\"",
              "BuildWindowsCommandLine wraps batch script via cmd.exe");
    }

    {
        const std::vector<std::string> base = {
            "=C:=C:\\work",
            "Path=C:\\a",
            "XLINGS_HOME=C:\\Users\\demo\\.mcpp\\registry",
            "alpha=1",
        };
        const std::vector<std::pair<std::string, std::optional<std::string>>> overrides = {
            {"xlings_home", std::nullopt},
            {"path", "C:\\clean"},
            {"Beta", "2"},
        };
        const auto merged = MergeWindowsEnvEntries(base, overrides);
        Check(merged.size() == 4, "MergeWindowsEnvEntries count after erase+insert");
        Check(merged[0] == "=C:=C:\\work", "MergeWindowsEnvEntries keeps drive-cwd hidden entry first");
        Check(merged[1] == "alpha=1", "MergeWindowsEnvEntries preserves untouched var sorted");
        Check(merged[2] == "Beta=2", "MergeWindowsEnvEntries inserts override in case-insensitive order");
        Check(merged[3] == "path=C:\\clean", "MergeWindowsEnvEntries replaces case-insensitively");
    }

    if constexpr (mcpplibs::os::is_windows) {
        std::vector<std::string> lines;
        const ProcessResult res = RunProcessStreaming(
            {"cmd.exe", "/d", "/c", "echo hxlings-win-ok"},
            [&](const std::string& line) { lines.push_back(line); }
        );
        Check(res.exit_code == 0, "Windows RunProcessStreaming cmd echo exit code 0");
        Check(!res.cancelled, "Windows RunProcessStreaming cmd echo not cancelled");
        Check(res.raw_output.find("hxlings-win-ok") != std::string::npos,
              "Windows RunProcessStreaming cmd echo raw_output contains marker");
        bool saw_line = false;
        for (const auto& l : lines) {
            if (l.find("hxlings-win-ok") != std::string::npos) {
                saw_line = true;
                break;
            }
        }
        Check(saw_line, "Windows RunProcessStreaming cmd echo streamed line");

        auto ctrl = std::make_shared<ActiveProcessControl>();
        const auto t0 = std::chrono::steady_clock::now();
        std::thread canceller([ctrl] {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            ctrl->Cancel();
        });
        const ProcessResult cancel_res = RunProcessStreaming(
            {"cmd.exe", "/d", "/c", "ping -n 10 127.0.0.1 >nul"},
            {},
            ctrl
        );
        canceller.join();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0);
        Check(cancel_res.cancelled, "Windows RunProcessStreaming marks cancelled=true");
        Check(elapsed < std::chrono::seconds(3), "Windows RunProcessStreaming cancel completes promptly");
    }
}

} // namespace

void TestExternalUrlValidationAndNormalization() {
    Check(IsSafeExternalUrl("https://github.com/FarnaHerry/Hxlings"), "https url valid");
    Check(IsSafeExternalUrl("HTTP://Example.com/docs?q=1#top"), "http uppercase scheme valid");
    Check(NormalizeBrowserUrl("  HTTPS://GitHub.com/HuxerUI/HuxerUI  ") ==
              "https://GitHub.com/HuxerUI/HuxerUI",
          "trimmed and lowercased scheme");
    Check(!IsSafeExternalUrl(""), "empty url rejected");
    Check(!IsSafeExternalUrl("javascript:alert(1)"), "javascript scheme rejected");
    Check(!IsSafeExternalUrl("file:///etc/passwd"), "file scheme rejected");
    Check(!IsSafeExternalUrl("https://github.com/a b"), "whitespace rejected");
    Check(!IsSafeExternalUrl("https://\"quoted\""), "quotes rejected");
    Check(!IsSafeExternalUrl("https:///nohost"), "missing host rejected");
}

void TestAppUpdaterParsingSha256AndSelection() {
    Check(NormalizeSemverTag(" v0.1.6 ") == "0.1.6", "NormalizeSemverTag strips v prefix and whitespace");
    Check(IsNewerAppVersion("0.1.5", "v0.1.6"), "0.1.6 > 0.1.5");
    Check(IsNewerAppVersion("0.1.5", "0.1.10"), "0.1.10 > 0.1.5 numeric semver");
    Check(!IsNewerAppVersion("0.1.5", "v0.1.5"), "same version is not newer");
    Check(!IsNewerAppVersion("0.1.5", "0.1.4"), "older version is not newer");

    Check(Sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "SHA-256 empty string matches FIPS 180-4");
    Check(Sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "SHA-256 'abc' matches FIPS 180-4");

    Sha256Hasher chunked;
    chunked.UpdateString("a");
    chunked.UpdateString("bc");
    Check(chunked.FinalHex() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "chunked SHA-256 matches single-shot");

    const std::string gh_json = R"json({
  "tag_name": "v0.1.6",
  "name": "Hxlings v0.1.6",
  "published_at": "2026-11-23T01:45:00Z",
  "html_url": "https://github.com/FarnaHerry/Hxlings/releases/tag/v0.1.6",
  "body": "## What's New\n- Tray dark theme sync\n- Self-update support",
  "assets": [
    {"name": "bad-xss.exe", "browser_download_url": "javascript:alert(1)", "size": 100},
    {"name": "hxlings-0.1.6-windows-x86_64-setup.exe", "browser_download_url": "https://github.com/FarnaHerry/Hxlings/releases/download/v0.1.6/hxlings-0.1.6-windows-x86_64-setup.exe", "size": 4655519},
    {"name": "hxlings-0.1.6-windows-x86_64.msi", "browser_download_url": "https://github.com/FarnaHerry/Hxlings/releases/download/v0.1.6/hxlings-0.1.6-windows-x86_64.msi", "size": 2351104},
    {"name": "hxlings-0.1.6-macos-arm64.dmg", "browser_download_url": "https://github.com/FarnaHerry/Hxlings/releases/download/v0.1.6/hxlings-0.1.6-macos-arm64.dmg", "size": 5203158},
    {"name": "hxlings-0.1.6-macos-arm64.tar.gz", "browser_download_url": "https://github.com/FarnaHerry/Hxlings/releases/download/v0.1.6/hxlings-0.1.6-macos-arm64.tar.gz", "size": 4367917},
    {"name": "hxlings-0.1.6-linux-x86_64.AppImage", "browser_download_url": "https://github.com/FarnaHerry/Hxlings/releases/download/v0.1.6/hxlings-0.1.6-linux-x86_64.AppImage", "size": 33405432},
    {"name": "hxlings-0.1.6-linux-x86_64.deb", "browser_download_url": "https://github.com/FarnaHerry/Hxlings/releases/download/v0.1.6/hxlings-0.1.6-linux-x86_64.deb", "size": 4053158},
    {"name": "hxlings-0.1.6-linux-x86_64.rpm", "browser_download_url": "https://github.com/FarnaHerry/Hxlings/releases/download/v0.1.6/hxlings-0.1.6-linux-x86_64.rpm", "size": 4177714},
    {"name": "hxlings-0.1.6-linux-x86_64.tar.gz", "browser_download_url": "https://github.com/FarnaHerry/Hxlings/releases/download/v0.1.6/hxlings-0.1.6-linux-x86_64.tar.gz", "size": 38158186},
    {"name": "SHA256SUMS.txt", "browser_download_url": "https://github.com/FarnaHerry/Hxlings/releases/download/v0.1.6/SHA256SUMS.txt", "size": 894}
  ]
})json";
    const auto rel = ParseGitHubLatestReleaseJson(gh_json);
    Check(rel.has_value(), "GitHub release JSON parsed");
    Check(rel->tag_name == "v0.1.6" && rel->version == "0.1.6", "release tag/version extracted");
    Check(rel->assets.size() == 8, "unsafe URL filtered and SHA256SUMS separated");
    Check(!rel->checksums_url.empty(), "checksums URL extracted");

    const std::string sums_txt =
        "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855  hxlings-0.1.6-linux-x86_64.AppImage\n"
        "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD *./hxlings-0.1.6-windows-x86_64-setup.exe\n";
    Check(ParseSha256SumsText(sums_txt, "hxlings-0.1.6-linux-x86_64.AppImage") ==
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "SHA256SUMS plain entry parsed lowercase");
    Check(ParseSha256SumsText(sums_txt, "hxlings-0.1.6-windows-x86_64-setup.exe") ==
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "SHA256SUMS *./ entry parsed lowercase");

    const auto win_sel = SelectPlatformReleaseAsset(*rel, {.os_name = "windows", .arch = "x86_64"});
    Check(win_sel.first.has_value() && win_sel.second == UpdateInstallKind::WindowsSetupExe,
          "Windows selects setup.exe over msi/zip");

    const auto mac_sel = SelectPlatformReleaseAsset(*rel, {.os_name = "macos", .arch = "arm64"});
    Check(mac_sel.first.has_value() && mac_sel.second == UpdateInstallKind::MacOsDmg,
          "macOS arm64 selects dmg over tar.gz");

    const auto ai_sel = SelectPlatformReleaseAsset(
        *rel, {.os_name = "linux", .arch = "x86_64", .appimage_path = "/tmp/Hxlings.AppImage", .has_dpkg = true});
    Check(ai_sel.first.has_value() && ai_sel.second == UpdateInstallKind::LinuxAppImage,
          "Linux AppImage runtime prefers AppImage over deb");

    const auto deb_sel = SelectPlatformReleaseAsset(
        *rel, {.os_name = "linux", .arch = "x86_64", .exe_path = "/usr/bin/hxlings", .has_dpkg = true});
    Check(deb_sel.first.has_value() && deb_sel.second == UpdateInstallKind::LinuxDeb,
          "Linux dpkg install selects deb");

    const auto rpm_sel = SelectPlatformReleaseAsset(
        *rel, {.os_name = "linux", .arch = "x86_64", .exe_path = "/usr/bin/hxlings", .has_rpm = true});
    Check(rpm_sel.first.has_value() && rpm_sel.second == UpdateInstallKind::LinuxRpm,
          "Linux rpm install selects rpm");

    const auto port_sel = SelectPlatformReleaseAsset(*rel, {.os_name = "linux", .arch = "x86_64"});
    Check(port_sel.first.has_value() && port_sel.second == UpdateInstallKind::LinuxAppImage,
          "Linux portable fallback picks AppImage");

    const AppReleaseInfo tar_only_rel{
        .tag_name = "v0.1.8",
        .version = "0.1.8",
        .assets = {
            {.name = "hxlings-0.1.8-macos-arm64.tar.gz", .download_url = "https://example.invalid/mac.tar.gz"},
            {.name = "hxlings-0.1.8-macos-arm64.tar.xz", .download_url = "https://example.invalid/mac.tar.xz"},
            {.name = "hxlings-0.1.8-linux-x86_64.tar.gz", .download_url = "https://example.invalid/lin.tar.gz"},
            {.name = "hxlings-0.1.8-linux-x86_64.tar.xz", .download_url = "https://example.invalid/lin.tar.xz"},
        },
    };
    const auto mac_xz = SelectPlatformReleaseAsset(tar_only_rel, {.os_name = "macos", .arch = "arm64"});
    Check(mac_xz.first.has_value() && mac_xz.second == UpdateInstallKind::MacOsTarXz,
          "macOS prefers tar.xz over tar.gz when dmg absent");
    const auto lin_xz = SelectPlatformReleaseAsset(tar_only_rel, {.os_name = "linux", .arch = "x86_64"});
    Check(lin_xz.first.has_value() && lin_xz.second == UpdateInstallKind::LinuxTarXz,
          "Linux prefers tar.xz over tar.gz when package/AppImage absent");
}

void TestSingleInstanceGuard() {
    Check(SanitizeSingleInstanceToken("alice.smith+dev@host!#$", 20) == "alicesmithdevhost",
          "SanitizeSingleInstanceToken strips unsupported punctuation");
    Check(SanitizeSingleInstanceToken("0123456789abcdefghijEXTRA", 20) == "0123456789abcdefghij",
          "SanitizeSingleInstanceToken enforces max length");
    Check(SanitizeSingleInstanceToken("dev_user-01", 20) == "dev_user-01",
          "SanitizeSingleInstanceToken preserves [A-Za-z0-9_-]");

    const SingleInstanceEndpoint def_ep = ResolveDefaultSingleInstanceEndpoint();
    Check(!def_ep.directory.empty(), "ResolveDefaultSingleInstanceEndpoint returns non-empty directory");
    Check(!def_ep.name.empty() && def_ep.name.ends_with(".sock") && def_ep.name.size() <= 40,
          "ResolveDefaultSingleInstanceEndpoint returns bounded .sock name");

    std::error_code ec;
    std::filesystem::path temp_base = mcpplibs::os::dirs::temp();
    if (temp_base.empty()) {
        temp_base = std::filesystem::temp_directory_path(ec);
    }
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto tmp_dir = temp_base / ("hxl-si-" + std::to_string(stamp % 1000000));
    std::filesystem::remove_all(tmp_dir, ec);
    std::filesystem::create_directories(tmp_dir, ec);

    const auto caps = mcpplibs::os::fs::caps(tmp_dir);
    if (caps && caps->local_endpoint) {
        std::atomic<int> activations{0};
        SingleInstanceGuard primary;
        const SingleInstanceStatus s1 = primary.Acquire(
            {tmp_dir, "a.sock"},
            [&activations] { activations.fetch_add(1, std::memory_order_relaxed); }
        );
        Check(s1 == SingleInstanceStatus::Primary && primary.ShouldContinueLaunch(),
              "SingleInstanceGuard first instance becomes Primary");

        SingleInstanceGuard secondary;
        const SingleInstanceStatus s2 = secondary.Acquire({tmp_dir, "a.sock"});
        Check(s2 == SingleInstanceStatus::SecondaryNotified && !secondary.ShouldContinueLaunch(),
              "SingleInstanceGuard second instance notifies Primary and exits launch");
        Check(activations.load(std::memory_order_relaxed) == 1,
              "Primary received first activation before secondary returned");

        SingleInstanceGuard third;
        const SingleInstanceStatus s3 = third.Acquire({tmp_dir, "a.sock"});
        Check(s3 == SingleInstanceStatus::SecondaryNotified &&
                  activations.load(std::memory_order_relaxed) == 2,
              "Primary received second activation");

        SingleInstanceGuard primary2;
        Check(primary2.Acquire({tmp_dir, "b.sock"}) == SingleInstanceStatus::Primary,
              "Primary without initial handler starts cleanly");
        SingleInstanceGuard secondary2;
        Check(secondary2.Acquire({tmp_dir, "b.sock"}) == SingleInstanceStatus::SecondaryNotified,
              "Secondary notifies Primary before activation handler is registered");
        std::atomic<int> delayed_activations{0};
        primary2.SetActivationHandler([&delayed_activations] {
            delayed_activations.fetch_add(1, std::memory_order_relaxed);
        });
        Check(delayed_activations.load(std::memory_order_relaxed) == 1,
              "Buffered early activation fires immediately upon SetActivationHandler");
        primary2.Release();

        primary.Release();
        Check(primary.Status() == SingleInstanceStatus::Unavailable,
              "Release resets Primary guard status");
        Check(!std::filesystem::exists(tmp_dir / "a.sock", ec),
              "Release unlinks primary rendezvous socket");

        SingleInstanceGuard next_primary;
        Check(next_primary.Acquire({tmp_dir, "a.sock"}) == SingleInstanceStatus::Primary,
              "New instance acquires Primary after Release");
        next_primary.Release();

        {
            std::ofstream stale_file(tmp_dir / "c.sock", std::ios::binary);
            stale_file << "stale";
        }
        SingleInstanceGuard stale_guard;
        Check(stale_guard.Acquire({tmp_dir, "c.sock"}) == SingleInstanceStatus::Primary,
              "Primary recovers from stale non-listening rendezvous file");
        stale_guard.Release();
    }

    std::filesystem::remove_all(tmp_dir, ec);
}

void TestLubanParsersAndStackEngine() {
    // 1. Parse host status JSON (with leading [warn] lines matching real luban output)
    {
        const std::string status_raw =
            "[warn] index hint line\n"
            R"json({
  "arch": "x86_64",
  "os": "Linux 7.3.4",
  "ready": true,
  "isolation": {
    "backend": {"name": "bwrap", "path": "/usr/bin/bwrap", "source": "system"},
    "gates": [
      {"gate": "FsGate", "enforced": "kernel", "reason": "bwrap mount namespace", "supported": true},
      {"gate": "NetGate", "enforced": "kernel", "reason": "net namespace", "supported": true}
    ],
    "tools": [
      {"name": "bwrap", "package": "bwrap", "path": "/usr/bin/bwrap", "source": "system"},
      {"name": "hdiutil", "package": "", "install": "", "path": null, "platform": "macos"},
      {"name": "virtiofsd", "package": "virtiofsd", "install": "xlings install virtiofsd", "path": null, "platform": "linux"}
    ]
  },
  "modes": [
    {"name": "sandbox", "ready": true, "how": "isolated here (bwrap)"},
    {"name": "machine", "ready": true, "how": "its own kernel (KVM)"},
    {"name": "desktop", "ready": false, "reason": "luban-os not installed", "route": "xlings install luban-os"}
  ],
  "versions": {
    "xlings": "2026.11.23.1",
    "luban": "2026.11.23.1",
    "luban-os": "",
    "protocol": "1.19",
    "index": "c54a116",
    "running": ["dev-box"]
  }
})json";
        const LubanHostStatus st = ParseLubanStatusJson(status_raw);
        Check(st.ready && st.os == "Linux 7.3.4" && st.arch == "x86_64", "Luban host basic fields parsed");
        Check(st.backend_name == "bwrap" && st.backend_source == "system", "Luban host isolation backend parsed");
        Check(st.sandbox_ready && st.machine_ready && !st.desktop_ready, "Luban host modes parsed");
        Check(st.desktop_route == "xlings install luban-os", "Luban desktop install route preserved");
        Check(st.gates.size() == 2 && st.gates[0].gate == "FsGate" && st.gates[0].supported, "Luban gates parsed");
        Check(st.missing_tools.size() == 1 && st.missing_tools[0].name == "virtiofsd" &&
                  st.missing_tools[0].install_cmd == "xlings install virtiofsd",
              "Luban missing tools excludes cross-OS null-install tools and retains virtiofsd");
        Check(st.luban_version == "2026.11.23.1" && st.protocol_version == "1.19" &&
                  st.running_envs.size() == 1 && st.running_envs[0] == "dev-box",
              "Luban versions and running envs parsed");
    }

    // 2. Parse luban ls --json & luban search --json
    {
        const std::string ls_raw = R"json({
  "environments": [
    {"name": "dev-box", "kind": "rootfs", "edition": "luban:core", "carrier": "local", "shell": "fish", "desktops": ["xdesk"], "packages": 19, "running": true},
    {"name": "my_host_env", "kind": "view", "edition": "", "carrier": "local", "shell": "", "desktops": [], "packages": 0, "running": false}
  ],
  "place": "host"
})json";
        const auto envs = ParseLubanLsJson(ls_raw);
        Check(envs.size() == 2, "ParseLubanLsJson parsed 2 environments");
        Check(envs[0].name == "dev-box" && envs[0].kind == "rootfs" && envs[0].running && envs[0].packages == 19 &&
                  envs[0].desktops.size() == 1 && envs[0].desktops[0] == "xdesk",
              "Luban rootfs running environment fields parsed");
        Check(envs[1].name == "my_host_env" && envs[1].kind == "view" && !envs[1].running,
              "Luban view environment fields parsed");

        const std::string search_raw = R"json({
  "groups": [
    {"group": "distributions", "items": [
      {"name": "core", "ref": "luban:core", "version": "2026.10.24.1", "status": "stable", "source": "official", "available": true, "highlights": ["gcc", "cmake"], "description": "Luban Core"}
    ]},
    {"group": "desktops", "items": [
      {"name": "xdesk", "ref": "luban:xdesk", "version": "2026.10.24.2", "status": "stable", "source": "official", "tier": 5, "available": true, "title": {"en": "xdesk", "zh": "xdesk 桌面"}, "summary": {"en": "WuXian", "zh": "WuXian 无限画布"}, "description": "xdesk desktop"},
      {"name": "kde", "ref": "luban:kde", "version": "6.6.6-r2", "status": "dev", "gpu": "better", "tier": 4, "available": true, "description": "KDE Plasma"}
    ]}
  ]
})json";
        const LubanCatalog cat = ParseLubanSearchJson(search_raw);
        Check(cat.distributions.size() == 1 && cat.distributions[0].ref == "luban:core" &&
                  cat.distributions[0].highlights.size() == 2,
              "ParseLubanSearchJson parsed distributions");
        Check(cat.desktops.size() == 2 && cat.desktops[0].title_zh == "xdesk 桌面" &&
                  cat.desktops[1].gpu_hint == "better",
              "ParseLubanSearchJson parsed desktops and i18n titles");
    }

    // 3. Parse wrapped {"result": ...} env status/config & history/logs
    {
        const std::string env_status_raw = R"json({
  "result": {
    "name": "agent-box",
    "kind": "rootfs",
    "edition": "luban:agent-workspace",
    "carrier": "local",
    "mode": "sandbox",
    "persist": "all",
    "enter": "console",
    "network": "socks5h://127.0.0.1:7897",
    "shell": "bash",
    "desktop": "",
    "desktops": [],
    "needs": ["bwrap"],
    "notes": ["private identity"],
    "unmet": [],
    "packages": 22,
    "running": false
  }
})json";
        const auto est = ParseLubanEnvStatusJson(env_status_raw);
        Check(est.has_value() && est->name == "agent-box" && est->network == "socks5h://127.0.0.1:7897" &&
                  est->needs.size() == 1 && est->notes.size() == 1,
              "ParseLubanEnvStatusJson unwraps result payload");

        const std::string env_cfg_raw = R"json({
  "result": {
    "name": "agent-box",
    "mode": "sandbox",
    "persist": "all",
    "shell": "bash",
    "desktop": "",
    "on_close": "background",
    "files": "virtiofs",
    "kernel": "",
    "isolation": {
      "extends": "xim:agent-private@2026.10.11.1",
      "source": "policy",
      "mounts": ["/home/demo/repo:/work:rw"],
      "observe": {"level": "standard"},
      "permissions": {"fetch": {"default": "ask"}, "index_update": "auto"},
      "isolation": {
        "net": "proxy",
        "proxy": "socks5h://127.0.0.1:7897",
        "no_degrade": true,
        "grants": ["clipboard"],
        "grants_allowed": ["audio", "clipboard", "gpu"],
        "env_pass": ["EDITOR"]
      }
    }
  }
})json";
        const auto ecfg = ParseLubanEnvConfigJson(env_cfg_raw);
        Check(ecfg.has_value() && ecfg->mode == "sandbox" && ecfg->net == "proxy" &&
                  ecfg->proxy == "socks5h://127.0.0.1:7897" && ecfg->observe_level == "standard" &&
                  ecfg->fetch_policy == "ask" && ecfg->no_degrade &&
                  ecfg->grants.size() == 1 && ecfg->mounts.size() == 1,
              "ParseLubanEnvConfigJson unwraps nested isolation configuration");

        const std::string hist_err =
            R"({"event":"error","hint":"xlings use pkg --subos my_host_env","text":"cannot rollback 'my_host_env'"})";
        const std::string hist_fmt = FormatLubanHistoryText(hist_err);
        Check(hist_fmt.find("cannot rollback") != std::string::npos &&
                  hist_fmt.find("提示:") != std::string::npos,
              "FormatLubanHistoryText extracts error and hint");

        Check(FormatLubanLogsText("[]") == "暂无日志记录", "FormatLubanLogsText handles empty array");
        const std::string logs_fmt = FormatLubanLogsText(
            R"([{"timestamp":"2026-11-24 23:00:00","source":"xdesk","text":"started"}])");
        Check(logs_fmt == "[2026-11-24 23:00:00] [xdesk] started", "FormatLubanLogsText formats entries");
    }

    // 4. Argv builders
    {
        LubanCreateOptions co;
        co.name = "devbox";
        co.edition = "core";
        co.desktop = "xdesk";
        co.mode = "machine";
        co.persist = "all";
        co.policy = "dev";
        co.xpkgs = {"xim:llvm", "xim:cmake"};
        co.allows = {"gpu", "clipboard"};
        co.no_password = true;
        co.dry_run = true;
        const auto new_args = BuildLubanNewArgs(co);
        Check(new_args.size() >= 18 && new_args[0] == "new" && new_args[1] == "devbox" &&
                  std::ranges::count(new_args, "--xpkg") == 2 &&
                  std::ranges::count(new_args, "--allow") == 2 &&
                  std::ranges::find(new_args, "--no-password") != new_args.end() &&
                  std::ranges::find(new_args, "--dry-run") != new_args.end() &&
                  new_args.back() == "--yes",
              "BuildLubanNewArgs includes all expected options and flags");

        LubanConfigPatch patch;
        patch.mode = "sandbox";
        patch.proxy = "socks5h://127.0.0.1:7897";
        patch.allow = {"gpu"};
        patch.mounts = {"/a:/b:ro"};
        const auto cfg_args = BuildLubanConfigArgs("devbox", patch);
        Check(cfg_args.size() >= 8 && cfg_args[0] == "config" && cfg_args[1] == "devbox" &&
                  std::ranges::find(cfg_args, "--mode") != cfg_args.end() &&
                  std::ranges::find(cfg_args, "--proxy") != cfg_args.end() &&
                  std::ranges::find(cfg_args, "--allow") != cfg_args.end() &&
                  std::ranges::find(cfg_args, "--mount") != cfg_args.end(),
              "BuildLubanConfigArgs emits non-empty patch flags");

        const auto exp_args = BuildLubanExportArgs({
            .name = "devbox",
            .file_path = "/tmp/devbox.qcow2",
            .format = "qcow2",
            .size = "8G",
            .boot = "virt",
        });
        Check(exp_args[0] == "export" && exp_args[1] == "devbox" && exp_args[2] == "/tmp/devbox.qcow2" &&
                  std::ranges::find(exp_args, "--format") != exp_args.end() &&
                  std::ranges::find(exp_args, "--size") != exp_args.end() &&
                  std::ranges::find(exp_args, "--boot") != exp_args.end(),
              "BuildLubanExportArgs constructs export invocation");
    }

    // 5. Declarative stack JSON & TOML parsing, validation & round-trip
    {
        const std::string toml_doc = R"toml(
# Comment line
schema = "hxlings.luban-stack/v1"
name = "team-stack" # inline comment
description = "Demo # sharp in quotes"

[[environments]]
name = "cpp-box"
edition = 'core'
mode = "sandbox"
persist = "all"
policy = "dev"
xpkgs = [
  "xim:llvm@22.1.8",
  "xim:cmake@4.5.0", # trailing comment
]
allows = ["clipboard"]
mounts = ["~/dev:/work:rw"]
no_password = true

[[environments]]
name = "agent-box"
edition = "agent-workspace"
policy = "xim:agent-private"
proxy = "socks5h://127.0.0.1:7897"
fetch = "ask"
observe = "standard"
)toml";
        const LubanStackParseResult parsed = ParseLubanStackAuto(toml_doc);
        Check(parsed.ok && parsed.detected_format == "toml" && parsed.spec.name == "team-stack" &&
                  parsed.spec.description == "Demo # sharp in quotes" && parsed.spec.environments.size() == 2,
              "ParseLubanStackAuto parses TOML with comments and multi-line arrays");
        Check(parsed.spec.environments[0].xpkgs.size() == 2 && parsed.spec.environments[0].no_password &&
                  parsed.spec.environments[1].proxy == "socks5h://127.0.0.1:7897",
              "TOML environment fields match expected values");

        const std::string json_rt = SerializeLubanStackJson(parsed.spec);
        const LubanStackParseResult reparsed_json = ParseLubanStackAuto(json_rt);
        Check(reparsed_json.ok && reparsed_json.detected_format == "json" && reparsed_json.spec == parsed.spec,
              "LubanStack JSON serialize -> parse round-trip preserves spec");

        const std::string toml_rt = SerializeLubanStackToml(parsed.spec);
        const LubanStackParseResult reparsed_toml = ParseLubanStackAuto(toml_rt);
        Check(reparsed_toml.ok && reparsed_toml.detected_format == "toml" && reparsed_toml.spec == parsed.spec,
              "LubanStack TOML serialize -> parse round-trip preserves spec");

        // Validation failure checks
        const auto bad_reserved = ParseLubanStackJson(R"({"name":"bad","environments":[{"name":"default"}]})");
        Check(!bad_reserved.ok && !bad_reserved.errors.empty(), "Rejects reserved environment name 'default'");

        const auto bad_dup = ParseLubanStackJson(
            R"({"name":"bad","environments":[{"name":"dup"},{"name":"dup"}]})");
        Check(!bad_dup.ok, "Rejects duplicate environment names");

        const auto bad_mode = ParseLubanStackJson(
            R"({"name":"bad","environments":[{"name":"box1","mode":"docker"}]})");
        Check(!bad_mode.ok, "Rejects invalid mode value");

        const auto bad_proxy = ParseLubanStackJson(
            R"({"name":"bad","environments":[{"name":"agent1","edition":"agent-workspace","policy":"xim:agent-private"}]})");
        Check(!bad_proxy.ok, "Rejects agent-workspace / agent-private without proxy");
    }

    // 6. Diff plan, export & builtin blueprints
    {
        LubanStackSpec spec;
        spec.name = "plan-test";
        spec.environments = {
            LubanStackEnvSpec{.name = "env-create", .edition = "core", .mode = "sandbox"},
            LubanStackEnvSpec{.name = "env-match", .edition = "core", .mode = "sandbox", .persist = "all", .policy = "dev"},
            LubanStackEnvSpec{
                .name = "env-drift",
                .edition = "core",
                .mode = "machine",
                .persist = "home",
                .allows = {"gpu"},
            },
        };
        const std::vector<LubanEnvironment> existing{
            LubanEnvironment{.name = "default", .kind = "view"},
            LubanEnvironment{.name = "env-match", .kind = "rootfs", .edition = "luban:core"},
            LubanEnvironment{.name = "env-drift", .kind = "rootfs", .edition = "luban:core"},
        };
        const std::map<std::string, LubanEnvConfigDetail> cfgs{
            {"env-match", LubanEnvConfigDetail{.name = "env-match", .mode = "sandbox", .persist = "all", .policy_extends = "dev"}},
            {"env-drift", LubanEnvConfigDetail{.name = "env-drift", .mode = "sandbox", .persist = "all", .grants = {}}},
        };

        const LubanStackPlan plan = BuildLubanStackPlan(spec, existing, cfgs);
        Check(plan.create_count == 1 && plan.unchanged_count == 1 && plan.configure_count == 1 &&
                  plan.actions.size() == 3,
              "BuildLubanStackPlan produces 1 Create, 1 UpToDate, 1 Configure");
        Check(plan.actions[2].kind == LubanStackActionKind::Configure &&
                  plan.actions[2].config_patch.mode == std::optional<std::string>("machine") &&
                  plan.actions[2].config_patch.persist == std::optional<std::string>("home") &&
                  plan.actions[2].config_patch.allow == std::vector<std::string>{"gpu"},
              "BuildLubanStackPlan computes minimal config diff patch");

        const LubanStackSpec exported = ExportEnvironmentsToStack("snap", existing, cfgs);
        Check(exported.environments.size() == 2 && exported.environments[0].name == "env-match",
              "ExportEnvironmentsToStack omits reserved 'default' environment");

        const auto builtins = BuiltinLubanStacks();
        Check(builtins.size() == 3, "BuiltinLubanStacks provides 3 blueprints");
        for (const auto& b : builtins) {
            Check(ValidateLubanStackSpec(b).empty(), "Builtin stack '" + b.name + "' validates cleanly");
        }
    }

    // 7. Blueprint file persistence in isolated temp dir
    {
        std::error_code ec;
        std::filesystem::path temp_base = mcpplibs::os::dirs::temp();
        if (temp_base.empty()) temp_base = std::filesystem::temp_directory_path(ec);
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto stack_dir = temp_base / ("hxl-luban-stacks-" + std::to_string(stamp));
        std::filesystem::remove_all(stack_dir, ec);

        const auto builtins = BuiltinLubanStacks();
        const std::string toml_text = SerializeLubanStackToml(builtins.front());
        const auto saved_path = SaveLubanStackFile("My Demo Stack", toml_text, "toml", stack_dir);
        Check(saved_path.has_value() && std::filesystem::exists(*saved_path, ec),
              "SaveLubanStackFile saves TOML blueprint atomically");

        const auto listed = ListSavedLubanStacks(stack_dir);
        Check(listed.size() == 1 && listed[0].format == "toml" && listed[0].env_count == 2,
              "ListSavedLubanStacks lists saved TOML blueprint");

        const auto loaded = LoadLubanStackFile(*saved_path);
        Check(loaded.has_value() && ParseLubanStackAuto(*loaded).ok,
              "LoadLubanStackFile reads saved blueprint back cleanly");

        Check(DeleteLubanStackFile(*saved_path) && ListSavedLubanStacks(stack_dir).empty(),
              "DeleteLubanStackFile deletes saved blueprint");
        std::filesystem::remove_all(stack_dir, ec);
    }
}

int main() {
    TestProcessStreamingAndCancel();
    TestProcessStreamingExceptionSafety();
    TestAppSettingsSerializationAndPersistence();
    TestBundledResourcesAndChildEnvSanitization();
    TestWindowsProcessHelpersAndSpawn();
    TestExternalUrlValidationAndNormalization();
    TestAppUpdaterParsingSha256AndSelection();
    TestSingleInstanceGuard();
    TestLubanParsersAndStackEngine();

    if (failures == 0) {
        std::println("All runtime and Luban integration tests passed.");
        return 0;
    }
    std::println(std::cerr, "{} test(s) failed.", failures);
    return 1;
}

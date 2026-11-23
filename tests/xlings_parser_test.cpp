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

using namespace hxlings;

namespace {

int failures = 0;

void Check(bool cond, std::string_view msg) {
    if (!cond) {
        std::println(std::cerr, "FAIL: {}", msg);
        ++failures;
    }
}

void TestSplitPackageSpec() {
    {
        const auto p = SplitPackageSpec("xim:mcpp@2026.11.21.1");
        Check(p.ns == "xim" && p.name == "mcpp" && p.version == "2026.11.21.1", "SplitPackageSpec xim:mcpp@ver");
    }
    {
        const auto p = SplitPackageSpec("local:node@26.13.0");
        Check(p.ns == "local" && p.name == "node" && p.version == "26.13.0", "SplitPackageSpec local:node@ver");
    }
    {
        const auto p = SplitPackageSpec("xim:gcc");
        Check(p.ns == "xim" && p.name == "gcc" && p.version.empty(), "SplitPackageSpec xim:gcc");
    }
    {
        const auto p = SplitPackageSpec("gcc@15");
        Check(p.ns.empty() && p.name == "gcc" && p.version == "15", "SplitPackageSpec gcc@15");
    }
    {
        const auto p = SplitPackageSpec("node");
        Check(p.ns.empty() && p.name == "node" && p.version.empty(), "SplitPackageSpec node");
    }
}

void TestNdjsonAndEnv() {
    const std::string sample =
        "{\"dataKind\":\"env\",\"kind\":\"data\",\"payload\":{\"activeSubos\":\"default\","
        "\"binDir\":\"/home/u/.xlings/subos/default/bin\",\"dataDir\":\"/home/u/.xlings/data\","
        "\"lang\":\"zh\",\"libDir\":\"/home/u/.xlings/subos/default/lib\",\"mirror\":\"CN\","
        "\"subosDir\":\"/home/u/.xlings/subos/default\",\"xlingsHome\":\"/home/u/.xlings\"}}\n"
        "{\"exitCode\":0,\"kind\":\"result\"}\n";
    const auto events = ParseNdjsonLines(sample);
    Check(events.size() == 2, "ParseNdjsonLines count");
    const XlingsEnv env = ParseEnvEvents(events);
    Check(env.active_subos == "default", "ParseEnvEvents activeSubos");
    Check(env.mirror == "CN", "ParseEnvEvents mirror");
    Check(env.xlings_home == "/home/u/.xlings", "ParseEnvEvents xlingsHome");
}

void TestSystemStatusAndBindings() {
    const std::string sample =
        "{\"dataKind\":\"system_info\",\"kind\":\"data\",\"payload\":{\"fields\":["
        "{\"highlight\":true,\"label\":\"active subos\",\"value\":\"default\"}],"
        "\"title\":\"xlings status\"}}\n"
        "{\"dataKind\":\"binding_state\",\"kind\":\"data\",\"payload\":{\"findings\":[{"
        "\"code\":\"xvm-active-group-incoherent\","
        "\"hint\":\"run `xlings use libz.so 1.3.1`\","
        "\"severity\":\"broken\","
        "\"subos\":\"default\","
        "\"summary\":\"libz.so incoherent\","
        "\"target\":\"zlib.files.3\","
        "\"version\":\"1.3.1\"}]}}\n";
    const auto st = ParseSystemStatusEvents(ParseNdjsonLines(sample));
    Check(st.fields.size() == 1 && st.fields[0].highlight, "ParseSystemStatusEvents fields");
    Check(st.findings.size() == 1 && st.findings[0].target == "zlib.files.3", "ParseSystemStatusEvents findings");
}

void TestInstalledAndGrouping() {
    const std::string sample =
        "{\"dataKind\":\"styled_list\",\"kind\":\"data\",\"payload\":{\"items\":["
        "[\"local:node@26.13.0\",\"\",\"degraded: index entry unavailable\"],"
        "[\"xim:mcpp@2026.11.21.1\",\"build tool\",\"\"],"
        "[\"xim:mcpp@2026.11.18.1\",\"build tool\",\"inactive\"],"
        "[\"xim:gcc@16.1.0\",\"GNU compiler\",\"inactive\"]]}}\n";
    const auto raw = ParseInstalledPackagesEvents(ParseNdjsonLines(sample));
    Check(raw.size() == 4, "ParseInstalledPackagesEvents count");
    Check(raw[0].is_degraded && !raw[0].is_active, "node degraded flags");
    Check(raw[1].is_active && !raw[1].is_degraded, "mcpp active flags");

    const auto groups = GroupInstalledPackages(raw);
    Check(groups.size() == 3, "GroupInstalledPackages count");
    // Sorted alphabetically by name: gcc, mcpp, node
    Check(groups[0].name == "gcc" && groups[0].active_version.empty(), "Group 0 gcc");
    Check(groups[1].name == "mcpp" && groups[1].versions.size() == 2 && groups[1].active_version == "2026.11.21.1",
          "Group 1 mcpp active first");
    Check(groups[2].name == "node" && groups[2].has_degraded, "Group 2 node degraded");
}

void TestSearchAndInfoAndPlan() {
    const std::string search_raw =
        "{\"dataKind\":\"packages\",\"kind\":\"data\",\"payload\":[{"
        "\"archs\":[\"x86_64\"],\"categories\":[\"compiler\"],\"description\":\"GCC\","
        "\"index\":\"xim\",\"keywords\":[\"c++\"],\"name\":\"gcc\",\"namespace\":\"xim\","
        "\"package\":\"xim:gcc\",\"status\":\"stable\",\"version\":\"16.1.0\"}]}\n";
    const auto search = ParseSearchPackagesEvents(ParseNdjsonLines(search_raw));
    Check(search.size() == 1 && search[0].package == "xim:gcc" && search[0].version == "16.1.0",
          "ParseSearchPackagesEvents");

    const std::string info_raw =
        "{\"dataKind\":\"info_panel\",\"kind\":\"data\",\"payload\":{"
        "\"fields\":[{\"highlight\":false,\"label\":\"status\",\"value\":\"stable\"}],"
        "\"package\":{\"archs\":[\"x86_64\"],\"categories\":[\"compiler\"],\"description\":\"GCC\","
        "\"name\":\"gcc\",\"namespace\":\"xim\",\"package\":\"xim:gcc\",\"status\":\"stable\","
        "\"version\":\"16.1.0\",\"versions\":[\"16.2.0\",\"16.1.0\"]},\"title\":\"xim:gcc\"}}\n";
    const auto detail = ParsePackageInfoEvents(ParseNdjsonLines(info_raw));
    Check(detail.package == "xim:gcc" && detail.available_versions.size() == 2, "ParsePackageInfoEvents");

    const std::string plan_raw =
        "{\"dataKind\":\"install_plan\",\"kind\":\"data\",\"payload\":{\"packages\":["
        "[\"xim:binutils@2.47\",\"\",0],[\"xim:gcc@16.1.0\",\"new\",1]]}}\n";
    const auto plan = ParseInstallPlanEvents(ParseNdjsonLines(plan_raw));
    Check(plan.size() == 2 && plan[1].action_code == 1 && plan[1].spec == "xim:gcc@16.1.0",
          "ParseInstallPlanEvents");
}

void TestSubosReposIndexesAndCli() {
    const std::string subos_raw =
        "{\"dataKind\":\"subos_list\",\"kind\":\"data\",\"payload\":{\"entries\":[{"
        "\"abi\":\"native\",\"active\":true,\"carrier\":\"local\",\"commands\":118,"
        "\"dir\":\"/home/u/.xlings/subos/default\",\"host\":false,\"kind\":\"view\","
        "\"name\":\"default\",\"packages\":114,\"view\":\"overlay\"}]}}\n";
    const auto subs = ParseSubosListEvents(ParseNdjsonLines(subos_raw));
    Check(subs.size() == 1 && subs[0].active && subs[0].commands == 118, "ParseSubosListEvents");

    const std::string repo_raw =
        "{\"dataKind\":\"repo_list\",\"kind\":\"data\",\"payload\":{\"repos\":[{"
        "\"name\":\"xim\",\"scope\":\"global\",\"url\":\"https://example.test/xim.git\"}]}}\n";
    const auto repos = ParseRepoListEvents(ParseNdjsonLines(repo_raw));
    Check(repos.size() == 1 && repos[0].name == "xim", "ParseRepoListEvents");

    const std::string idx_raw =
        "{\"dataKind\":\"index_versions\",\"kind\":\"data\",\"payload\":[{"
        "\"current\":\"bafdb27\",\"git_managed\":false,\"installed\":\"bafdb27\",\"name\":\"xim\",\"pinned\":\"\","
        "\"snapshots\":[{\"artifact\":{\"name\":\"xim-bafdb27.tar.gz\",\"size\":1957663},"
        "\"current\":true,\"generated_at\":\"2026-11-22T07:36:45Z\",\"index_version\":\"bafdb27\",\"installed\":true}]}]}\n";
    const auto idx = ParseIndexVersionsEvents(ParseNdjsonLines(idx_raw));
    Check(idx.size() == 1 && idx[0].snapshots.size() == 1 && idx[0].snapshots[0].size_bytes == 1957663ULL,
          "ParseIndexVersionsEvents");

    const std::string cfg_zh =
        "xlings config\n"
        "  XLINGS_HOME: /home/u/.xlings\n"
        "  镜像: CN\n"
        "  语言: auto (zh)\n"
        "  界面模式: cli\n"
        "  交互模式: false (默认)\n";
    const auto cfg = ParseConfigText(cfg_zh);
    Check(cfg.xlings_home == "/home/u/.xlings" && cfg.mirror == "CN" && cfg.ui_mode == "cli",
          "ParseConfigText ZH");

    const std::string xpkg_txt =
        "name        version   status   source\n"
        "winscp      6.7.1     unique   /tmp/winscp.lua\n";
    const auto xpkgs = ParseLocalXpkgTable(xpkg_txt);
    Check(xpkgs.size() == 1 && xpkgs[0].name == "winscp" && xpkgs[0].version == "6.7.1",
          "ParseLocalXpkgTable");

    const std::string log_txt =
        "{\"command\":\"xlings self doctor\",\"exit\":0,\"started\":\"2026-11-22T07:50:47Z\",\"trace\":\"abc\"}\n";
    const auto logs = ParseSelfLogLines(log_txt);
    Check(logs.size() == 1 && logs[0].trace == "abc" && logs[0].exit_code == 0, "ParseSelfLogLines");
}

void TestLiveXlingsBinaryAndEnv() {
    auto in_mcpp_registry = [](std::string_view s) {
        return s.find("/.mcpp/registry") != std::string_view::npos ||
               s.find("\\.mcpp\\registry") != std::string_view::npos;
    };

    const std::string bin = ResolveXlingsBinary();
    Check(!bin.empty(), "ResolveXlingsBinary non-empty");
    Check(!in_mcpp_registry(bin), "ResolveXlingsBinary skips mcpp registry sandbox: " + bin);

    const ProcessResult res = RunProcessStreaming({bin, "interface", "env"});
    Check(res.exit_code == 0, "Live `xlings interface env` exit 0");
    const XlingsEnv env = ParseEnvEvents(ParseNdjsonLines(res.raw_output));
    Check(!env.xlings_home.empty(), "Live xlings_home non-empty");
    Check(!in_mcpp_registry(env.xlings_home), "Live xlings_home is user home, not mcpp sandbox: " + env.xlings_home);
}

void TestVersionCompareAndMultiActiveGrouping() {
    Check(CompareVersionStrings("2026.11.21.1", "2026.9.30.1") > 0, "CompareVersionStrings 2026.11.21.1 > 2026.9.30.1");
    Check(CompareVersionStrings("26.13.0", "24.20.0") > 0, "CompareVersionStrings 26.13.0 > 24.20.0");
    Check(CompareVersionStrings("4.5.0", "4.4.2") > 0, "CompareVersionStrings 4.5.0 > 4.4.2");
    Check(CompareVersionStrings("1.2.1", "1.2") > 0, "CompareVersionStrings 1.2.1 > 1.2");
    Check(CompareVersionStrings("2026.11.21.1", "2026.11.21.1") == 0, "CompareVersionStrings equal");

    const std::string sample =
        "{\"dataKind\":\"styled_list\",\"kind\":\"data\",\"payload\":{\"items\":["
        "[\"xim:cmake@4.4.2\",\"CMake\",\"\"],"
        "[\"xim:cmake@4.5.0\",\"CMake\",\"\"],"
        "[\"xim:mcpp@2026.9.30.1\",\"mcpp\",\"inactive\"],"
        "[\"xim:mcpp@2026.11.18.1\",\"mcpp\",\"inactive\"],"
        "[\"xim:mcpp@2026.11.21.1\",\"mcpp\",\"\"]]}}\n";
    const auto groups = GroupInstalledPackages(ParseInstalledPackagesEvents(ParseNdjsonLines(sample)));
    Check(groups.size() == 2, "MultiActiveGrouping group count");
    Check(groups[0].name == "cmake" && groups[0].active_version == "4.5.0",
          "MultiActiveGrouping picks highest active version 4.5.0");
    Check(groups[1].name == "mcpp" && groups[1].versions.size() == 3 &&
              groups[1].versions[0].version == "2026.11.21.1" &&
              groups[1].versions[1].version == "2026.11.18.1" &&
              groups[1].versions[2].version == "2026.9.30.1",
          "MultiActiveGrouping orders versions newest-first semantically");
}

void TestParseRemovePlanEvents() {
    const std::string xlings_plan_raw =
        "{\"dataKind\":\"remove_plan\",\"kind\":\"data\",\"payload\":{\"bytes\":1099397952,"
        "\"dry_run\":true,\"keep\":[],\"name\":\"xlings\",\"older_than\":\"2026.11.22.1\","
        "\"remove\":[{\"bytes\":49518230,\"version\":\"2026.11.16.1\"},"
        "{\"bytes\":49093304,\"version\":\"2026.10.26.1\"}]}}\n"
        "{\"exitCode\":0,\"kind\":\"result\"}\n";
    const auto p1 = ParseRemovePlanEvents(ParseNdjsonLines(xlings_plan_raw));
    Check(p1.has_value() && p1->name == "xlings" && p1->older_than == "2026.11.22.1" &&
              p1->total_bytes == 1099397952ULL && p1->remove.size() == 2 &&
              p1->remove[0].version == "2026.11.16.1" && p1->remove[0].bytes == 49518230ULL &&
              p1->keep.empty(),
          "ParseRemovePlanEvents xlings plan");

    const std::string glibc_plan_raw =
        "{\"dataKind\":\"remove_plan\",\"kind\":\"data\",\"payload\":{\"bytes\":0,"
        "\"dry_run\":true,\"keep\":[{\"reason\":\"linked by xim:gcc-runtime@15.1.0\",\"version\":\"2.39\"}],"
        "\"name\":\"glibc\",\"older_than\":\"2.44.2\",\"remove\":[]}}\n";
    const auto p2 = ParseRemovePlanEvents(ParseNdjsonLines(glibc_plan_raw));
    Check(p2.has_value() && p2->name == "glibc" && p2->total_bytes == 0ULL &&
              p2->remove.empty() && p2->keep.size() == 1 &&
              p2->keep[0].version == "2.39" &&
              p2->keep[0].reason == "linked by xim:gcc-runtime@15.1.0",
          "ParseRemovePlanEvents glibc kept plan");

    const auto empty_plan = ParseRemovePlanEvents(ParseNdjsonLines("{\"exitCode\":0,\"kind\":\"result\"}\n"));
    Check(!empty_plan.has_value(), "ParseRemovePlanEvents returns nullopt when absent");
}

void TestParsePayloadStatusAndUpdatablePlans() {
    const std::string status_raw =
        "{\"dataKind\":\"payload_status\",\"kind\":\"data\",\"payload\":{\"targets\":["
        "{\"error\":\"package 'local:node' not found in the synced index\\ndetail line\",\"request\":\"local:node\"},"
        "{\"complete\":true,\"current\":false,\"installed_revision\":0,\"name\":\"bwrap\",\"namespace\":\"xim\",\"origin\":\"local\",\"payload_dir\":\"/tmp/bwrap\",\"request\":\"xim:bwrap\",\"revision\":1,\"usable\":true,\"version\":\"0.11.2\"},"
        "{\"complete\":true,\"current\":true,\"installed_revision\":0,\"name\":\"cmake\",\"namespace\":\"xim\",\"origin\":\"local\",\"payload_dir\":\"/tmp/cmake\",\"request\":\"xim:cmake\",\"revision\":0,\"usable\":true,\"version\":\"4.5.0\"},"
        "{\"complete\":false,\"current\":false,\"installed_revision\":null,\"name\":\"pnpm\",\"namespace\":\"xim\",\"origin\":\"absent\",\"payload_dir\":\"/tmp/pnpm\",\"request\":\"xim:pnpm\",\"revision\":0,\"usable\":false,\"version\":\"12.22.0\"},"
        "{\"complete\":false,\"current\":false,\"installed_revision\":null,\"name\":\"huxerui\",\"namespace\":\"xim\",\"origin\":\"absent\",\"payload_dir\":\"/tmp/huxerui\",\"request\":\"xim:huxerui\",\"revision\":0,\"usable\":false,\"version\":\"0.5.0\"}"
        "]}}\n"
        "{\"exitCode\":1,\"kind\":\"result\"}\n";

    const auto statuses = ParsePayloadStatusEvents(ParseNdjsonLines(status_raw));
    Check(statuses.size() == 5, "ParsePayloadStatusEvents parses all targets including error entry");
    Check(!statuses[0].error.empty() && statuses[0].request == "local:node" && !statuses[0].installed_revision.has_value(),
          "ParsePayloadStatusEvents captures unindexed error item");
    Check(statuses[1].request == "xim:bwrap" && statuses[1].installed_revision == std::optional<int>(0) &&
              statuses[1].revision == 1 && !statuses[1].current,
          "ParsePayloadStatusEvents captures revision delta");
    Check(statuses[3].request == "xim:pnpm" && !statuses[3].installed_revision.has_value() &&
              statuses[3].version == "12.22.0",
          "ParsePayloadStatusEvents handles null installed_revision");

    const std::vector<PkgGroup> groups{
        PkgGroup{
            .key = "local:node",
            .ns = "local",
            .name = "node",
            .active_version = "",
            .versions = {InstalledPkg{.raw_spec = "local:node@26.13.0", .ns = "local", .name = "node", .version = "26.13.0", .is_active = false, .is_degraded = true}},
        },
        PkgGroup{
            .key = "xim:bwrap",
            .ns = "xim",
            .name = "bwrap",
            .active_version = "",
            .versions = {InstalledPkg{.raw_spec = "xim:bwrap@0.11.2", .ns = "xim", .name = "bwrap", .version = "0.11.2", .is_active = false}},
        },
        PkgGroup{
            .key = "xim:cmake",
            .ns = "xim",
            .name = "cmake",
            .active_version = "4.5.0",
            .versions = {InstalledPkg{.raw_spec = "xim:cmake@4.5.0", .ns = "xim", .name = "cmake", .version = "4.5.0", .is_active = true}},
        },
        PkgGroup{
            .key = "xim:pnpm",
            .ns = "xim",
            .name = "pnpm",
            .active_version = "12.0.0",
            .versions = {InstalledPkg{.raw_spec = "xim:pnpm@12.0.0", .ns = "xim", .name = "pnpm", .version = "12.0.0", .is_active = true}},
        },
        PkgGroup{
            .key = "xim:huxerui",
            .ns = "xim",
            .name = "huxerui",
            .active_version = "0.3.0",
            .versions = {InstalledPkg{.raw_spec = "xim:huxerui@0.3.0", .ns = "xim", .name = "huxerui", .version = "0.3.0", .is_active = true}},
        },
    };

    const auto plans = BuildUpdatablePlans(groups, statuses);
    Check(plans.size() == 4, "BuildUpdatablePlans excludes current xim:cmake");
    Check(plans[0].key == "xim:huxerui" && !plans[0].is_revision_only && !plans[0].is_skipped &&
              plans[0].current_version == "0.3.0" && plans[0].latest_version == "0.5.0",
          "BuildUpdatablePlans orders xim:huxerui new version first");
    Check(plans[1].key == "xim:pnpm" && !plans[1].is_revision_only && !plans[1].is_skipped &&
              plans[1].current_version == "12.0.0" && plans[1].latest_version == "12.22.0",
          "BuildUpdatablePlans orders xim:pnpm new version second");
    Check(plans[2].key == "xim:bwrap" && plans[2].is_revision_only && !plans[2].is_skipped &&
              plans[2].installed_revision == 0 && plans[2].latest_revision == 1 && plans[2].latest_version == "0.11.2",
          "BuildUpdatablePlans orders xim:bwrap revision update third");
    Check(plans[3].key == "local:node" && plans[3].is_skipped &&
              plans[3].skip_reason == "package 'local:node' not found in the synced index",
          "BuildUpdatablePlans places skipped unindexed error last with first-line reason");
}

void TestWhitespaceAndSearchNormalization() {
    Check(TrimWhitespace("  \t gcc \r\n ") == "gcc", "TrimWhitespace trims tabs/spaces/newlines");
    Check(TrimWhitespace("   ").empty(), "TrimWhitespace empty when blank");

    const std::string multiline_raw =
        "{\"dataKind\":\"packages\",\"kind\":\"data\",\"payload\":[{"
        "\"archs\":[],\"categories\":[],\"description\":\"   Line one\\n    line two\\t\\tline three   \","
        "\"index\":\"awesome\",\"keywords\":[],\"name\":\"awesome\",\"namespace\":\"awesome\","
        "\"package\":\"awesome:awesome\",\"status\":\"dev\",\"version\":\"latest\"}]}\n";
    const auto pkgs = ParseSearchPackagesEvents(ParseNdjsonLines(multiline_raw));
    Check(pkgs.size() == 1 && pkgs[0].description == "Line one line two line three",
          "ParseSearchPackagesEvents normalizes multiline descriptions");

    const std::string info_raw =
        "{\"dataKind\":\"info_panel\",\"kind\":\"data\",\"payload\":{"
        "\"package\":{\"description\":\"  First\\r\\n  second  \",\"name\":\"awesome\",\"namespace\":\"awesome\",\"package\":\"awesome:awesome\"},"
        "\"title\":\"awesome:awesome\"}}\n";
    const auto detail = ParsePackageInfoEvents(ParseNdjsonLines(info_raw));
    Check(detail.description == "First second", "ParsePackageInfoEvents normalizes multiline description");
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

void TestConfigExtensionsAndNetworkParsers() {
    const std::string cfg_txt = R"(xlings config
  XLINGS_HOME: /tmp/xhome
  XLINGS_DATA: /tmp/xhome/data
  XLINGS_SUBOS: /tmp/xhome/subos/default
  当前子系统: default
  镜像: CN
  主题: tokyo-night
  默认运行时: glibc@2.44.3
  归档目录: /tmp/arc1, /tmp/arc2
)";
    const auto cfg = ParseConfigText(cfg_txt);
    Check(cfg.theme == "tokyo-night", "config theme parsed");
    Check(cfg.default_runtime == "glibc@2.44.3", "config default runtime parsed");
    Check(cfg.archive_dirs.size() == 2 && cfg.archive_dirs[0] == "/tmp/arc1" && cfg.archive_dirs[1] == "/tmp/arc2",
          "config archive dirs parsed");

    const std::string theme_txt = R"(themes:
  default            (built in)
  tokyo-night        @xlings/config/themes/tokyo-night.json
  sakura             @xlings/config/themes/sakura.json

  active: tokyo-night
  to customise: copy a file
)";
    const auto themes = ParseConfigThemeList(theme_txt);
    Check(themes.size() == 3, "3 themes parsed");
    Check(!themes[0].active && themes[1].active && themes[1].name == "tokyo-night", "active theme marked");

    const auto ep_evs = ParseNdjsonLines(
        R"({"dataKind":"network_endpoints","kind":"data","payload":{"endpoints":[{"region":"GLOBAL","resource":"xim","url":"https://raw.githubusercontent.com/openxlings/xim-pkgindex/main"},{"region":"CN","resource":"xim","url":"https://raw.gitcode.com/sunrisepeak/xim-pkgindex/raw/main"}]}})"
    );
    const auto eps = ParseNetworkEndpointsEvents(ep_evs);
    Check(eps.size() == 2 && eps[0].region == "GLOBAL" && eps[1].region == "CN", "network endpoints parsed");

    const auto probe_evs = ParseNdjsonLines(
        R"({"dataKind":"network_probe","kind":"data","payload":{"elapsed_seconds":0.215,"host":"raw.gitcode.com","http_status":200,"proxy":false,"reachable":true,"status":"reachable"}})"
    );
    const auto probe = ParseNetworkProbeEvents(probe_evs, "https://raw.gitcode.com/test");
    Check(probe.ok && probe.reachable && probe.http_status == 200 && probe.host == "raw.gitcode.com",
          "network probe parsed");

    const auto timeout_evs = ParseNdjsonLines(
        R"({"dataKind":"network_probe","kind":"data","payload":{"elapsed_seconds":3.01,"host":"example.com","http_status":0,"proxy":false,"reachable":false,"status":"timeout"}})"
    );
    const auto timeout_probe = ParseNetworkProbeEvents(timeout_evs, "https://example.com");
    Check(timeout_probe.ok && !timeout_probe.reachable && timeout_probe.status == "timeout",
          "network probe timeout payload parsed");

    const auto err_evs = ParseNdjsonLines(
        R"({"code":"E_INTERNAL","kind":"error","message":"probe failed"})"
    );
    const auto err_probe = ParseNetworkProbeEvents(err_evs, "https://example.com");
    Check(!err_probe.ok && err_probe.status == "error" && err_probe.error_message == "probe failed",
          "network probe error event parsed");

    const auto default_probe_args = BuildProbeUrlArgs("https://github.com/FarnaHerry/Hxlings");
    Check(default_probe_args["url"] == "https://github.com/FarnaHerry/Hxlings" &&
              !default_probe_args.contains("socket_timeout_ms"),
          "BuildProbeUrlArgs omits socket_timeout_ms by default");
    const auto clamped_high_args = BuildProbeUrlArgs("https://example.com", 5000);
    Check(clamped_high_args["socket_timeout_ms"] == 3000, "BuildProbeUrlArgs clamps >3000 to 3000");
    const auto clamped_low_args = BuildProbeUrlArgs("https://example.com", 50);
    Check(clamped_low_args["socket_timeout_ms"] == 100, "BuildProbeUrlArgs clamps <100 to 100");
    const auto valid_probe_args = BuildProbeUrlArgs("https://example.com", 2500);
    Check(valid_probe_args["socket_timeout_ms"] == 2500, "BuildProbeUrlArgs preserves 2500");

    const XlingsClient live_client;
    const auto live_default = live_client.ProbeUrl("http://127.0.0.1:1");
    Check(live_default.ok && live_default.error_message.empty() && !live_default.reachable,
          "Live offline ProbeUrl omits socket_timeout_ms without CLI exit 2");
    const auto live_clamped = live_client.ProbeUrl("http://127.0.0.1:1", 5000);
    Check(live_clamped.ok && live_clamped.error_message.empty() && !live_clamped.reachable,
          "Live offline ProbeUrl clamps 5000ms without CLI exit 2");
}

void TestSubosGovernanceAndFormatParsers() {
    const std::string status_json = R"({
  "instance": "dev",
  "policy_source": "@xlings/config/policies/dev.json",
  "effective": {
    "enters": true,
    "spec": {"backend": "bwrap"},
    "gates": [{"gate": "userns", "enforced": true, "supported": true, "route": "unprivileged", "reason": "ok"}]
  },
  "requested": {
    "extends": "dev",
    "isolation": {"disable_userns": false, "net": "nat", "proxy": "socks5h://127.0.0.1:1080", "no_degrade": true, "env_pass": ["TERM"]},
    "observe": {"level": "standard"},
    "permissions": {"fetch": {"default": "ask"}, "index_update": "auto"}
  }
})";
    const auto st = ParseSubosStatusJson(status_json);
    Check(st.instance == "dev" && st.enters && st.backend == "bwrap", "subos status parsed");
    Check(st.gates.size() == 1 && st.gates[0].enforced && st.gates[0].gate == "userns", "subos gates parsed");
    Check(st.requested.net == "nat" && st.requested.no_degrade && st.requested.index_update == "auto",
          "subos requested policy parsed");

    const std::string cfg_json = R"({"extends":"private","source":"inline","isolation":{"net":"none","grants":["display"],"env_pass":["HOME"]},"mounts":["/tmp:/tmp:ro"],"observe":{"level":"full"},"permissions":{"fetch":{"default":"deny"},"index_update":"deny"}})";
    const auto pol = ParseSubosConfigJson(cfg_json);
    Check(pol.extends == "private" && pol.net == "none" && pol.mounts.size() == 1 && pol.observe_level == "full" &&
              pol.index_update == "deny",
          "subos config json parsed");

    const std::string doc_json = R"({"errors":1,"gates":[],"instances":[{"instance":"dev","findings":[{"check":"sandbox","level":"error","detail":"bwrap missing","fix":"xlings install bwrap"}]}]})";
    const auto doc = ParseSubosDoctorJson(doc_json);
    Check(doc.errors == 1 && doc.findings.size() == 1 && doc.findings[0].check == "sandbox", "subos doctor parsed");

    const auto ps = ParseSubosPsJson(R"({"id":"s-1","instance":"dev","backend":"bwrap","pid":1234,"state":"running","started":"2026-11-23T00:00:00Z","command":["bash"]})");
    Check(ps.size() == 1 && ps[0].id == "s-1" && ps[0].pid == 1234 && ps[0].command == "bash", "subos ps parsed");
    Check(ParseSubosPsJson("[xlings] no running sessions\n").empty(), "empty ps banner returns empty");

    const auto reqs = ParseSubosRequestsJson(R"({"id":"r-9","instance":"dev","kind":"pkg","resource":"xim:cmake","detail":"needs cmake","state":"pending"})");
    Check(reqs.size() == 1 && reqs[0].id == "r-9" && reqs[0].resource == "xim:cmake", "subos requests parsed");

    const std::string rep_json = R"({"instance":"dev","policy_changes":["net=nat"],"sessions":[{"id":"rep-1","backend":"bwrap","started":"2026-11-23","commands":3,"executions":2,"exit":0,"ms":420,"denied":["curl"],"changed":["/etc/hosts"],"programs":["cmake"]}]})";
    const auto rep = ParseSubosReportJson(rep_json);
    Check(rep.instance == "dev" && rep.policy_changes.size() == 1 && rep.sessions.size() == 1 &&
              rep.sessions[0].duration_ms == 420 && rep.sessions[0].commands == 3,
          "subos report parsed");

    const auto info_evs = ParseNdjsonLines(
        R"({"dataKind":"info_panel","kind":"data","payload":{"title":"default","fields":[{"label":"active","value":"yes","highlight":true}],"extra_fields":[{"label":"vendors","value":"NONE","alert":true}]}})"
    );
    const std::string info_fmt = FormatNdjsonInfoPanel(info_evs);
    Check(info_fmt.find("=== default ===") != std::string::npos && info_fmt.find("[!] vendors: NONE") != std::string::npos,
          "info_panel formatted with alerts");

    const auto diff_evs = ParseNdjsonLines(R"({"dataKind":"subos_diff","kind":"data","payload":[]})");
    Check(FormatSubosDiffEvents(diff_evs).find("一致") != std::string::npos, "empty diff formatted");

    const auto rb_evs = ParseNdjsonLines(
        R"({"code":"E_INVALID_INPUT","hint":"xlings use <pkg> --subos default","kind":"error","message":"no root generations"})"
    );
    const std::string rb_fmt = FormatSubosRollbackListEvents(rb_evs);
    Check(rb_fmt.find("no root generations") != std::string::npos && rb_fmt.find("提示: xlings use") != std::string::npos,
          "rollback list error+hint formatted");
}

void TestTimestampParsingAndFormatting() {
    const auto utc_ep = ParseTimestampToEpochSeconds("2026-11-23T01:21:48Z");
    const auto frac_ep = ParseTimestampToEpochSeconds("2026-11-23T01:21:48.987Z");
    const auto plus8_ep = ParseTimestampToEpochSeconds("2026-11-23T09:21:48+08:00");
    const auto minus5_ep = ParseTimestampToEpochSeconds("2026-11-22T20:21:48-05:00");
    Check(utc_ep.has_value(), "ISO UTC timestamp parsed");
    Check(frac_ep == utc_ep, "fractional seconds timestamp parsed");
    Check(plus8_ep == utc_ep, "+08:00 timestamp equals UTC epoch");
    Check(minus5_ep == utc_ep, "-05:00 timestamp equals UTC epoch");

    const std::string ep_sec_str = std::to_string(*utc_ep);
    const std::string ep_ms_str = ep_sec_str + "123";
    Check(ParseTimestampToEpochSeconds(ep_sec_str) == utc_ep, "10-digit Unix seconds parsed");
    Check(ParseTimestampToEpochSeconds(ep_ms_str) == utc_ep, "13-digit Unix ms parsed");

    Check(FormatEpochWithOffsetMinutes(*utc_ep, 480) == "2026-11-23 09:21:48", "+08:00 formatted deterministically");
    Check(FormatEpochWithOffsetMinutes(*utc_ep, 0) == "2026-11-23 01:21:48", "UTC formatted deterministically");
    Check(FormatEpochWithOffsetMinutes(*utc_ep, -300) == "2026-11-22 20:21:48", "-05:00 formatted deterministically");

    const std::string local_iso = FormatLocalTimestamp("2026-11-23T01:21:48Z");
    const std::string local_epoch = FormatLocalTimestamp(ep_sec_str);
    Check(local_iso.size() == 19 && local_iso[10] == ' ' && local_iso.find('T') == std::string::npos &&
              local_iso.find('Z') == std::string::npos,
          "local timestamp formatted as YYYY-MM-DD HH:MM:SS");
    Check(local_iso == local_epoch, "ISO UTC and Unix epoch yield identical local formatting");
    Check(FormatLocalTimestamp(local_iso) == local_iso, "already-formatted local timestamp is idempotent");

    Check(IsIsoTimestampWithZone("2026-11-23T01:21:48Z"), "ISO UTC detected as zoned timestamp");
    Check(!IsIsoTimestampWithZone("2026-11-23 09:21:48"), "naive local string not re-converted");
    Check(!ParseTimestampToEpochSeconds("2026.11.22.1").has_value(), "version string rejected as timestamp");
    Check(FormatLocalTimestamp("2026.11.22.1") == "2026.11.22.1", "version string preserved unchanged");
    Check(FormatLocalTimestamp("2026-11-23") == "2026-11-23", "date-only string preserved unchanged");
    Check(FormatLocalTimestamp("115") == "115", "short integer preserved unchanged");
    Check(FormatLocalTimestamp("session-1") == "session-1", "session identifier preserved unchanged");

    const std::string embedded = FormatTimestampsInText("created 2026-11-23T01:21:48Z (ok)");
    Check(embedded == ("created " + local_iso + " (ok)"), "embedded ISO UTC timestamp replaced in free text");
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
}

int main() {
    TestSplitPackageSpec();
    TestNdjsonAndEnv();
    TestSystemStatusAndBindings();
    TestInstalledAndGrouping();
    TestSearchAndInfoAndPlan();
    TestSubosReposIndexesAndCli();
    TestLiveXlingsBinaryAndEnv();
    TestVersionCompareAndMultiActiveGrouping();
    TestParseRemovePlanEvents();
    TestParsePayloadStatusAndUpdatablePlans();
    TestWhitespaceAndSearchNormalization();
    TestProcessStreamingAndCancel();
    TestProcessStreamingExceptionSafety();
    TestAppSettingsSerializationAndPersistence();
    TestBundledResourcesAndChildEnvSanitization();
    TestWindowsProcessHelpersAndSpawn();
    TestExternalUrlValidationAndNormalization();
    TestConfigExtensionsAndNetworkParsers();
    TestSubosGovernanceAndFormatParsers();
    TestTimestampParsingAndFormatting();
    TestAppUpdaterParsingSha256AndSelection();

    if (failures == 0) {
        std::println("All parser and xlings integration tests passed.");
        return 0;
    }
    std::println(std::cerr, "{} test(s) failed.", failures);
    return 1;
}

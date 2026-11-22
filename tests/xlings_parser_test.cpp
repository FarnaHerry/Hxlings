import std;
import xlings.types;
import xlings.parser;
import xlings.process;

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

} // namespace

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

    if (failures == 0) {
        std::println("All parser and xlings integration tests passed.");
        return 0;
    }
    std::println(std::cerr, "{} test(s) failed.", failures);
    return 1;
}

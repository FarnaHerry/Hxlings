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

} // namespace

int main() {
    TestSplitPackageSpec();
    TestNdjsonAndEnv();
    TestSystemStatusAndBindings();
    TestInstalledAndGrouping();
    TestSearchAndInfoAndPlan();
    TestSubosReposIndexesAndCli();
    TestLiveXlingsBinaryAndEnv();

    if (failures == 0) {
        std::println("All parser and xlings integration tests passed.");
        return 0;
    }
    std::println(std::cerr, "{} test(s) failed.", failures);
    return 1;
}

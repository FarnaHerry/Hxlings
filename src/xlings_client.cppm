export module xlings.client;

import std;
import nlohmann.json;
import xlings.types;
import xlings.parser;
import xlings.process;

export namespace hxlings {

using EventCallback = std::function<void(const NdjsonEvent&)>;
using LineCallback = std::function<void(const std::string&)>;

class XlingsClient {
public:
    explicit XlingsClient(std::string binary_path = ResolveXlingsBinary())
        : binary_path_(std::move(binary_path)) {}

    [[nodiscard]] const std::string& BinaryPath() const noexcept {
        return binary_path_;
    }

    CommandOutcome RunInterface(
        std::string_view capability,
        const nlohmann::json& args_json = nlohmann::json::object(),
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> cmd{binary_path_, "interface", std::string(capability)};
        if (!args_json.is_null() && !(args_json.is_object() && args_json.empty())) {
            cmd.emplace_back("--args");
            cmd.push_back(args_json.dump());
        }

        CommandOutcome outcome;
        const ProcessResult proc = RunProcessStreaming(
            cmd,
            [&](const std::string& line) {
                if (line.empty()) return;
                NdjsonEvent ev = ParseNdjsonLine(line);
                if (ev.kind == "error" && outcome.error_message.empty()) {
                    outcome.error_message = ev.message;
                }
                if (ev.kind == "result" && ev.exit_code.has_value()) {
                    outcome.exit_code = *ev.exit_code;
                }
                if (on_event) on_event(ev);
                outcome.events.push_back(std::move(ev));
            },
            std::move(control)
        );

        outcome.raw_output = proc.raw_output;
        if (outcome.exit_code < 0) outcome.exit_code = proc.exit_code;
        if (!proc.error_message.empty() && outcome.error_message.empty()) {
            outcome.error_message = proc.error_message;
        }
        outcome.ok = !proc.cancelled && proc.exit_code == 0 && outcome.exit_code == 0;
        if (!outcome.ok && outcome.error_message.empty()) {
            outcome.error_message = proc.cancelled ? "Cancelled" : ("Command exited with code " + std::to_string(outcome.exit_code));
        }
        return outcome;
    }

    CommandOutcome RunCli(
        const std::vector<std::string>& sub_args,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> cmd;
        cmd.reserve(1 + sub_args.size());
        cmd.push_back(binary_path_);
        cmd.insert(cmd.end(), sub_args.begin(), sub_args.end());

        CommandOutcome outcome;
        const ProcessResult proc = RunProcessStreaming(
            cmd,
            [&](const std::string& line) {
                if (on_line) on_line(line);
                NdjsonEvent ev;
                ev.kind = "raw";
                ev.message = line;
                ev.raw_line = line;
                outcome.events.push_back(std::move(ev));
            },
            std::move(control)
        );

        outcome.raw_output = proc.raw_output;
        outcome.exit_code = proc.exit_code;
        outcome.error_message = proc.error_message;
        outcome.ok = !proc.cancelled && proc.exit_code == 0;
        if (!outcome.ok && outcome.error_message.empty()) {
            outcome.error_message = proc.cancelled ? "Cancelled" : ("Command exited with code " + std::to_string(outcome.exit_code));
        }
        return outcome;
    }

    XlingsVersionInfo QueryVersion() const {
        const ProcessResult proc = RunProcessStreaming({binary_path_, "interface", "--version"});
        return ParseVersionJson(proc.raw_output);
    }

    XlingsEnv QueryEnv() const {
        const CommandOutcome out = RunInterface("env");
        XlingsEnv env = ParseEnvEvents(out.events);
        env.binary_path = binary_path_;
        return env;
    }

    SystemStatus QuerySystemStatus() const {
        const CommandOutcome out = RunInterface("system_status");
        return ParseSystemStatusEvents(out.events);
    }

    std::vector<InstalledPkg> ListInstalledPackages(std::string_view filter = {}) const {
        nlohmann::json args = nlohmann::json::object();
        if (!filter.empty()) args["filter"] = std::string(filter);
        const CommandOutcome out = RunInterface("list_packages", args);
        return ParseInstalledPackagesEvents(out.events);
    }

    std::vector<InstalledVersionItem> ListInstalledVersions(std::string_view plain_name) const {
        const CommandOutcome out = RunInterface("list_installed_versions", {{"target", std::string(plain_name)}});
        return ParseInstalledVersionsEvents(out.events);
    }

    SearchOutcome SearchPackagesChecked(std::string_view keyword, std::string_view category = {}) const {
        const std::string trimmed_kw = TrimWhitespace(keyword);
        const std::string trimmed_cat = TrimWhitespace(category);
        nlohmann::json args = nlohmann::json::object();
        if (!trimmed_kw.empty()) args["keyword"] = trimmed_kw;
        if (!trimmed_cat.empty()) args["category"] = trimmed_cat;
        const CommandOutcome out = RunInterface("search_packages", args);
        SearchOutcome result;
        result.packages = ParseSearchPackagesEvents(out.events);
        result.ok = out.ok;
        if (!out.ok) {
            result.error_message = out.error_message;
        }
        return result;
    }

    std::vector<SearchPkg> SearchPackages(std::string_view keyword, std::string_view category = {}) const {
        return SearchPackagesChecked(keyword, category).packages;
    }

    PackageDetail QueryPackageInfo(std::string_view target) const {
        const CommandOutcome out = RunInterface("package_info", {{"target", std::string(target)}});
        return ParsePackageInfoEvents(out.events);
    }

    std::vector<InstallPlanItem> PlanInstall(const std::vector<std::string>& targets, bool global = false) const {
        nlohmann::json args = {{"targets", targets}};
        if (global) args["global"] = true;
        const CommandOutcome out = RunInterface("plan_install", args);
        return ParseInstallPlanEvents(out.events);
    }

    std::vector<SubosEntry> ListSubos() const {
        const CommandOutcome out = RunInterface("list_subos");
        return ParseSubosListEvents(out.events);
    }

    std::vector<std::string> ListSubosShims() const {
        const CommandOutcome out = RunInterface("list_subos_shims");
        return ParseSubosShimsEvents(out.events);
    }

    std::vector<SubosEnvVar> QuerySubosEnv(std::string_view name = {}) const {
        nlohmann::json args = nlohmann::json::object();
        if (!name.empty()) args["name"] = std::string(name);
        const CommandOutcome out = RunInterface("subos_env", args);
        return ParseSubosEnvEvents(out.events);
    }

    std::vector<SubosEvent> QuerySubosEvents(
        std::string_view name,
        std::string_view kind = {},
        int limit = 100
    ) const {
        nlohmann::json args = {{"name", std::string(name)}, {"limit", limit}};
        if (!kind.empty()) args["kind"] = std::string(kind);
        const CommandOutcome out = RunInterface("subos_events", args);
        return ParseSubosEvents(out.events);
    }

    std::vector<RepoEntry> ListRepos() const {
        const CommandOutcome out = RunInterface("list_repos");
        return ParseRepoListEvents(out.events);
    }

    std::vector<IndexRepoVersions> ListIndexVersions(std::string_view name = {}) const {
        nlohmann::json args = nlohmann::json::object();
        if (!name.empty()) args["name"] = std::string(name);
        const CommandOutcome out = RunInterface("list_index_versions", args);
        return ParseIndexVersionsEvents(out.events);
    }

    ConfigSnapshot QueryConfig() const {
        const CommandOutcome out = RunCli({"config", "--agent"});
        return ParseConfigText(out.raw_output);
    }

    std::vector<LocalXpkg> ListLocalXpkgs() const {
        const CommandOutcome out = RunCli({"config", "--list-xpkg", "--agent"});
        return ParseLocalXpkgTable(out.raw_output);
    }

    std::vector<ProfileGeneration> ListProfiles() const {
        const CommandOutcome out = RunCli({"profile", "list", "--agent"});
        return ParseProfileListText(out.raw_output);
    }

    std::vector<SelfLogEntry> QuerySelfLog(int limit = 25) const {
        const CommandOutcome out = RunCli({"self", "log", "--json"});
        auto entries = ParseSelfLogLines(out.raw_output);
        if (limit > 0 && entries.size() > static_cast<std::size_t>(limit)) {
            entries.resize(static_cast<std::size_t>(limit));
        }
        return entries;
    }

    CommandOutcome InstallPackages(
        const std::vector<std::string>& targets,
        bool global,
        bool activate_after_install,
        std::string_view subos,
        bool reconfig,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        nlohmann::json args = {
            {"targets", targets},
            {"yes", true},
        };
        if (global) args["global"] = true;
        if (activate_after_install) args["useAfterInstall"] = true;
        if (!subos.empty()) args["subos"] = std::string(subos);
        if (reconfig) args["reconfig"] = true;
        return RunInterface("install_packages", args, std::move(on_event), std::move(control));
    }

    CommandOutcome RemovePackage(
        std::string_view target,
        std::string_view older_than = {},
        bool dry_run = false,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        nlohmann::json args = {
            {"target", std::string(target)},
            {"yes", true},
        };
        if (!older_than.empty()) args["older_than"] = std::string(older_than);
        if (dry_run) args["dry_run"] = true;
        return RunInterface("remove_package", args, std::move(on_event), std::move(control));
    }

    std::optional<RemovePlan> PlanRemoveOlderThan(
        std::string_view target,
        std::string_view older_than
    ) const {
        const CommandOutcome out = RemovePackage(target, older_than, true);
        if (!out.ok) return std::nullopt;
        return ParseRemovePlanEvents(out.events);
    }

    std::vector<PayloadStatusItem> QueryPayloadStatus(const std::vector<std::string>& targets) const {
        if (targets.empty()) return {};
        const CommandOutcome out = RunInterface("payload_status", {{"targets", targets}});
        return ParsePayloadStatusEvents(out.events);
    }

    CommandOutcome UpdatePackages(
        std::string_view target = {},
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        nlohmann::json args = {{"yes", true}};
        if (!target.empty()) args["target"] = std::string(target);
        return RunInterface("update_packages", args, std::move(on_event), std::move(control));
    }

    CommandOutcome ActivateVersion(
        std::string_view plain_name,
        std::string_view version,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunInterface(
            "use_version",
            {{"target", std::string(plain_name)}, {"version", std::string(version)}},
            std::move(on_event),
            std::move(control)
        );
    }

    CommandOutcome CreateSubos(
        std::string_view name,
        std::string_view custom_dir = {},
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        nlohmann::json args = {{"name", std::string(name)}, {"yes", true}};
        if (!custom_dir.empty()) args["dir"] = std::string(custom_dir);
        return RunInterface("create_subos", args, std::move(on_event), std::move(control));
    }

    CommandOutcome SwitchSubos(
        std::string_view name,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunInterface("switch_subos", {{"name", std::string(name)}}, std::move(on_event), std::move(control));
    }

    CommandOutcome RemoveSubos(
        std::string_view name,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunInterface(
            "remove_subos",
            {{"name", std::string(name)}, {"yes", true}},
            std::move(on_event),
            std::move(control)
        );
    }

    CommandOutcome StartSubosSession(
        std::string_view name,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunInterface("subos_start", {{"name", std::string(name)}}, std::move(on_event), std::move(control));
    }

    CommandOutcome StopSubosSession(
        std::string_view name,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunInterface("subos_stop", {{"name", std::string(name)}}, std::move(on_event), std::move(control));
    }

    CommandOutcome ExecInSubos(
        std::string_view name,
        const std::vector<std::string>& argv,
        bool sandbox,
        std::string_view cwd,
        std::string_view timeout,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        nlohmann::json args = {{"argv", argv}};
        if (!name.empty()) args["name"] = std::string(name);
        if (sandbox) args["sandbox"] = true;
        if (!cwd.empty()) args["cwd"] = std::string(cwd);
        if (!timeout.empty()) args["timeout"] = std::string(timeout);
        return RunInterface("subos_exec", args, std::move(on_event), std::move(control));
    }

    CommandOutcome ExecInSubos(
        std::string_view name,
        const std::vector<std::string>& argv,
        bool sandbox,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return ExecInSubos(name, argv, sandbox, {}, {}, std::move(on_event), std::move(control));
    }

    CommandOutcome AddRepo(
        std::string_view name,
        std::string_view url,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunInterface(
            "add_repo",
            {{"name", std::string(name)}, {"url", std::string(url)}},
            std::move(on_event),
            std::move(control)
        );
    }

    CommandOutcome RemoveRepo(
        std::string_view name,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunInterface("remove_repo", {{"name", std::string(name)}}, std::move(on_event), std::move(control));
    }

    CommandOutcome SyncIndex(
        std::string_view name,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunInterface("sync_index", {{"name", std::string(name)}}, std::move(on_event), std::move(control));
    }

    CommandOutcome PinIndexVersion(
        std::string_view name,
        std::string_view version,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli(
            {"index", "use", std::string(name), std::string(version), "--yes", "--agent"},
            std::move(on_line),
            std::move(control)
        );
    }

    CommandOutcome SetConfigMirror(
        std::string_view mirror,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli({"config", "--mirror", std::string(mirror), "--agent"}, std::move(on_line), std::move(control));
    }

    CommandOutcome SetConfigLang(
        std::string_view lang,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli({"config", "--lang", std::string(lang), "--agent"}, std::move(on_line), std::move(control));
    }

    CommandOutcome SetConfigUiMode(
        std::string_view mode,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli({"config", "--ui-mode", std::string(mode), "--agent"}, std::move(on_line), std::move(control));
    }

    CommandOutcome SetConfigInteractive(
        bool enabled,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli(
            {"config", "--interactive", enabled ? "true" : "false", "--agent"},
            std::move(on_line),
            std::move(control)
        );
    }

    CommandOutcome AddLocalXpkg(
        std::string_view file_path,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli({"config", "--add-xpkg", std::string(file_path), "--agent"}, std::move(on_line), std::move(control));
    }

    CommandOutcome RemoveLocalXpkg(
        std::string_view name,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli({"config", "--remove-xpkg", std::string(name), "--agent"}, std::move(on_line), std::move(control));
    }

    CommandOutcome RunDoctor(
        bool deep,
        bool fix,
        bool dry_run,
        bool isolation,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> args{"self", "doctor", "--yes", "--agent"};
        if (deep) args.emplace_back("--deep");
        if (fix) args.emplace_back("--fix");
        if (dry_run) args.emplace_back("--dry-run");
        if (isolation) args.emplace_back("--isolation");
        return RunCli(args, std::move(on_line), std::move(control));
    }

    CommandOutcome RunSelfClean(
        bool dry_run,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> args{"self", "clean", "--yes", "--agent"};
        if (dry_run) args.emplace_back("--dry-run");
        return RunCli(args, std::move(on_line), std::move(control));
    }

    CommandOutcome RunSelfUpdate(
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli({"self", "update", "--yes", "--agent"}, std::move(on_line), std::move(control));
    }

    CommandOutcome CommitProfile(
        std::string_view reason = {},
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> args{"profile", "commit"};
        if (!reason.empty()) args.emplace_back(reason);
        args.emplace_back("--yes");
        args.emplace_back("--agent");
        return RunCli(args, std::move(on_line), std::move(control));
    }

    CommandOutcome RollbackProfile(
        std::string_view generation,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli(
            {"profile", "rollback", std::string(generation), "--yes", "--agent"},
            std::move(on_line),
            std::move(control)
        );
    }

    std::vector<NetworkEndpoint> QueryNetworkEndpoints() const {
        const auto outcome = RunInterface("network_endpoints");
        return ParseNetworkEndpointsEvents(outcome.events);
    }

    NetworkProbeResult ProbeUrl(std::string_view url, int socket_timeout_ms = 5000) const {
        const auto outcome = RunInterface(
            "probe_url",
            {{"url", std::string(url)}, {"socket_timeout_ms", socket_timeout_ms}}
        );
        return ParseNetworkProbeEvents(outcome.events, url);
    }

    std::vector<ConfigThemeItem> QueryConfigThemes() const {
        const auto outcome = RunCli({"config", "--theme", "list", "--agent"});
        return ParseConfigThemeList(outcome.raw_output);
    }

    CommandOutcome SetConfigTheme(
        std::string_view theme,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli({"config", "--theme", std::string(theme), "--agent"}, std::move(on_line), std::move(control));
    }

    CommandOutcome SetConfigRuntime(
        std::string_view runtime_spec,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli(
            {"config", "--runtime", std::string(runtime_spec), "--agent"},
            std::move(on_line),
            std::move(control)
        );
    }

    CommandOutcome AddArchiveDir(
        std::string_view dir_path,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli(
            {"config", "--add-archives", std::string(dir_path), "--agent"},
            std::move(on_line),
            std::move(control)
        );
    }

    CommandOutcome RemoveArchiveDir(
        std::string_view dir_path,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli(
            {"config", "--rm-archives", std::string(dir_path), "--agent"},
            std::move(on_line),
            std::move(control)
        );
    }

    CommandOutcome ClearLocalXpkgs(
        std::string_view mode,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli(
            {"config", "--clear-xpkg", std::string(mode), "--agent"},
            std::move(on_line),
            std::move(control)
        );
    }

    CommandOutcome RunScript(
        std::string_view script_file,
        const std::vector<std::string>& script_args = {},
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> args{"script", std::string(script_file)};
        args.insert(args.end(), script_args.begin(), script_args.end());
        return RunCli(args, std::move(on_line), std::move(control));
    }

    CommandOutcome RunSelfMigrate(
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli({"self", "migrate", "--yes", "--agent"}, std::move(on_line), std::move(control));
    }

    CommandOutcome RunSelfInit(
        std::string_view home_dir = {},
        bool force = false,
        LineCallback on_line = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> args{"self", "init", "--yes", "--agent"};
        if (!home_dir.empty()) {
            args.emplace_back("--home");
            args.emplace_back(home_dir);
        }
        if (force) args.emplace_back("--force");
        return RunCli(args, std::move(on_line), std::move(control));
    }

    CommandOutcome RunSubosCommand(
        const std::vector<std::string>& argv,
        bool yes = false,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        nlohmann::json args = {{"argv", argv}};
        if (yes) args["yes"] = true;
        return RunInterface("subos_command", args, std::move(on_event), std::move(control));
    }

    SubosStatusSnapshot QuerySubosStatus(std::string_view name) const {
        if (name.empty()) return {};
        const auto outcome = RunCli({"subos", "status", "--json", std::string(name)});
        return ParseSubosStatusJson(outcome.raw_output);
    }

    SubosPolicySnapshot QuerySubosConfig(std::string_view name) const {
        if (name.empty()) return {};
        const auto outcome = RunCli({"subos", "config", "--json", std::string(name)});
        return ParseSubosConfigJson(outcome.raw_output);
    }

    SubosDoctorSnapshot QuerySubosDoctor(std::string_view name = {}) const {
        std::vector<std::string> args{"subos", "doctor", "--json"};
        if (!name.empty()) args.emplace_back(name);
        const auto outcome = RunCli(args);
        return ParseSubosDoctorJson(outcome.raw_output);
    }

    std::vector<SubosSessionItem> QuerySubosPs(std::string_view name = {}) const {
        std::vector<std::string> args{"subos", "ps", "--json"};
        if (!name.empty()) args.emplace_back(name);
        const auto outcome = RunCli(args);
        return ParseSubosPsJson(outcome.raw_output);
    }

    std::vector<SubosRequestItem> QuerySubosRequests(std::string_view name) const {
        if (name.empty()) return {};
        const auto outcome = RunCli({"subos", "requests", "--json", std::string(name)});
        return ParseSubosRequestsJson(outcome.raw_output);
    }

    SubosReportSnapshot QuerySubosReport(std::string_view name) const {
        if (name.empty()) return {};
        const auto outcome = RunCli({"subos", "report", "--json", std::string(name)});
        return ParseSubosReportJson(outcome.raw_output);
    }

    std::string QuerySubosInfo(std::string_view name) const {
        if (name.empty()) return {};
        const auto outcome = RunSubosCommand({"info", std::string(name)});
        const std::string formatted = FormatNdjsonInfoPanel(outcome.events);
        return formatted.empty() ? outcome.raw_output : formatted;
    }

    std::string QuerySubosRollbackList(std::string_view name) const {
        if (name.empty()) return {};
        const auto outcome = RunSubosCommand({"rollback", "--list", std::string(name)});
        const std::string formatted = FormatSubosRollbackListEvents(outcome.events);
        return formatted.empty() ? outcome.error_message : formatted;
    }

    std::string DiffSubos(std::string_view lhs, std::string_view rhs) const {
        if (lhs.empty() || rhs.empty()) return {};
        const auto outcome = RunSubosCommand({"diff", std::string(lhs), std::string(rhs)});
        const std::string formatted = FormatSubosDiffEvents(outcome.events);
        return formatted.empty() ? outcome.error_message : formatted;
    }

    CommandOutcome CreateSubosAdvanced(
        const SubosCreateOptions& opts,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> argv{"new"};
        if (!opts.sandbox.empty()) {
            argv.emplace_back("--sandbox");
            argv.push_back(opts.sandbox);
        }
        if (!opts.storage.empty()) {
            argv.emplace_back("--storage");
            argv.push_back(opts.storage);
        }
        if (!opts.from_subos.empty()) {
            argv.emplace_back("--from");
            argv.push_back(opts.from_subos);
        }
        if (!opts.runtime.empty()) {
            argv.emplace_back("--runtime");
            argv.push_back(opts.runtime);
        }
        if (opts.rootfs) argv.emplace_back("--rootfs");
        if (!opts.proxy.empty()) {
            argv.emplace_back("--proxy");
            argv.push_back(opts.proxy);
        }
        if (!opts.abi.empty()) {
            argv.emplace_back("--abi");
            argv.push_back(opts.abi);
        }
        if (opts.dry_run) argv.emplace_back("--dry-run");
        argv.push_back(opts.name);
        return RunSubosCommand(argv, true, std::move(on_event), std::move(control));
    }

    CommandOutcome UpdateSubosConfig(
        std::string_view name,
        const SubosPolicyPatch& patch,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> argv{"config"};
        if (!patch.sandbox.empty()) {
            argv.emplace_back("--sandbox");
            argv.push_back(patch.sandbox);
        }
        if (!patch.net.empty()) {
            argv.emplace_back("--net");
            argv.push_back(patch.net);
        }
        if (!patch.proxy.empty()) {
            argv.emplace_back("--proxy");
            argv.push_back(patch.proxy);
        }
        if (!patch.fetch_default.empty()) {
            argv.emplace_back("--fetch");
            argv.push_back(patch.fetch_default);
        }
        if (!patch.index_update.empty()) {
            argv.emplace_back("--index-update");
            argv.push_back(patch.index_update);
        }
        if (!patch.observe_level.empty()) {
            argv.emplace_back("--observe");
            argv.push_back(patch.observe_level);
        }
        if (!patch.allow_cap.empty()) {
            argv.emplace_back("--allow");
            argv.push_back(patch.allow_cap);
        }
        if (!patch.disallow_cap.empty()) {
            argv.emplace_back("--disallow");
            argv.push_back(patch.disallow_cap);
        }
        if (!patch.env_pass_csv.empty()) {
            std::size_t pos = 0;
            while (pos <= patch.env_pass_csv.size()) {
                const std::size_t sep = patch.env_pass_csv.find_first_of(",; ", pos);
                const std::string token = sep == std::string::npos
                                              ? patch.env_pass_csv.substr(pos)
                                              : patch.env_pass_csv.substr(pos, sep - pos);
                if (!token.empty()) {
                    argv.emplace_back("--env-pass");
                    argv.push_back(token);
                }
                if (sep == std::string::npos) break;
                pos = sep + 1;
            }
        }
        if (!patch.mount_spec.empty()) {
            argv.emplace_back("--mount");
            argv.push_back(patch.mount_spec);
        }
        if (!patch.unmount_spec.empty()) {
            argv.emplace_back("--unmount");
            argv.push_back(patch.unmount_spec);
        }
        if (patch.no_degrade.has_value()) {
            argv.emplace_back(*patch.no_degrade ? "--no-degrade" : "--degrade");
        }
        if (patch.policy_upgrade) {
            argv.emplace_back("--policy-upgrade");
        }
        if (patch.reset) argv.emplace_back("--reset");
        argv.emplace_back(name);
        return RunSubosCommand(argv, true, std::move(on_event), std::move(control));
    }

    CommandOutcome ApproveSubosRequest(
        std::string_view name,
        std::string_view request_id,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunSubosCommand(
            {"approve", std::string(name), std::string(request_id)},
            true,
            std::move(on_event),
            std::move(control)
        );
    }

    CommandOutcome DenySubosRequest(
        std::string_view name,
        std::string_view request_id,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunSubosCommand(
            {"deny", std::string(name), std::string(request_id)},
            true,
            std::move(on_event),
            std::move(control)
        );
    }

    CommandOutcome RunSubosDoctorFix(
        std::string_view name = {},
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> argv{"doctor", "--fix"};
        if (!name.empty()) argv.emplace_back(name);
        return RunSubosCommand(argv, true, std::move(on_event), std::move(control));
    }

    CommandOutcome UpgradeSubos(
        std::string_view name,
        std::string_view to_version = {},
        bool dry_run = false,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> argv{"upgrade"};
        if (!to_version.empty()) {
            argv.emplace_back("--to");
            argv.emplace_back(to_version);
        }
        if (dry_run) argv.emplace_back("--dry-run");
        argv.emplace_back(name);
        return RunSubosCommand(argv, true, std::move(on_event), std::move(control));
    }

    CommandOutcome RollbackSubos(
        std::string_view name,
        std::string_view generation = {},
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> argv{"rollback"};
        if (!generation.empty()) {
            argv.emplace_back("--to");
            argv.emplace_back(generation);
        }
        argv.emplace_back(name);
        return RunSubosCommand(argv, true, std::move(on_event), std::move(control));
    }

    CommandOutcome RebindSubosRuntime(
        std::string_view name,
        std::string_view binding,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> argv{"runtime", std::string(binding)};
        if (!name.empty()) argv.emplace_back(name);
        return RunSubosCommand(argv, true, std::move(on_event), std::move(control));
    }

    CommandOutcome ExportSubos(
        std::string_view name,
        std::string_view format,
        std::string_view output_path,
        std::string_view disk_size = {},
        bool with_data = false,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> argv{"export"};
        if (format == "rootfs") {
            argv.emplace_back("--rootfs");
        } else if (format == "disk") {
            argv.emplace_back("--disk");
        } else {
            argv.emplace_back("--tar");
        }
        argv.emplace_back(output_path);
        if (format == "disk" && !disk_size.empty()) {
            argv.emplace_back("--size");
            argv.emplace_back(disk_size);
        }
        if (with_data) argv.emplace_back("--with-data");
        argv.emplace_back(name);
        return RunSubosCommand(argv, true, std::move(on_event), std::move(control));
    }

    CommandOutcome PackSubos(
        std::string_view name,
        std::string_view as_pkg = {},
        std::string_view out_dir = {},
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> argv{"pack"};
        if (!as_pkg.empty()) {
            argv.emplace_back("--as");
            argv.emplace_back(as_pkg);
        }
        if (!out_dir.empty()) {
            argv.emplace_back("--out");
            argv.emplace_back(out_dir);
        }
        argv.emplace_back(name);
        return RunSubosCommand(argv, true, std::move(on_event), std::move(control));
    }

    CommandOutcome CopySubosFiles(
        std::string_view src,
        std::string_view dst,
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunSubosCommand(
            {"cp", std::string(src), std::string(dst)},
            true,
            std::move(on_event),
            std::move(control)
        );
    }

private:
    std::string binary_path_;
};

} // namespace hxlings

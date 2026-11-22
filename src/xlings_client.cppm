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

    std::vector<SearchPkg> SearchPackages(std::string_view keyword, std::string_view category = {}) const {
        nlohmann::json args = nlohmann::json::object();
        if (!keyword.empty()) args["keyword"] = std::string(keyword);
        if (!category.empty()) args["category"] = std::string(category);
        const CommandOutcome out = RunInterface("search_packages", args);
        return ParseSearchPackagesEvents(out.events);
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
        EventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        nlohmann::json args = {{"argv", argv}};
        if (!name.empty()) args["name"] = std::string(name);
        if (sandbox) args["sandbox"] = true;
        return RunInterface("subos_exec", args, std::move(on_event), std::move(control));
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

private:
    std::string binary_path_;
};

} // namespace hxlings

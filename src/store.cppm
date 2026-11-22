export module hxlings.store;

import std;
import huxerui;
import xlings.types;
import xlings.parser;
import xlings.process;
import xlings.client;
import hxlings.theme;

using namespace huxerui;

export namespace hxlings {

struct RefreshSnapshot {
    XlingsVersionInfo version_info;
    XlingsEnv env;
    SystemStatus system_status;
    std::vector<InstalledPkg> installed_raw;
    std::vector<PkgGroup> installed_groups;
    std::vector<SubosEntry> subos_list;
    std::vector<std::string> active_shims;
    std::vector<RepoEntry> repos;
    std::vector<IndexRepoVersions> index_versions;
    ConfigSnapshot config;
    std::vector<LocalXpkg> local_xpkgs;
    std::vector<ProfileGeneration> profiles;
    std::vector<SelfLogEntry> recent_logs;
};

struct InstalledDetailSnapshot {
    std::string key;
    PackageDetail detail;
    std::vector<InstalledVersionItem> versions;
};

struct SubosDetailSnapshot {
    std::string name;
    std::vector<SubosEnvVar> env_vars;
    std::vector<SubosEvent> events;
};

class XlingsStore : public std::enable_shared_from_this<XlingsStore> {
public:
    XlingsStore() = default;

    bool skip_initial_refresh = false;

    // UI Preferences
    State<bool> dark_mode{false};
    State<Color> brand_color{DefaultBrandColor()};
    State<std::size_t> active_nav{0};
    State<bool> console_expanded{false};

    // Environment & Dashboard
    State<XlingsVersionInfo> version_info{XlingsVersionInfo{}};
    State<XlingsEnv> env{XlingsEnv{}};
    State<SystemStatus> system_status{SystemStatus{}};
    State<std::vector<SelfLogEntry>> recent_logs{std::vector<SelfLogEntry>{}};
    State<bool> refreshing{false};
    State<bool> initial_loaded{false};

    // Installed Packages
    State<std::vector<InstalledPkg>> installed_raw{std::vector<InstalledPkg>{}};
    State<std::vector<PkgGroup>> installed_groups{std::vector<PkgGroup>{}};
    State<std::string> selected_installed_key{std::string{}};
    State<std::optional<PackageDetail>> selected_installed_detail{std::optional<PackageDetail>{}};
    State<std::vector<InstalledVersionItem>> selected_installed_versions{std::vector<InstalledVersionItem>{}};
    State<std::vector<InstallPlanItem>> installed_plan_preview{std::vector<InstallPlanItem>{}};
    State<std::optional<RemovePlan>> selected_remove_plan{std::optional<RemovePlan>{}};
    State<bool> loading_installed_detail{false};
    State<bool> loading_remove_plan{false};
    State<std::vector<ExpiredPackagePlan>> expired_plans{std::vector<ExpiredPackagePlan>{}};
    State<bool> scanning_expired{false};
    State<bool> has_scanned_expired{false};
    State<std::vector<UpdatablePackagePlan>> updatable_plans{std::vector<UpdatablePackagePlan>{}};
    State<bool> scanning_updates{false};
    State<bool> has_scanned_updates{false};

    // Store / Search
    State<std::vector<SearchPkg>> search_results{std::vector<SearchPkg>{}};
    State<bool> searching{false};
    State<bool> has_searched{false};
    State<std::string> selected_store_pkg{std::string{}};
    State<std::optional<PackageDetail>> selected_store_detail{std::optional<PackageDetail>{}};
    State<bool> loading_store_detail{false};
    State<std::vector<InstallPlanItem>> store_plan_preview{std::vector<InstallPlanItem>{}};

    // SubOS
    State<std::vector<SubosEntry>> subos_list{std::vector<SubosEntry>{}};
    State<std::string> selected_subos{std::string{}};
    State<std::vector<std::string>> active_shims{std::vector<std::string>{}};
    State<std::vector<SubosEnvVar>> selected_subos_env{std::vector<SubosEnvVar>{}};
    State<std::vector<SubosEvent>> selected_subos_events{std::vector<SubosEvent>{}};
    State<std::string> subos_exec_output{std::string{}};
    State<bool> loading_subos_detail{false};
    State<bool> running_subos_exec{false};

    // Indexes & Local Recipes
    State<std::vector<RepoEntry>> repos{std::vector<RepoEntry>{}};
    State<std::vector<IndexRepoVersions>> index_versions{std::vector<IndexRepoVersions>{}};
    State<std::vector<LocalXpkg>> local_xpkgs{std::vector<LocalXpkg>{}};

    // Config, Doctor & Profiles
    State<ConfigSnapshot> config{ConfigSnapshot{}};
    State<std::vector<ProfileGeneration>> profiles{std::vector<ProfileGeneration>{}};
    State<std::string> doctor_output{std::string{}};
    State<std::string> clean_output{std::string{}};

    // Streaming Task Runner & Console
    State<bool> task_running{false};
    State<std::string> task_title{std::string{}};
    State<std::string> task_last_status{std::string{"就绪"}};
    State<bool> task_last_failed{false};
    State<std::optional<float>> task_progress{std::optional<float>{}};
    State<std::vector<std::string>> console_lines{std::vector<std::string>{}};

    [[nodiscard]] const XlingsClient& Client() const noexcept {
        return client_;
    }

    void AppendConsoleLine(const std::string& line) {
        if (line.empty()) return;
        console_lines.Update([&](std::vector<std::string>& lines) {
            lines.push_back(line);
            constexpr std::size_t kMaxLines = 400;
            if (lines.size() > kMaxLines) {
                lines.erase(lines.begin(), lines.begin() + static_cast<std::ptrdiff_t>(lines.size() - kMaxLines));
            }
        });
    }

    void ClearConsole() {
        console_lines = std::vector<std::string>{};
    }

    void CancelCurrentOperation() {
        if (active_process_) {
            active_process_->Cancel();
            AppendConsoleLine("[操作] 已请求取消当前任务...");
        }
    }

    void RefreshAll(TaskScope tasks) {
        if (refreshing.Get()) return;
        refreshing = true;

        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, tasks, client]() -> Task<void> {
            RefreshSnapshot snap = co_await RunWorker([client]() -> RefreshSnapshot {
                RefreshSnapshot s;
                s.version_info = client.QueryVersion();
                s.env = client.QueryEnv();
                s.system_status = client.QuerySystemStatus();
                s.installed_raw = client.ListInstalledPackages();
                s.installed_groups = GroupInstalledPackages(s.installed_raw);
                s.subos_list = client.ListSubos();
                s.active_shims = client.ListSubosShims();
                s.repos = client.ListRepos();
                s.index_versions = client.ListIndexVersions();
                s.config = client.QueryConfig();
                s.local_xpkgs = client.ListLocalXpkgs();
                s.profiles = client.ListProfiles();
                s.recent_logs = client.QuerySelfLog(25);
                return s;
            });

            self->version_info = std::move(snap.version_info);
            self->env = std::move(snap.env);
            self->system_status = std::move(snap.system_status);
            self->installed_raw = std::move(snap.installed_raw);
            self->installed_groups = std::move(snap.installed_groups);
            self->subos_list = std::move(snap.subos_list);
            self->active_shims = std::move(snap.active_shims);
            self->repos = std::move(snap.repos);
            self->index_versions = std::move(snap.index_versions);
            self->config = std::move(snap.config);
            self->local_xpkgs = std::move(snap.local_xpkgs);
            self->profiles = std::move(snap.profiles);
            self->recent_logs = std::move(snap.recent_logs);
            self->refreshing = false;
            self->initial_loaded = true;

            // Ensure selected installed package stays valid
            const auto& groups = self->installed_groups.Get();
            const std::string cur_pkg = self->selected_installed_key.Get();
            auto pkg_it = std::ranges::find_if(groups, [&](const PkgGroup& g) { return g.key == cur_pkg; });
            if (pkg_it == groups.end() && !groups.empty()) {
                self->LoadInstalledDetail(tasks, groups.front());
            } else if (pkg_it != groups.end()) {
                self->LoadInstalledDetail(tasks, *pkg_it);
            } else {
                self->selected_installed_key = std::string{};
                self->selected_installed_detail = std::optional<PackageDetail>{};
                self->selected_installed_versions = std::vector<InstalledVersionItem>{};
            }

            // Ensure selected SubOS stays valid
            const auto& suboses = self->subos_list.Get();
            std::string subos_sel = self->selected_subos.Get();
            const bool subos_exists = std::ranges::any_of(suboses, [&](const SubosEntry& e) { return e.name == subos_sel; });
            if (!subos_exists) {
                subos_sel = self->env.Get().active_subos;
                if (subos_sel.empty() && !suboses.empty()) subos_sel = suboses.front().name;
                self->selected_subos = subos_sel;
            }
            if (!subos_sel.empty()) {
                self->LoadSubosDetail(tasks, subos_sel, {});
            }
        });
    }

    void LoadInstalledDetail(TaskScope tasks, const PkgGroup& group) {
        selected_installed_key = group.key;
        installed_plan_preview = std::vector<InstallPlanItem>{};
        selected_remove_plan = std::optional<RemovePlan>{};
        loading_installed_detail = true;
        loading_remove_plan = false;

        auto self = shared_from_this();
        const XlingsClient client = client_;
        const std::string target = group.key;
        const std::string plain_name = group.name;

        tasks.Launch([self, client, target, plain_name]() -> Task<void> {
            InstalledDetailSnapshot snap = co_await RunWorker([client, target, plain_name]() -> InstalledDetailSnapshot {
                InstalledDetailSnapshot s;
                s.key = target;
                s.detail = client.QueryPackageInfo(target);
                s.versions = client.ListInstalledVersions(plain_name);
                return s;
            });

            if (self->selected_installed_key.Get() == snap.key) {
                self->selected_installed_detail = std::move(snap.detail);
                self->selected_installed_versions = std::move(snap.versions);
                self->loading_installed_detail = false;
            }
        });
    }

    void RunSearch(TaskScope tasks, std::string keyword, std::string category = {}) {
        searching = true;
        has_searched = true;

        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, tasks, client, keyword = std::move(keyword), category = std::move(category)]() -> Task<void> {
            auto results = co_await RunWorker([client, keyword, category]() {
                return client.SearchPackages(keyword, category);
            });
            self->search_results = std::move(results);
            self->searching = false;

            const auto& res = self->search_results.Get();
            const std::string cur = self->selected_store_pkg.Get();
            const bool still_here = std::ranges::any_of(res, [&](const SearchPkg& p) { return p.package == cur; });
            if (!still_here && !res.empty()) {
                self->LoadStoreDetail(tasks, res.front().package);
            } else if (res.empty()) {
                self->selected_store_pkg = std::string{};
                self->selected_store_detail = std::optional<PackageDetail>{};
                self->store_plan_preview = std::vector<InstallPlanItem>{};
            }
        });
    }

    void LoadStoreDetail(TaskScope tasks, const std::string& package_target) {
        selected_store_pkg = package_target;
        store_plan_preview = std::vector<InstallPlanItem>{};
        loading_store_detail = true;

        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client, package_target]() -> Task<void> {
            PackageDetail detail = co_await RunWorker([client, package_target]() {
                return client.QueryPackageInfo(package_target);
            });
            if (self->selected_store_pkg.Get() == package_target) {
                self->selected_store_detail = std::move(detail);
                self->loading_store_detail = false;
            }
        });
    }

    void PreviewInstallPlan(TaskScope tasks, std::vector<std::string> targets, bool global, bool for_store) {
        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client, targets = std::move(targets), global, for_store]() -> Task<void> {
            auto items = co_await RunWorker([client, targets, global]() {
                return client.PlanInstall(targets, global);
            });
            if (for_store) {
                self->store_plan_preview = std::move(items);
            } else {
                self->installed_plan_preview = std::move(items);
            }
        });
    }

    void LoadSubosDetail(TaskScope tasks, const std::string& subos_name, const std::string& event_kind) {
        selected_subos = subos_name;
        loading_subos_detail = true;

        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client, subos_name, event_kind]() -> Task<void> {
            SubosDetailSnapshot snap = co_await RunWorker([client, subos_name, event_kind]() -> SubosDetailSnapshot {
                SubosDetailSnapshot s;
                s.name = subos_name;
                s.env_vars = client.QuerySubosEnv(subos_name);
                s.events = client.QuerySubosEvents(subos_name, event_kind, 120);
                return s;
            });
            if (self->selected_subos.Get() == snap.name) {
                self->selected_subos_env = std::move(snap.env_vars);
                self->selected_subos_events = std::move(snap.events);
                self->loading_subos_detail = false;
            }
        });
    }

    void RunSubosExec(TaskScope tasks, ToastHandle toast, std::string subos_name, std::vector<std::string> argv, bool sandbox) {
        if (argv.empty()) {
            toast.Show("请输入要执行的命令");
            return;
        }
        running_subos_exec = true;
        subos_exec_output = std::string{"正在执行...\n"};

        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, toast, client, subos_name = std::move(subos_name), argv = std::move(argv), sandbox]() -> Task<void> {
            const CommandOutcome outcome = co_await RunWorker([client, subos_name, argv, sandbox]() {
                return client.ExecInSubos(subos_name, argv, sandbox);
            });

            std::string text;
            for (const auto& ev : outcome.events) {
                if (ev.kind == "data" && ev.data_kind == "subos_exec_output" && !ev.message.empty()) {
                    text += ev.message + "\n";
                } else if ((ev.kind == "log" || ev.kind == "error") && !ev.message.empty()) {
                    text += ev.message + "\n";
                } else if (ev.kind == "raw" && !ev.message.empty()) {
                    text += ev.message + "\n";
                }
            }
            if (text.empty()) {
                text = outcome.raw_output.empty()
                           ? ("(无输出，exitCode=" + std::to_string(outcome.exit_code) + ")")
                           : outcome.raw_output;
            }
            self->subos_exec_output = std::move(text);
            self->running_subos_exec = false;
            if (outcome.ok) {
                toast.Show("命令执行完成 (exit 0)");
            } else {
                toast.Show("命令退出: " + outcome.error_message);
            }
        });
    }

    template <class WorkerFn, class AfterFn>
    void ExecuteOperation(
        TaskScope tasks,
        ToastHandle toast,
        std::string title,
        WorkerFn worker_fn,
        AfterFn after_fn
    ) {
        if (task_running.Get()) {
            toast.Show("已有任务正在执行，请等待完成或先取消");
            return;
        }

        auto ctrl = std::make_shared<ActiveProcessControl>();
        active_process_ = ctrl;
        task_running = true;
        task_title = title;
        task_last_status = "运行中: " + title;
        task_last_failed = false;
        task_progress = std::optional<float>{};
        AppendConsoleLine("[开始] " + title);

        std::weak_ptr<XlingsStore> weak_self = shared_from_this();
        auto self = shared_from_this();

        tasks.Launch([self, weak_self, tasks, toast, title = std::move(title), ctrl,
                      worker_fn = std::move(worker_fn), after_fn = std::move(after_fn)]() mutable -> Task<void> {
            const CommandOutcome outcome = co_await RunWorker([weak_self, tasks, ctrl, worker_fn = std::move(worker_fn)]() {
                auto emit_event = [weak_self, tasks](const NdjsonEvent& ev) {
                    std::string line;
                    std::optional<float> pct = ev.progress_percent;
                    if (ev.kind == "log") {
                        line = ev.level.empty() ? ev.message : ("[" + ev.level + "] " + ev.message);
                    } else if (ev.kind == "error") {
                        line = "[错误] " + ev.message;
                    } else if (ev.kind == "progress") {
                        if (!ev.message.empty()) line = "[进度] " + ev.message;
                    } else if (ev.kind == "data" && ev.data_kind == "tip") {
                        if (!ev.message.empty()) line = "[提示] " + ev.message;
                    } else if (ev.kind == "data" && ev.data_kind == "subos_exec_output") {
                        line = ev.message;
                    } else if (ev.kind == "data" && ev.data_kind == "remove_plan") {
                        if (const auto p = ParseRemovePlanEvents({ev}); p.has_value()) {
                            line = "[清理计划] " + p->name + ": 移除 " + std::to_string(p->remove.size()) +
                                   " 个旧版本 (" + FormatBytes(p->total_bytes) + ")，保留 " +
                                   std::to_string(p->keep.size()) + " 个";
                        }
                    } else if (ev.kind == "raw") {
                        line = ev.message;
                    }
                    if (!line.empty() || pct.has_value()) {
                        tasks.Post([weak_self, line = std::move(line), pct]() {
                            if (auto s = weak_self.lock()) {
                                if (pct.has_value()) s->task_progress = pct;
                                if (!line.empty()) s->AppendConsoleLine(line);
                            }
                        });
                    }
                };

                auto emit_line = [weak_self, tasks](const std::string& raw_line) {
                    if (raw_line.empty()) return;
                    tasks.Post([weak_self, raw_line]() {
                        if (auto s = weak_self.lock()) {
                            s->AppendConsoleLine(raw_line);
                        }
                    });
                };

                return worker_fn(emit_event, emit_line, ctrl);
            });

            self->active_process_.reset();
            self->task_running = false;
            self->task_progress = std::optional<float>{};

            if (outcome.ok) {
                self->task_last_failed = false;
                self->task_last_status = "完成: " + title;
                self->AppendConsoleLine("[完成] " + title);
                toast.Show("完成: " + title);
            } else {
                self->task_last_failed = true;
                self->task_last_status = "失败: " + title + " — " + outcome.error_message;
                self->AppendConsoleLine("[失败] " + outcome.error_message);
                toast.Show("失败: " + outcome.error_message);
            }

            after_fn(self, tasks, outcome);
        });
    }

    void InstallPackageAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string target,
        bool global,
        bool activate_after_install,
        std::string subos,
        bool reconfig
    ) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "安装 " + target,
            [client, target, global, activate_after_install, subos, reconfig](auto on_ev, auto, auto ctrl) {
                return client.InstallPackages(
                    {target}, global, activate_after_install, subos, reconfig, std::move(on_ev), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->updatable_plans = std::vector<UpdatablePackagePlan>{};
                s->has_scanned_updates = false;
                s->expired_plans = std::vector<ExpiredPackagePlan>{};
                s->has_scanned_expired = false;
                s->RefreshAll(t);
            }
        );
    }

    void RemovePackageAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string target,
        std::string older_than = {},
        bool dry_run = false
    ) {
        const XlingsClient client = client_;
        const std::string label = older_than.empty()
                                      ? ("卸载 " + target)
                                      : ((dry_run ? "预演清理旧版本 " : "清理旧版本 ") + target);
        ExecuteOperation(
            tasks,
            toast,
            label,
            [client, target, older_than, dry_run](auto on_ev, auto, auto ctrl) {
                return client.RemovePackage(target, older_than, dry_run, std::move(on_ev), std::move(ctrl));
            },
            [dry_run](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                if (!dry_run) {
                    s->expired_plans = std::vector<ExpiredPackagePlan>{};
                    s->has_scanned_expired = false;
                    s->selected_remove_plan = std::optional<RemovePlan>{};
                    s->RefreshAll(t);
                }
            }
        );
    }

    void PreviewSingleRemoveOlderThan(TaskScope tasks, std::string pkg_key, std::string older_than) {
        if (pkg_key.empty() || older_than.empty()) return;
        loading_remove_plan = true;
        selected_remove_plan = std::optional<RemovePlan>{};

        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client, pkg_key = std::move(pkg_key), older_than = std::move(older_than)]() -> Task<void> {
            auto plan = co_await RunWorker([client, pkg_key, older_than]() {
                return client.PlanRemoveOlderThan(pkg_key, older_than);
            });
            if (self->selected_installed_key.Get() == pkg_key) {
                if (!plan.has_value()) {
                    plan = RemovePlan{
                        .name = SplitPackageSpec(pkg_key).name,
                        .older_than = older_than,
                        .dry_run = true,
                    };
                }
                self->selected_remove_plan = std::move(plan);
                self->loading_remove_plan = false;
            }
        });
    }

    void ScanExpiredVersions(TaskScope tasks) {
        if (scanning_expired.Get()) return;
        scanning_expired = true;

        const bool already_loaded = initial_loaded.Get();
        std::vector<PkgGroup> candidates;
        for (const auto& g : installed_groups.Get()) {
            if (g.versions.size() > 1) candidates.push_back(g);
        }

        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client, already_loaded, candidates = std::move(candidates)]() mutable -> Task<void> {
            auto results = co_await RunWorker([client, already_loaded, candidates = std::move(candidates)]() mutable {
                if (candidates.empty() && !already_loaded) {
                    for (const auto& g : GroupInstalledPackages(client.ListInstalledPackages())) {
                        if (g.versions.size() > 1) candidates.push_back(g);
                    }
                }
                std::vector<ExpiredPackagePlan> out;
                for (const auto& g : candidates) {
                    std::string cutoff = g.active_version;
                    bool cutoff_is_active = !cutoff.empty();

                    const auto inst_vers = client.ListInstalledVersions(g.name);
                    std::string detected_active;
                    for (const auto& iv : inst_vers) {
                        if (!iv.active || iv.version.empty()) continue;
                        const std::size_t colon = iv.version.find(':');
                        std::string candidate_ver;
                        if (colon == std::string::npos) {
                            candidate_ver = iv.version;
                        } else if (iv.version.substr(0, colon) == g.ns) {
                            candidate_ver = iv.version.substr(colon + 1);
                        }
                        if (!candidate_ver.empty() &&
                            (detected_active.empty() || CompareVersionStrings(candidate_ver, detected_active) > 0)) {
                            detected_active = std::move(candidate_ver);
                        }
                    }
                    if (!detected_active.empty()) {
                        cutoff = std::move(detected_active);
                        cutoff_is_active = true;
                    }

                    if (cutoff.empty()) {
                        cutoff_is_active = false;
                        for (const auto& v : g.versions) {
                            if (!v.version.empty() &&
                                (cutoff.empty() || CompareVersionStrings(v.version, cutoff) > 0)) {
                                cutoff = v.version;
                            }
                        }
                    }
                    if (cutoff.empty()) continue;

                    auto plan = client.PlanRemoveOlderThan(g.key, cutoff);
                    if (plan.has_value() && (!plan->remove.empty() || !plan->keep.empty())) {
                        out.push_back(ExpiredPackagePlan{
                            .key = g.key,
                            .ns = g.ns,
                            .name = g.name,
                            .cutoff_version = std::move(cutoff),
                            .cutoff_is_active = cutoff_is_active,
                            .installed_count = g.versions.size(),
                            .plan = std::move(*plan),
                        });
                    }
                }

                std::ranges::sort(out, [](const ExpiredPackagePlan& a, const ExpiredPackagePlan& b) {
                    if (a.plan.total_bytes != b.plan.total_bytes) return a.plan.total_bytes > b.plan.total_bytes;
                    if (a.plan.remove.size() != b.plan.remove.size()) return a.plan.remove.size() > b.plan.remove.size();
                    return a.key < b.key;
                });
                return out;
            });

            self->expired_plans = std::move(results);
            self->scanning_expired = false;
            self->has_scanned_expired = true;
        });
    }

    void BatchRemoveExpiredAsync(TaskScope tasks, ToastHandle toast, std::vector<ExpiredPackagePlan> targets) {
        std::vector<RemovalStep> steps;
        std::size_t total_vers = 0;
        for (const auto& t : targets) {
            if (t.plan.remove.empty() || t.cutoff_version.empty()) continue;
            total_vers += t.plan.remove.size();
            steps.push_back(RemovalStep{
                .target = t.key,
                .older_than = t.cutoff_version,
                .progress_message = "正在清理 " + t.key + " 早于 " + t.cutoff_version + " 的旧版本...",
            });
        }
        if (steps.empty()) {
            toast.Show("没有可清理的过期版本");
            return;
        }
        const std::string title =
            "批量清理过期版本 (" + std::to_string(steps.size()) + " 个包 · " + std::to_string(total_vers) + " 个版本)";
        ExecuteBatchRemovalSteps(tasks, toast, title, std::move(steps));
    }

    void BatchRemoveTargetsAsync(TaskScope tasks, ToastHandle toast, std::vector<std::string> full_targets) {
        if (full_targets.empty()) return;
        std::vector<RemovalStep> steps;
        steps.reserve(full_targets.size());
        for (auto& target : full_targets) {
            std::string msg = "正在卸载 " + target + "...";
            steps.push_back(RemovalStep{
                .target = std::move(target),
                .older_than = {},
                .progress_message = std::move(msg),
            });
        }
        const std::string title = "批量卸载 " + std::to_string(steps.size()) + " 个指定版本";
        ExecuteBatchRemovalSteps(tasks, toast, title, std::move(steps));
    }

    void ScanUpdatablePackages(TaskScope tasks) {
        if (scanning_updates.Get()) return;
        scanning_updates = true;

        const bool already_loaded = initial_loaded.Get();
        std::vector<PkgGroup> groups = installed_groups.Get();

        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client, already_loaded, groups = std::move(groups)]() mutable -> Task<void> {
            auto results = co_await RunWorker([client, already_loaded, groups = std::move(groups)]() mutable {
                if (groups.empty() && !already_loaded) {
                    groups = GroupInstalledPackages(client.ListInstalledPackages());
                }
                std::vector<std::string> targets;
                targets.reserve(groups.size());
                for (const auto& g : groups) {
                    if (g.key.empty()) continue;
                    if (std::ranges::find(targets, g.key) == targets.end()) {
                        targets.push_back(g.key);
                    }
                }
                if (targets.empty()) return std::vector<UpdatablePackagePlan>{};
                const auto statuses = client.QueryPayloadStatus(targets);
                return BuildUpdatablePlans(groups, statuses);
            });

            self->updatable_plans = std::move(results);
            self->scanning_updates = false;
            self->has_scanned_updates = true;
        });
    }

    void SyncIndexAndScanUpdatesAsync(TaskScope tasks, ToastHandle toast) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "同步包索引并检查更新",
            [client](auto on_ev, auto, auto ctrl) {
                return client.UpdatePackages({}, std::move(on_ev), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->updatable_plans = std::vector<UpdatablePackagePlan>{};
                s->has_scanned_updates = false;
                s->RefreshAll(t);
                s->ScanUpdatablePackages(t);
            }
        );
    }

    void BatchUpdatePackagesAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::vector<UpdatablePackagePlan> targets,
        bool activate_updated
    ) {
        struct UpdateStep {
            std::string key;
            std::string spec;
            bool activate = false;
            std::string progress_message;
        };

        std::vector<UpdateStep> steps;
        steps.reserve(targets.size());
        for (const auto& t : targets) {
            if (t.is_skipped || t.key.empty() || t.latest_version.empty()) continue;
            const std::string spec = t.key + "@" + t.latest_version;
            const bool activate =
                activate_updated &&
                !t.active_version.empty() &&
                (!t.is_revision_only || t.active_version == t.latest_version);
            std::string msg;
            if (t.is_revision_only) {
                msg = "正在更新修订 " + spec + " (r" + std::to_string(t.installed_revision) +
                      " → r" + std::to_string(t.latest_revision) + ")...";
            } else {
                msg = "正在更新 " + t.key + " (" +
                      (t.current_version.empty() ? std::string("?") : t.current_version) +
                      " → " + t.latest_version + ")...";
            }
            steps.push_back(UpdateStep{
                .key = t.key,
                .spec = spec,
                .activate = activate,
                .progress_message = std::move(msg),
            });
        }

        if (steps.empty()) {
            toast.Show("没有可更新的软件包");
            return;
        }

        const XlingsClient client = client_;
        const std::string title = "批量更新软件包 (" + std::to_string(steps.size()) + " 个包)";
        const std::size_t count = steps.size();
        ExecuteBatchSteps(
            tasks,
            toast,
            title,
            count,
            [client, steps = std::move(steps)](std::size_t i, auto& emit_progress, auto& on_ev, auto& ctrl) {
                emit_progress(steps[i].progress_message);
                const CommandOutcome out = client.InstallPackages(
                    {steps[i].spec}, false, steps[i].activate, {}, false, on_ev, ctrl
                );
                return std::pair<std::string, CommandOutcome>{steps[i].key, out};
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->updatable_plans = std::vector<UpdatablePackagePlan>{};
                s->has_scanned_updates = false;
                s->expired_plans = std::vector<ExpiredPackagePlan>{};
                s->has_scanned_expired = false;
                s->RefreshAll(t);
            }
        );
    }

    void ActivateVersionAsync(TaskScope tasks, ToastHandle toast, std::string plain_name, std::string version) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "切换 " + plain_name + " -> " + version,
            [client, plain_name, version](auto on_ev, auto, auto ctrl) {
                return client.ActivateVersion(plain_name, version, std::move(on_ev), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

    void UpdatePackagesAsync(TaskScope tasks, ToastHandle toast, std::string target = {}) {
        const XlingsClient client = client_;
        const std::string label = target.empty() ? "更新索引与软件包" : ("更新 " + target);
        ExecuteOperation(
            tasks,
            toast,
            label,
            [client, target](auto on_ev, auto, auto ctrl) {
                return client.UpdatePackages(target, std::move(on_ev), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->updatable_plans = std::vector<UpdatablePackagePlan>{};
                s->has_scanned_updates = false;
                s->RefreshAll(t);
            }
        );
    }

    void CreateSubosAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string custom_dir) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "新建 SubOS: " + name,
            [client, name, custom_dir](auto on_ev, auto, auto ctrl) {
                return client.CreateSubos(name, custom_dir, std::move(on_ev), std::move(ctrl));
            },
            [name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome& out) {
                if (out.ok) s->selected_subos = name;
                s->RefreshAll(t);
            }
        );
    }

    void SwitchSubosAsync(TaskScope tasks, ToastHandle toast, std::string name) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "切换 SubOS -> " + name,
            [client, name](auto on_ev, auto, auto ctrl) {
                return client.SwitchSubos(name, std::move(on_ev), std::move(ctrl));
            },
            [name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome& out) {
                if (out.ok) s->selected_subos = name;
                s->RefreshAll(t);
            }
        );
    }

    void RemoveSubosAsync(TaskScope tasks, ToastHandle toast, std::string name) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "删除 SubOS: " + name,
            [client, name](auto on_ev, auto, auto ctrl) {
                return client.RemoveSubos(name, std::move(on_ev), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

    void AddRepoAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string url) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "添加/更新仓库: " + name,
            [client, name, url](auto on_ev, auto, auto ctrl) {
                return client.AddRepo(name, url, std::move(on_ev), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

    void RemoveRepoAsync(TaskScope tasks, ToastHandle toast, std::string name) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "移除仓库: " + name,
            [client, name](auto on_ev, auto, auto ctrl) {
                return client.RemoveRepo(name, std::move(on_ev), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

    void SyncIndexAsync(TaskScope tasks, ToastHandle toast, std::string name) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "同步索引: " + name,
            [client, name](auto on_ev, auto, auto ctrl) {
                return client.SyncIndex(name, std::move(on_ev), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

    void PinIndexVersionAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string version) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "切换索引快照 " + name + "@" + version,
            [client, name, version](auto, auto on_line, auto ctrl) {
                return client.PinIndexVersion(name, version, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

    void AddLocalXpkgAsync(TaskScope tasks, ToastHandle toast, std::string path) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "添加本地 xpkg",
            [client, path](auto, auto on_line, auto ctrl) {
                return client.AddLocalXpkg(path, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

    void RemoveLocalXpkgAsync(TaskScope tasks, ToastHandle toast, std::string name) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "移除本地 xpkg: " + name,
            [client, name](auto, auto on_line, auto ctrl) {
                return client.RemoveLocalXpkg(name, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

    void SetMirrorAsync(TaskScope tasks, ToastHandle toast, std::string mirror) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "设置镜像源 -> " + mirror,
            [client, mirror](auto, auto on_line, auto ctrl) {
                return client.SetConfigMirror(mirror, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

    void SetLangAsync(TaskScope tasks, ToastHandle toast, std::string lang) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "设置语言 -> " + lang,
            [client, lang](auto, auto on_line, auto ctrl) {
                return client.SetConfigLang(lang, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

    void SetUiModeAsync(TaskScope tasks, ToastHandle toast, std::string mode) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "设置 CLI 界面模式 -> " + mode,
            [client, mode](auto, auto on_line, auto ctrl) {
                return client.SetConfigUiMode(mode, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

    void SetInteractiveAsync(TaskScope tasks, ToastHandle toast, bool enabled) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            std::string("设置交互模式 -> ") + (enabled ? "开启" : "关闭"),
            [client, enabled](auto, auto on_line, auto ctrl) {
                return client.SetConfigInteractive(enabled, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

    void RunDoctorAsync(TaskScope tasks, ToastHandle toast, bool deep, bool fix, bool dry_run, bool isolation) {
        const XlingsClient client = client_;
        std::string title = "环境诊断 (self doctor)";
        if (fix && dry_run) title = "预演修复 (doctor --fix --dry-run)";
        else if (fix) title = "执行修复 (doctor --fix)";
        else if (isolation) title = "沙箱隔离检查 (doctor --isolation)";
        else if (deep) title = "深度诊断 (doctor --deep)";

        ExecuteOperation(
            tasks,
            toast,
            title,
            [client, deep, fix, dry_run, isolation](auto, auto on_line, auto ctrl) {
                return client.RunDoctor(deep, fix, dry_run, isolation, std::move(on_line), std::move(ctrl));
            },
            [fix, dry_run](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome& out) {
                s->doctor_output = out.raw_output;
                if (fix && !dry_run) s->RefreshAll(t);
            }
        );
    }

    void RunSelfCleanAsync(TaskScope tasks, ToastHandle toast, bool dry_run) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            dry_run ? "缓存清理预演 (self clean --dry-run)" : "清理缓存 (self clean)",
            [client, dry_run](auto, auto on_line, auto ctrl) {
                return client.RunSelfClean(dry_run, std::move(on_line), std::move(ctrl));
            },
            [dry_run](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome& out) {
                s->clean_output = out.raw_output;
                if (!dry_run) s->RefreshAll(t);
            }
        );
    }

    void RunSelfUpdateAsync(TaskScope tasks, ToastHandle toast) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "更新 xlings 自身 (self update)",
            [client](auto, auto on_line, auto ctrl) {
                return client.RunSelfUpdate(std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

    void CommitProfileAsync(TaskScope tasks, ToastHandle toast, std::string reason) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "提交 Profile 代际快照",
            [client, reason](auto, auto on_line, auto ctrl) {
                return client.CommitProfile(reason, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

    void RollbackProfileAsync(TaskScope tasks, ToastHandle toast, std::string generation) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "回滚 Profile -> " + generation,
            [client, generation](auto, auto on_line, auto ctrl) {
                return client.RollbackProfile(generation, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshAll(t);
            }
        );
    }

private:
    struct RemovalStep {
        std::string target;
        std::string older_than;
        std::string progress_message;
    };

    template <class StepFn, class AfterFn>
    void ExecuteBatchSteps(
        TaskScope tasks,
        ToastHandle toast,
        std::string title,
        std::size_t step_count,
        StepFn step_fn,
        AfterFn after_fn
    ) {
        if (step_count == 0) return;
        ExecuteOperation(
            tasks,
            toast,
            std::move(title),
            [step_count, step_fn = std::move(step_fn)](auto on_ev, auto, auto ctrl) -> CommandOutcome {
                CommandOutcome combined;
                std::size_t succeeded = 0;
                std::size_t failed = 0;
                bool cancelled = false;
                std::string last_error;

                for (std::size_t i = 0; i < step_count; ++i) {
                    if (ctrl && ctrl->cancel_requested.load(std::memory_order_acquire)) {
                        cancelled = true;
                        break;
                    }
                    if (ctrl) ctrl->pid.store(-1, std::memory_order_release);

                    auto emit_progress = [&](const std::string& msg) {
                        NdjsonEvent prog;
                        prog.kind = "progress";
                        prog.progress_percent = static_cast<float>(i) / static_cast<float>(step_count);
                        prog.message = msg;
                        on_ev(prog);
                    };

                    const auto [target_label, step_out] = step_fn(i, emit_progress, on_ev, ctrl);
                    if (ctrl && ctrl->cancel_requested.load(std::memory_order_acquire)) {
                        cancelled = true;
                        if (step_out.ok) {
                            ++succeeded;
                        } else {
                            last_error = step_out.error_message;
                        }
                        break;
                    }
                    if (step_out.ok) {
                        ++succeeded;
                    } else {
                        ++failed;
                        if (!step_out.error_message.empty()) {
                            last_error = target_label + ": " + step_out.error_message;
                        } else {
                            last_error = target_label + ": exit code " + std::to_string(step_out.exit_code);
                        }
                    }
                }

                combined.ok = (failed == 0 && !cancelled);
                combined.exit_code = combined.ok ? 0 : 1;
                if (cancelled) {
                    combined.error_message =
                        "已取消 (已完成 " + std::to_string(succeeded) + "/" + std::to_string(step_count) + ")";
                } else if (failed > 0) {
                    combined.error_message =
                        "完成 " + std::to_string(succeeded) + " 项，" + std::to_string(failed) +
                        " 项失败" + (last_error.empty() ? std::string{} : (" (" + last_error + ")"));
                }
                return combined;
            },
            std::move(after_fn)
        );
    }

    void ExecuteBatchRemovalSteps(
        TaskScope tasks,
        ToastHandle toast,
        std::string title,
        std::vector<RemovalStep> steps
    ) {
        if (steps.empty()) return;
        const XlingsClient client = client_;
        const std::size_t count = steps.size();
        ExecuteBatchSteps(
            tasks,
            toast,
            std::move(title),
            count,
            [client, steps = std::move(steps)](std::size_t i, auto& emit_progress, auto& on_ev, auto& ctrl) {
                emit_progress(steps[i].progress_message);
                const CommandOutcome step_out = client.RemovePackage(
                    steps[i].target, steps[i].older_than, false, on_ev, ctrl
                );
                return std::pair<std::string, CommandOutcome>{steps[i].target, step_out};
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->expired_plans = std::vector<ExpiredPackagePlan>{};
                s->has_scanned_expired = false;
                s->selected_remove_plan = std::optional<RemovePlan>{};
                s->RefreshAll(t);
            }
        );
    }

    XlingsClient client_;
    std::shared_ptr<ActiveProcessControl> active_process_;
};

} // namespace hxlings

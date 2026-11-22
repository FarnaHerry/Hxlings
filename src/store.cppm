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
    State<bool> loading_installed_detail{false};

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
        loading_installed_detail = true;

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
                if (!dry_run) s->RefreshAll(t);
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
    XlingsClient client_;
    std::shared_ptr<ActiveProcessControl> active_process_;
};

} // namespace hxlings

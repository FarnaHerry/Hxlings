export module hxlings.store;

import std;
import mcpplibs.os;
import huxerui;
import xlings.types;
import xlings.parser;
import xlings.process;
import xlings.client;
import hxlings.theme;
import hxlings.settings;
import hxlings.updater;
import hxlings.single_instance;
import hxlings.luban.types;
import hxlings.luban.client;
import hxlings.luban.stack;

using namespace huxerui;

export namespace hxlings {

struct CoreEnvSnapshot {
    XlingsVersionInfo version_info;
    XlingsEnv env;
};

struct CorePackagesSnapshot {
    std::vector<InstalledPkg> installed_raw;
    std::vector<PkgGroup> installed_groups;
};

struct CoreSubosSnapshot {
    std::vector<SubosEntry> subos_list;
    std::vector<std::string> active_shims;
};

struct CoreReposLogsSnapshot {
    std::vector<RepoEntry> repos;
    std::vector<SelfLogEntry> recent_logs;
};

struct IndexesLocalSnapshot {
    std::vector<RepoEntry> repos;
    std::vector<LocalXpkg> local_xpkgs;
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

struct LubanOverviewSnapshot {
    bool luban_available = false;
    bool luban_os_available = false;
    std::string luban_binary_path;
    std::string luban_os_binary_path;
    LubanHostStatus host_status;
    std::vector<LubanEnvironment> envs;
    LubanCatalog catalog;
};

struct LubanDetailSnapshot {
    std::string name;
    std::optional<LubanEnvStatusDetail> status;
    std::optional<LubanEnvConfigDetail> config;
    std::string history;
    std::string logs;
};

class XlingsStore : public std::enable_shared_from_this<XlingsStore> {
public:
    XlingsStore() = default;

    bool skip_initial_refresh = false;

    // Client Preferences (decoupled from xlings CLI global config)
    State<ThemeMode> theme_mode{ThemeMode::Auto};
    State<bool> dark_mode{false};
    State<Color> brand_color{DefaultBrandColor()};
    State<AppSettings> app_settings{AppSettings{}};
    State<std::size_t> active_nav{0};
    State<std::size_t> settings_tab{0};
    State<bool> console_expanded{false};
    bool force_quit_requested = false;
    bool tray_activate_registered = false;
    bool initial_refresh_started = false;
    std::optional<WindowHandle> main_window;
    std::optional<ToastHandle> active_toast;
    std::function<void()> on_tray_state_changed;
    std::function<void(ThemeMode, bool)> on_theme_changed;

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
    State<std::string> search_error{std::string{}};
    State<std::string> selected_store_pkg{std::string{}};
    State<std::optional<PackageDetail>> selected_store_detail{std::optional<PackageDetail>{}};
    State<bool> loading_store_detail{false};
    State<std::vector<InstallPlanItem>> store_plan_preview{std::vector<InstallPlanItem>{}};

    // Shortcut & Focus Requests
    State<std::uint64_t> focus_search_nonce{0};
    State<std::uint64_t> open_batch_update_nonce{0};
    State<std::uint64_t> open_batch_cleanup_nonce{0};

    // SubOS
    State<std::vector<SubosEntry>> subos_list{std::vector<SubosEntry>{}};
    State<std::string> selected_subos{std::string{}};
    State<std::vector<std::string>> active_shims{std::vector<std::string>{}};
    State<std::vector<SubosEnvVar>> selected_subos_env{std::vector<SubosEnvVar>{}};
    State<std::vector<SubosEvent>> selected_subos_events{std::vector<SubosEvent>{}};
    State<std::string> subos_exec_output{std::string{}};
    State<bool> loading_subos_detail{false};
    State<bool> running_subos_exec{false};

    // LubanOS (lazy loaded when entering active_nav == 4)
    State<bool> luban_available{false};
    State<bool> luban_os_available{false};
    State<bool> luban_loaded{false};
    State<bool> loading_luban{false};
    State<std::size_t> luban_tab{0};
    State<LubanHostStatus> luban_host_status{LubanHostStatus{}};
    State<std::vector<LubanEnvironment>> luban_envs{std::vector<LubanEnvironment>{}};
    State<LubanCatalog> luban_catalog{LubanCatalog{}};
    State<std::string> selected_luban_env{std::string{}};
    State<std::optional<LubanEnvStatusDetail>> selected_luban_status{std::optional<LubanEnvStatusDetail>{}};
    State<std::optional<LubanEnvConfigDetail>> selected_luban_config{std::optional<LubanEnvConfigDetail>{}};
    State<std::string> selected_luban_history{std::string{}};
    State<std::string> selected_luban_logs{std::string{}};
    State<bool> loading_luban_detail{false};
    State<std::string> luban_run_output{std::string{}};
    State<bool> running_luban_cmd{false};
    State<std::map<std::string, LubanEnvConfigDetail>> luban_config_cache{std::map<std::string, LubanEnvConfigDetail>{}};
    State<std::vector<SavedLubanStackInfo>> saved_luban_stacks{std::vector<SavedLubanStackInfo>{}};

    // Indexes & Local Recipes
    State<std::vector<RepoEntry>> repos{std::vector<RepoEntry>{}};
    State<std::vector<IndexRepoVersions>> index_versions{std::vector<IndexRepoVersions>{}};
    State<std::vector<LocalXpkg>> local_xpkgs{std::vector<LocalXpkg>{}};
    State<bool> loading_indexes{false};
    State<bool> indexes_loaded{false};
    State<bool> loading_index_versions{false};
    State<bool> index_versions_loaded{false};

    // Config, Doctor & Profiles
    State<ConfigSnapshot> config{ConfigSnapshot{}};
    State<std::vector<ProfileGeneration>> profiles{std::vector<ProfileGeneration>{}};
    State<bool> loading_xlings_config{false};
    State<bool> xlings_config_loaded{false};
    State<bool> loading_profiles{false};
    State<bool> profiles_loaded{false};
    State<std::string> doctor_output{std::string{}};
    State<std::string> clean_output{std::string{}};
    State<std::string> last_opened_url{std::string{}};
    State<SubosStatusSnapshot> selected_subos_status{SubosStatusSnapshot{}};
    State<SubosPolicySnapshot> selected_subos_policy{SubosPolicySnapshot{}};
    State<std::vector<SubosSessionItem>> selected_subos_sessions{std::vector<SubosSessionItem>{}};
    State<std::vector<SubosRequestItem>> selected_subos_requests{std::vector<SubosRequestItem>{}};
    State<SubosReportSnapshot> selected_subos_report{SubosReportSnapshot{}};
    State<std::string> selected_subos_info{std::string{}};
    State<SubosDoctorSnapshot> subos_doctor_snapshot{SubosDoctorSnapshot{}};
    State<bool> loading_subos_governance{false};
    State<std::string> subos_diff_output{std::string{}};
    State<bool> loading_subos_diff{false};
    State<std::string> subos_rollback_list_output{std::string{}};
    State<std::vector<ConfigThemeItem>> config_themes{std::vector<ConfigThemeItem>{}};
    State<bool> loading_config_themes{false};
    State<std::vector<NetworkEndpoint>> network_endpoints{std::vector<NetworkEndpoint>{}};
    State<std::optional<NetworkProbeResult>> network_probe_result{std::optional<NetworkProbeResult>{}};
    State<bool> loading_network{false};
    State<bool> probing_url{false};
    State<std::string> probing_target_url{std::string{}};
    State<bool> probing_custom_url{false};
    State<std::string> script_output{std::string{}};
    State<bool> running_script{false};
    State<std::string> loaded_subos_governance_name{std::string{}};
    State<AppUpdateState> app_update_state{AppUpdateState{}};

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

    void NotifyTrayStateChanged() const {
        if (!skip_initial_refresh && on_tray_state_changed) {
            on_tray_state_changed();
        }
    }

    void NotifyThemeChanged(ThemeMode mode, bool effective_dark) const {
        if (!skip_initial_refresh && on_theme_changed) {
            on_theme_changed(mode, effective_dark);
        }
    }

    void ShowToastSafe(std::string message, std::optional<ToastHandle> fallback = std::nullopt) const {
        if (message.empty()) return;
        if (main_window.has_value() && !main_window->IsOpen()) {
            return;
        }
        const auto target = active_toast.has_value() ? active_toast : fallback;
        if (!target.has_value()) {
            return;
        }
        try {
            target->Show(std::move(message));
        } catch (...) {
        }
    }

    bool ConsumeFocusSearchNonce() {
        const std::uint64_t val = focus_search_nonce.Get();
        if (val <= consumed_focus_search_nonce_) return false;
        consumed_focus_search_nonce_ = val;
        return true;
    }

    bool ConsumeBatchUpdateNonce() {
        const std::uint64_t val = open_batch_update_nonce.Get();
        if (val <= consumed_batch_update_nonce_) return false;
        consumed_batch_update_nonce_ = val;
        return true;
    }

    bool ConsumeBatchCleanupNonce() {
        const std::uint64_t val = open_batch_cleanup_nonce.Get();
        if (val <= consumed_batch_cleanup_nonce_) return false;
        consumed_batch_cleanup_nonce_ = val;
        return true;
    }

    void CancelCurrentOperation() {
        if (active_process_) {
            active_process_->Cancel();
            AppendConsoleLine("[操作] 已请求取消当前任务...");
        }
    }

    void InitAppSettings(const AppSettings& loaded) {
        ApplyAppSettings(loaded, false);
    }

    void ApplyAppSettings(AppSettings next, bool persist = true) {
        const auto presets = BrandPresets();
        const int max_idx = static_cast<int>(presets.size()) - 1;
        next.brand_preset_index = std::clamp(next.brand_preset_index, 0, max_idx);
        app_settings = next;
        theme_mode = next.theme_mode;
        if (next.theme_mode == ThemeMode::Dark) {
            dark_mode = true;
        } else if (next.theme_mode == ThemeMode::Light) {
            dark_mode = false;
        }
        brand_color = presets[static_cast<std::size_t>(next.brand_preset_index)];
        if (persist && !skip_initial_refresh) {
            (void)SaveAppSettings(next);
        }
        NotifyThemeChanged(next.theme_mode, dark_mode.Get());
        NotifyTrayStateChanged();
    }

    void SetThemeMode(ThemeMode mode) {
        AppSettings next = app_settings.Get();
        next.theme_mode = mode;
        ApplyAppSettings(std::move(next), true);
    }

    void SetDarkMode(bool enabled) {
        SetThemeMode(enabled ? ThemeMode::Dark : ThemeMode::Light);
    }

    void ToggleEffectiveDark(bool current_effective_dark) {
        SetThemeMode(current_effective_dark ? ThemeMode::Light : ThemeMode::Dark);
    }

    void SetBrandPresetIndex(int index) {
        AppSettings next = app_settings.Get();
        next.brand_preset_index = index;
        ApplyAppSettings(std::move(next), true);
    }

    void SetTrayEnabled(bool enabled) {
        AppSettings next = app_settings.Get();
        next.tray_enabled = enabled;
        ApplyAppSettings(std::move(next), true);
    }

    void SetCloseToTray(bool enabled) {
        AppSettings next = app_settings.Get();
        next.close_to_tray = enabled;
        if (!enabled) {
            next.lightweight_mode = false;
        }
        ApplyAppSettings(std::move(next), true);
    }

    void SetLightweightMode(bool enabled) {
        AppSettings next = app_settings.Get();
        next.lightweight_mode = enabled;
        if (enabled && next.tray_enabled) {
            next.close_to_tray = true;
        }
        ApplyAppSettings(std::move(next), true);
    }

    void RefreshCore(TaskScope tasks, bool force = false);

    void EnsureInstalledDetailLoaded(TaskScope tasks, bool force = false);

    void EnsureSubosDetailLoaded(TaskScope tasks, bool force = false);

    void RefreshIndexes(TaskScope tasks, bool force = false);

    void RefreshXlingsConfig(TaskScope tasks, bool force = false);

    void RefreshProfiles(TaskScope tasks, bool force = false);

    void RefreshAll(TaskScope tasks);

    void LoadInstalledDetail(TaskScope tasks, const PkgGroup& group);

    void RequestFocusSearch() {
        focus_search_nonce = focus_search_nonce.Get() + 1;
    }

    void RequestOpenBatchUpdate() {
        active_nav = 1;
        open_batch_update_nonce = open_batch_update_nonce.Get() + 1;
    }

    void RequestOpenBatchCleanup() {
        active_nav = 1;
        open_batch_cleanup_nonce = open_batch_cleanup_nonce.Get() + 1;
    }

    void RunSearch(TaskScope tasks, std::string keyword, std::string category = {});

    void LoadStoreDetail(TaskScope tasks, const std::string& package_target);

    void PreviewInstallPlan(TaskScope tasks, std::vector<std::string> targets, bool global, bool for_store);

    void LoadSubosDetail(TaskScope tasks, const std::string& subos_name, const std::string& event_kind);

    void RunSubosExec(TaskScope tasks, ToastHandle toast, std::string subos_name, std::vector<std::string> argv, bool sandbox);

    template <class WorkerFn, class AfterFn>
    void ExecuteOperation(
        TaskScope tasks,
        ToastHandle toast,
        std::string title,
        WorkerFn worker_fn,
        AfterFn after_fn
    ) {
        if (task_running.Get()) {
            ShowToastSafe("已有任务正在执行，请等待完成或先取消", toast);
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
        NotifyTrayStateChanged();

        std::weak_ptr<XlingsStore> weak_self = shared_from_this();
        auto self = shared_from_this();

        active_operation_task_ = tasks.Launch([self, weak_self, tasks, toast, title = std::move(title), ctrl,
                      worker_fn = std::move(worker_fn), after_fn = std::move(after_fn)]() mutable -> Task<void> {
            const CommandOutcome outcome = co_await self->mutation_sequence_.Run(
                [weak_self, tasks, ctrl, worker_fn = std::move(worker_fn)](std::stop_token stop) {
                    std::stop_callback on_stop(stop, [ctrl] {
                        if (ctrl) ctrl->Cancel();
                    });
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
                }
            );

            self->active_process_.reset();
            self->task_running = false;
            self->task_progress = std::optional<float>{};

            if (outcome.ok) {
                self->task_last_failed = false;
                self->task_last_status = "完成: " + title;
                self->AppendConsoleLine("[完成] " + title);
                self->ShowToastSafe("完成: " + title, toast);
            } else {
                self->task_last_failed = true;
                self->task_last_status = "失败: " + title + " — " + outcome.error_message;
                self->AppendConsoleLine("[失败] " + outcome.error_message);
                self->ShowToastSafe("失败: " + outcome.error_message, toast);
            }
            self->NotifyTrayStateChanged();

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
    );

    void RemovePackageAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string target,
        std::string older_than = {},
        bool dry_run = false
    );

    void PreviewSingleRemoveOlderThan(TaskScope tasks, std::string pkg_key, std::string older_than);

    void ScanExpiredVersions(TaskScope tasks);

    void BatchRemoveExpiredAsync(TaskScope tasks, ToastHandle toast, std::vector<ExpiredPackagePlan> targets);

    void BatchRemoveTargetsAsync(TaskScope tasks, ToastHandle toast, std::vector<std::string> full_targets);

    void ScanUpdatablePackages(TaskScope tasks);

    void SyncIndexAndScanUpdatesAsync(TaskScope tasks, ToastHandle toast);

    void BatchUpdatePackagesAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::vector<UpdatablePackagePlan> targets,
        bool activate_updated
    );

    void ActivateVersionAsync(TaskScope tasks, ToastHandle toast, std::string plain_name, std::string version);

    void UpdatePackagesAsync(TaskScope tasks, ToastHandle toast, std::string target = {});

    void CreateSubosAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string custom_dir);

    void SwitchSubosAsync(TaskScope tasks, ToastHandle toast, std::string name);

    void RemoveSubosAsync(TaskScope tasks, ToastHandle toast, std::string name);

    void AddRepoAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string url);

    void RemoveRepoAsync(TaskScope tasks, ToastHandle toast, std::string name);

    void SyncIndexAsync(TaskScope tasks, ToastHandle toast, std::string name);

    void PinIndexVersionAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string version);

    void AddLocalXpkgAsync(TaskScope tasks, ToastHandle toast, std::string path);

    void RemoveLocalXpkgAsync(TaskScope tasks, ToastHandle toast, std::string name);

    void OpenUrl(ToastHandle toast, std::string_view raw_url) {
        const std::string url = NormalizeBrowserUrl(raw_url);
        if (url.empty()) {
            toast.Show("无效的外部链接");
            return;
        }
        last_opened_url = url;
        if (skip_initial_refresh) {
            return;
        }
        if (OpenExternalUrl(url)) {
            toast.Show("已在默认浏览器中打开链接");
        } else {
            toast.Show("无法打开浏览器: " + url);
        }
    }

    void RefreshConfigThemes(TaskScope tasks, bool force = false);

    void RefreshNetworkEndpoints(TaskScope tasks, bool force = false);

    void ProbeNetworkUrlAsync(TaskScope tasks, ToastHandle toast, std::string url, bool from_custom = false);

    void LoadSubosGovernance(TaskScope tasks, std::string subos_name, bool force = false);

    void QuerySubosRollbackListAsync(TaskScope tasks, std::string name);

    void DiffSubosAsync(TaskScope tasks, std::string lhs, std::string rhs);

    void RunSubosExecWithOptions(
        TaskScope tasks,
        ToastHandle toast,
        std::string subos_name,
        std::vector<std::string> argv,
        bool sandbox,
        std::string cwd,
        std::string timeout
    );

    void RunScriptAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string script_file,
        std::vector<std::string> script_args
    );

    void StartSubosSessionAsync(TaskScope tasks, ToastHandle toast, std::string name);

    void StopSubosSessionAsync(TaskScope tasks, ToastHandle toast, std::string name);

    void CreateSubosAdvancedAsync(TaskScope tasks, ToastHandle toast, SubosCreateOptions opts);

    void UpdateSubosConfigAsync(TaskScope tasks, ToastHandle toast, std::string name, SubosPolicyPatch patch);

    void ApproveSubosRequestAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string req_id);

    void DenySubosRequestAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string req_id);

    void RunSubosDoctorFixAsync(TaskScope tasks, ToastHandle toast, std::string name = {});

    void UpgradeSubosAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string name,
        std::string to_version,
        bool dry_run
    );

    void RollbackSubosAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string generation);

    void RebindSubosRuntimeAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string binding);

    void ExportSubosAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string name,
        std::string format,
        std::string output_path,
        std::string disk_size,
        bool with_data
    );

    void PackSubosAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string name,
        std::string as_pkg,
        std::string out_dir
    );

    void CopySubosFilesAsync(TaskScope tasks, ToastHandle toast, std::string src, std::string dst);

    // ---- LubanOS Domain ----

    void RefreshLuban(TaskScope tasks, bool force = false);

    void LoadLubanDetail(TaskScope tasks, std::string env_name, bool force = false);

    void CreateLubanEnvAsync(TaskScope tasks, ToastHandle toast, LubanCreateOptions opts);

    void ConfigureLubanEnvAsync(TaskScope tasks, ToastHandle toast, std::string name, LubanConfigPatch patch);

    void RunLubanCommandInlineAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string name,
        std::vector<std::string> cmd_argv
    );

    void StopLubanEnvAsync(TaskScope tasks, ToastHandle toast, std::string name);

    void UpgradeLubanEnvAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string name,
        std::string target_ver = {},
        bool dry_run = false
    );

    void RollbackLubanEnvAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string name,
        std::string gen = {}
    );

    void ExportLubanEnvAsync(TaskScope tasks, ToastHandle toast, LubanExportOptions opts);

    void ImportLubanEnvAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string archive_path,
        std::string custom_name = {}
    );

    void RemoveLubanEnvAsync(TaskScope tasks, ToastHandle toast, std::string name);

    void SetupLubanHostAsync(TaskScope tasks, ToastHandle toast);

    void InstallLubanComponentAsync(TaskScope tasks, ToastHandle toast, std::string pkg_spec);

    void LaunchLubanOsApp(ToastHandle toast);

    void RefreshSavedLubanStacks(TaskScope tasks);

    bool SaveLubanStackDraft(
        TaskScope tasks,
        ToastHandle toast,
        std::string name,
        std::string content,
        std::string format
    );

    void DeleteSavedLubanStack(
        TaskScope tasks,
        ToastHandle toast,
        std::filesystem::path path,
        std::string display_name
    );

    void SyncLubanConfigsForStackAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::vector<std::string> env_names
    );

    void ApplyLubanStackAsync(
        TaskScope tasks,
        ToastHandle toast,
        LubanStackSpec spec,
        bool dry_run = false
    );

    void StopLubanStackAsync(TaskScope tasks, ToastHandle toast, LubanStackSpec spec);

    void SetConfigThemeAsync(TaskScope tasks, ToastHandle toast, std::string theme_name);

    void SetConfigRuntimeAsync(TaskScope tasks, ToastHandle toast, std::string runtime_spec);

    void AddArchiveDirAsync(TaskScope tasks, ToastHandle toast, std::string dir_path);

    void RemoveArchiveDirAsync(TaskScope tasks, ToastHandle toast, std::string dir_path);

    void ClearLocalXpkgsAsync(TaskScope tasks, ToastHandle toast, std::string mode);

    void RunSelfMigrateAsync(TaskScope tasks, ToastHandle toast);

    void RunSelfInitAsync(TaskScope tasks, ToastHandle toast, std::string home_dir, bool force);

    void SetMirrorAsync(TaskScope tasks, ToastHandle toast, std::string mirror);

    void SetLangAsync(TaskScope tasks, ToastHandle toast, std::string lang);

    void SetUiModeAsync(TaskScope tasks, ToastHandle toast, std::string mode);

    void SetInteractiveAsync(TaskScope tasks, ToastHandle toast, bool enabled);

    void RunDoctorAsync(TaskScope tasks, ToastHandle toast, bool deep, bool fix, bool dry_run, bool isolation);

    void RunSelfCleanAsync(TaskScope tasks, ToastHandle toast, bool dry_run);

    void RunSelfUpdateAsync(TaskScope tasks, ToastHandle toast);

    void CommitProfileAsync(TaskScope tasks, ToastHandle toast, std::string reason);

    void RollbackProfileAsync(TaskScope tasks, ToastHandle toast, std::string generation);

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
    );

public:
    void CheckForAppUpdateAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::shared_ptr<HttpClient> http
    );

    void DownloadAppUpdateAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::shared_ptr<HttpClient> http
    );

    void CancelAppUpdate();

    void ApplyAppUpdateAsync(ToastHandle toast, ApplicationHandle application);

private:
    XlingsClient client_;
    LubanClient luban_client_;
    std::shared_ptr<ActiveProcessControl> active_process_;
    WorkerSequence mutation_sequence_;
    TaskHandle search_task_;
    TaskHandle installed_detail_task_;
    TaskHandle store_detail_task_;
    TaskHandle plan_preview_task_;
    TaskHandle remove_plan_task_;
    TaskHandle subos_detail_task_;
    TaskHandle luban_detail_task_;
    TaskHandle active_operation_task_;
    TaskHandle app_update_task_;
    std::filesystem::path active_update_part_path_;
    std::atomic<std::uint64_t> search_seq_{0};
    std::atomic<std::uint64_t> core_refresh_seq_{0};
    std::uint64_t consumed_focus_search_nonce_ = 0;
    std::uint64_t consumed_batch_update_nonce_ = 0;
    std::uint64_t consumed_batch_cleanup_nonce_ = 0;
    std::string loaded_subos_detail_name_;
    std::string loaded_luban_detail_name_;
};

} // namespace hxlings

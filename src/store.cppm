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

    void RefreshCore(TaskScope tasks, bool force = false) {
        if (!force && refreshing.Get()) return;
        refreshing = true;

        const std::uint64_t seq = ++core_refresh_seq_;
        auto pending = std::make_shared<std::atomic<int>>(5);
        auto self = shared_from_this();
        const XlingsClient client = client_;

        auto finish_one = [self, pending, seq]() {
            if (pending->fetch_sub(1, std::memory_order_acq_rel) == 1 &&
                self->core_refresh_seq_.load(std::memory_order_acquire) == seq) {
                self->refreshing = false;
            }
        };

        // 1. Fast Env + Version (~10ms) -> marks initial_loaded so shell renders immediately
        tasks.Launch([self, client, seq, finish_one]() -> Task<void> {
            CoreEnvSnapshot snap = co_await RunWorker([client]() -> CoreEnvSnapshot {
                return CoreEnvSnapshot{
                    .version_info = client.QueryVersion(),
                    .env = client.QueryEnv(),
                };
            });
            if (self->core_refresh_seq_.load(std::memory_order_acquire) == seq) {
                self->version_info = std::move(snap.version_info);
                self->env = std::move(snap.env);
                self->initial_loaded = true;
                self->NotifyTrayStateChanged();
            }
            finish_one();
        });

        // 2. System status (~163ms)
        tasks.Launch([self, client, seq, finish_one]() -> Task<void> {
            SystemStatus snap = co_await RunWorker([client]() -> SystemStatus {
                return client.QuerySystemStatus();
            });
            if (self->core_refresh_seq_.load(std::memory_order_acquire) == seq) {
                self->system_status = std::move(snap);
            }
            finish_one();
        });

        // 3. Installed packages (~124ms)
        tasks.Launch([self, tasks, client, seq, finish_one]() -> Task<void> {
            CorePackagesSnapshot snap = co_await RunWorker([client]() -> CorePackagesSnapshot {
                CorePackagesSnapshot s;
                s.installed_raw = client.ListInstalledPackages();
                s.installed_groups = GroupInstalledPackages(s.installed_raw);
                return s;
            });
            if (self->core_refresh_seq_.load(std::memory_order_acquire) == seq) {
                self->installed_raw = std::move(snap.installed_raw);
                self->installed_groups = std::move(snap.installed_groups);

                const auto& groups = self->installed_groups.Get();
                const std::string cur_pkg = self->selected_installed_key.Get();
                const bool cur_exists = std::ranges::any_of(
                    groups, [&](const PkgGroup& g) { return g.key == cur_pkg; });
                if (groups.empty()) {
                    self->selected_installed_key = std::string{};
                    self->selected_installed_detail = std::optional<PackageDetail>{};
                    self->selected_installed_versions = std::vector<InstalledVersionItem>{};
                    self->loading_installed_detail = false;
                } else {
                    if (!cur_exists) {
                        self->selected_installed_key = groups.front().key;
                        self->selected_installed_detail = std::optional<PackageDetail>{};
                        self->selected_installed_versions = std::vector<InstalledVersionItem>{};
                    }
                    if (self->active_nav.Get() == 1) {
                        self->EnsureInstalledDetailLoaded(tasks, cur_exists);
                    }
                }
            }
            finish_one();
        });

        // 4. SubOS + Shims (~84ms)
        tasks.Launch([self, tasks, client, seq, finish_one]() -> Task<void> {
            CoreSubosSnapshot snap = co_await RunWorker([client]() -> CoreSubosSnapshot {
                return CoreSubosSnapshot{
                    .subos_list = client.ListSubos(),
                    .active_shims = client.ListSubosShims(),
                };
            });
            if (self->core_refresh_seq_.load(std::memory_order_acquire) == seq) {
                self->subos_list = std::move(snap.subos_list);
                self->active_shims = std::move(snap.active_shims);

                const auto& suboses = self->subos_list.Get();
                std::string subos_sel = self->selected_subos.Get();
                const bool subos_exists = std::ranges::any_of(
                    suboses, [&](const SubosEntry& e) { return e.name == subos_sel; });
                if (!subos_exists) {
                    subos_sel.clear();
                    for (const auto& entry : suboses) {
                        if (entry.active) {
                            subos_sel = entry.name;
                            break;
                        }
                    }
                    if (subos_sel.empty()) {
                        const std::string active_env = self->env.Get().active_subos;
                        if (!active_env.empty() &&
                            std::ranges::any_of(suboses, [&](const SubosEntry& e) { return e.name == active_env; })) {
                            subos_sel = active_env;
                        } else if (!suboses.empty()) {
                            subos_sel = suboses.front().name;
                        }
                    }
                    self->selected_subos = subos_sel;
                    self->loaded_subos_detail_name_.clear();
                    self->selected_subos_env = std::vector<SubosEnvVar>{};
                    self->selected_subos_events = std::vector<SubosEvent>{};
                }
                if (!subos_sel.empty() && self->active_nav.Get() == 3) {
                    self->EnsureSubosDetailLoaded(tasks, subos_exists);
                }
            }
            finish_one();
        });

        // 5. Repos + Recent Logs (~42ms)
        tasks.Launch([self, client, seq, finish_one]() -> Task<void> {
            CoreReposLogsSnapshot snap = co_await RunWorker([client]() -> CoreReposLogsSnapshot {
                return CoreReposLogsSnapshot{
                    .repos = client.ListRepos(),
                    .recent_logs = client.QuerySelfLog(25),
                };
            });
            if (self->core_refresh_seq_.load(std::memory_order_acquire) == seq) {
                self->repos = std::move(snap.repos);
                self->recent_logs = std::move(snap.recent_logs);
            }
            finish_one();
        });
    }

    void EnsureInstalledDetailLoaded(TaskScope tasks, bool force = false) {
        if (loading_installed_detail.Get()) return;
        const auto& groups = installed_groups.Get();
        if (groups.empty()) {
            selected_installed_key = std::string{};
            selected_installed_detail = std::optional<PackageDetail>{};
            selected_installed_versions = std::vector<InstalledVersionItem>{};
            return;
        }
        const std::string cur_key = selected_installed_key.Get();
        auto it = std::ranges::find_if(groups, [&](const PkgGroup& g) { return g.key == cur_key; });
        const PkgGroup& target = (it != groups.end()) ? *it : groups.front();
        if (force || cur_key != target.key || !selected_installed_detail.Get().has_value()) {
            LoadInstalledDetail(tasks, target);
        }
    }

    void EnsureSubosDetailLoaded(TaskScope tasks, bool force = false) {
        if (loading_subos_detail.Get()) return;
        const auto& suboses = subos_list.Get();
        if (suboses.empty()) {
            selected_subos = std::string{};
            loaded_subos_detail_name_.clear();
            loaded_subos_governance_name = std::string{};
            selected_subos_env = std::vector<SubosEnvVar>{};
            selected_subos_events = std::vector<SubosEvent>{};
            selected_subos_status = SubosStatusSnapshot{};
            selected_subos_policy = SubosPolicySnapshot{};
            selected_subos_sessions = std::vector<SubosSessionItem>{};
            selected_subos_requests = std::vector<SubosRequestItem>{};
            selected_subos_report = SubosReportSnapshot{};
            selected_subos_info = std::string{};
            subos_doctor_snapshot = SubosDoctorSnapshot{};
            subos_rollback_list_output = std::string{};
            return;
        }
        std::string target = selected_subos.Get();
        if (!std::ranges::any_of(suboses, [&](const SubosEntry& e) { return e.name == target; })) {
            target.clear();
            for (const auto& entry : suboses) {
                if (entry.active) {
                    target = entry.name;
                    break;
                }
            }
            if (target.empty()) {
                const std::string active_env = env.Get().active_subos;
                if (!active_env.empty() &&
                    std::ranges::any_of(suboses, [&](const SubosEntry& e) { return e.name == active_env; })) {
                    target = active_env;
                } else {
                    target = suboses.front().name;
                }
            }
        }
        if (!target.empty() && (force || loaded_subos_detail_name_ != target || selected_subos.Get() != target)) {
            LoadSubosDetail(tasks, target, {});
        }
    }

    void RefreshIndexes(TaskScope tasks, bool force = false) {
        auto self = shared_from_this();
        const XlingsClient client = client_;

        if (!loading_indexes.Get() && (force || !indexes_loaded.Get())) {
            loading_indexes = true;
            tasks.Launch([self, client]() -> Task<void> {
                IndexesLocalSnapshot snap = co_await RunWorker([client]() -> IndexesLocalSnapshot {
                    return IndexesLocalSnapshot{
                        .repos = client.ListRepos(),
                        .local_xpkgs = client.ListLocalXpkgs(),
                    };
                });
                self->repos = std::move(snap.repos);
                self->local_xpkgs = std::move(snap.local_xpkgs);
                self->loading_indexes = false;
                self->indexes_loaded = true;
            });
        }

        if (!loading_index_versions.Get() && (force || !index_versions_loaded.Get())) {
            loading_index_versions = true;
            tasks.Launch([self, client]() -> Task<void> {
                std::vector<IndexRepoVersions> snap = co_await RunWorker([client]() {
                    return client.ListIndexVersions();
                });
                self->index_versions = std::move(snap);
                self->loading_index_versions = false;
                self->index_versions_loaded = true;
            });
        }
    }

    void RefreshXlingsConfig(TaskScope tasks, bool force = false) {
        if (loading_xlings_config.Get()) return;
        if (!force && xlings_config_loaded.Get()) return;
        loading_xlings_config = true;
        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client]() -> Task<void> {
            ConfigSnapshot snap = co_await RunWorker([client]() {
                return client.QueryConfig();
            });
            self->config = std::move(snap);
            self->loading_xlings_config = false;
            self->xlings_config_loaded = true;
        });
    }

    void RefreshProfiles(TaskScope tasks, bool force = false) {
        if (loading_profiles.Get()) return;
        if (!force && profiles_loaded.Get()) return;
        loading_profiles = true;
        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client]() -> Task<void> {
            std::vector<ProfileGeneration> snap = co_await RunWorker([client]() {
                return client.ListProfiles();
            });
            self->profiles = std::move(snap);
            self->loading_profiles = false;
            self->profiles_loaded = true;
        });
    }

    void RefreshAll(TaskScope tasks) {
        RefreshCore(tasks, true);
        const std::size_t nav = active_nav.Get();
        const std::size_t stab = settings_tab.Get();
        if (nav == 4 || indexes_loaded.Get()) {
            RefreshIndexes(tasks, true);
        }
        if (nav == 3 && !selected_subos.Get().empty() && !loaded_subos_governance_name.Get().empty()) {
            LoadSubosGovernance(tasks, selected_subos.Get(), true);
        }
        if ((nav == 5 && stab == 1) || xlings_config_loaded.Get()) {
            RefreshXlingsConfig(tasks, true);
            RefreshConfigThemes(tasks, true);
        }
        if ((nav == 5 && stab == 2) || profiles_loaded.Get()) {
            RefreshProfiles(tasks, true);
            RefreshNetworkEndpoints(tasks, true);
        }
    }

    void LoadInstalledDetail(TaskScope tasks, const PkgGroup& group) {
        installed_detail_task_.Cancel();
        selected_installed_key = group.key;
        installed_plan_preview = std::vector<InstallPlanItem>{};
        selected_remove_plan = std::optional<RemovePlan>{};
        loading_installed_detail = true;
        loading_remove_plan = false;

        auto self = shared_from_this();
        const XlingsClient client = client_;
        const std::string target = group.key;
        const std::string plain_name = group.name;

        installed_detail_task_ = tasks.Launch([self, client, target, plain_name]() -> Task<void> {
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

    void RunSearch(TaskScope tasks, std::string keyword, std::string category = {}) {
        search_task_.Cancel();
        keyword = TrimWhitespace(keyword);
        category = TrimWhitespace(category);
        const std::uint64_t seq = ++search_seq_;
        searching = true;
        has_searched = true;
        search_error = std::string{};

        auto self = shared_from_this();
        const XlingsClient client = client_;
        search_task_ = tasks.Launch([self, tasks, client, seq, keyword = std::move(keyword), category = std::move(category)]() -> Task<void> {
            auto outcome = co_await RunWorker([client, keyword, category]() {
                return client.SearchPackagesChecked(keyword, category);
            });
            if (self->search_seq_.load(std::memory_order_acquire) != seq) {
                co_return;
            }
            self->search_results = std::move(outcome.packages);
            self->search_error = outcome.ok ? std::string{} : std::move(outcome.error_message);
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
        store_detail_task_.Cancel();
        selected_store_pkg = package_target;
        store_plan_preview = std::vector<InstallPlanItem>{};
        loading_store_detail = true;

        auto self = shared_from_this();
        const XlingsClient client = client_;
        store_detail_task_ = tasks.Launch([self, client, package_target]() -> Task<void> {
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
        plan_preview_task_.Cancel();
        auto self = shared_from_this();
        const XlingsClient client = client_;
        plan_preview_task_ = tasks.Launch([self, client, targets = std::move(targets), global, for_store]() -> Task<void> {
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
        subos_detail_task_.Cancel();
        selected_subos = subos_name;
        loading_subos_detail = true;

        auto self = shared_from_this();
        const XlingsClient client = client_;
        subos_detail_task_ = tasks.Launch([self, client, subos_name, event_kind]() -> Task<void> {
            SubosDetailSnapshot snap = co_await RunWorker([client, subos_name, event_kind]() -> SubosDetailSnapshot {
                SubosDetailSnapshot s;
                s.name = subos_name;
                s.env_vars = client.QuerySubosEnv(subos_name);
                s.events = client.QuerySubosEvents(subos_name, event_kind, 120);
                return s;
            });
            if (self->selected_subos.Get() == snap.name) {
                self->loaded_subos_detail_name_ = snap.name;
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
                self->ShowToastSafe("命令执行完成 (exit 0)", toast);
            } else {
                self->ShowToastSafe("命令退出: " + outcome.error_message, toast);
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
                s->RefreshCore(t, true);
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
                    s->RefreshCore(t, true);
                }
            }
        );
    }

    void PreviewSingleRemoveOlderThan(TaskScope tasks, std::string pkg_key, std::string older_than) {
        if (pkg_key.empty() || older_than.empty()) return;
        remove_plan_task_.Cancel();
        loading_remove_plan = true;
        selected_remove_plan = std::optional<RemovePlan>{};

        auto self = shared_from_this();
        const XlingsClient client = client_;
        remove_plan_task_ = tasks.Launch([self, client, pkg_key = std::move(pkg_key), older_than = std::move(older_than)]() -> Task<void> {
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
                s->RefreshCore(t, true);
                if (s->indexes_loaded.Get()) s->RefreshIndexes(t, true);
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
                s->RefreshCore(t, true);
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
                s->RefreshCore(t, true);
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
                s->RefreshCore(t, true);
                if (s->indexes_loaded.Get()) s->RefreshIndexes(t, true);
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
                s->loaded_subos_detail_name_.clear();
                s->loaded_subos_governance_name = std::string{};
                s->RefreshCore(t, true);
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
                s->loaded_subos_detail_name_.clear();
                s->loaded_subos_governance_name = std::string{};
                s->RefreshCore(t, true);
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
                s->loaded_subos_detail_name_.clear();
                s->loaded_subos_governance_name = std::string{};
                s->RefreshCore(t, true);
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
                s->RefreshIndexes(t, true);
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
                s->RefreshIndexes(t, true);
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
                s->RefreshIndexes(t, true);
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
                s->RefreshIndexes(t, true);
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
                s->RefreshIndexes(t, true);
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
                s->RefreshIndexes(t, true);
            }
        );
    }

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

    void RefreshConfigThemes(TaskScope tasks, bool force = false) {
        if (loading_config_themes.Get()) return;
        if (!force && !config_themes.Get().empty()) return;
        loading_config_themes = true;
        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client]() -> Task<void> {
            auto themes = co_await RunWorker([client]() {
                return client.QueryConfigThemes();
            });
            self->config_themes = std::move(themes);
            self->loading_config_themes = false;
        });
    }

    void RefreshNetworkEndpoints(TaskScope tasks, bool force = false) {
        if (loading_network.Get()) return;
        if (!force && !network_endpoints.Get().empty()) return;
        loading_network = true;
        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client]() -> Task<void> {
            auto eps = co_await RunWorker([client]() {
                return client.QueryNetworkEndpoints();
            });
            self->network_endpoints = std::move(eps);
            self->loading_network = false;
        });
    }

    void ProbeNetworkUrlAsync(TaskScope tasks, ToastHandle toast, std::string url, bool from_custom = false) {
        const std::string normalized = NormalizeBrowserUrl(url);
        if (normalized.empty()) {
            ShowToastSafe("请输入有效的 http/https 链接", toast);
            return;
        }
        if (probing_url.Get()) {
            ShowToastSafe("正在探测网络连通性，请稍候", toast);
            return;
        }
        probing_url = true;
        probing_custom_url = from_custom;
        probing_target_url = from_custom ? std::string{} : url;
        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client, toast, normalized]() -> Task<void> {
            auto res = co_await RunWorker([client, normalized]() {
                return client.ProbeUrl(normalized);
            });
            self->network_probe_result = res;
            self->probing_url = false;
            self->probing_custom_url = false;
            self->probing_target_url = std::string{};
            if (res.reachable) {
                self->ShowToastSafe("连通正常 (" + std::to_string(res.http_status) + ")", toast);
            } else if (!res.error_message.empty()) {
                self->ShowToastSafe("探测失败: " + res.error_message, toast);
            } else if (!res.status.empty()) {
                self->ShowToastSafe("探测不可达 (" + res.status + ")", toast);
            } else {
                self->ShowToastSafe("探测不可达", toast);
            }
        });
    }

    void LoadSubosGovernance(TaskScope tasks, std::string subos_name, bool force = false) {
        if (subos_name.empty()) return;
        if (loading_subos_governance.Get()) return;
        if (!force && loaded_subos_governance_name.Get() == subos_name) return;

        if (loaded_subos_governance_name.Get() != subos_name) {
            selected_subos_status = SubosStatusSnapshot{};
            selected_subos_policy = SubosPolicySnapshot{};
            selected_subos_sessions = std::vector<SubosSessionItem>{};
            selected_subos_requests = std::vector<SubosRequestItem>{};
            selected_subos_report = SubosReportSnapshot{};
            selected_subos_info = std::string{};
            subos_doctor_snapshot = SubosDoctorSnapshot{};
        }

        loading_subos_governance = true;
        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client, subos_name]() -> Task<void> {
            struct GovernanceBundle {
                SubosStatusSnapshot status;
                SubosPolicySnapshot policy;
                std::vector<SubosSessionItem> sessions;
                std::vector<SubosRequestItem> requests;
                SubosReportSnapshot report;
                std::string info;
                SubosDoctorSnapshot doctor;
            };
            GovernanceBundle bundle = co_await RunWorker([client, subos_name]() -> GovernanceBundle {
                return GovernanceBundle{
                    .status = client.QuerySubosStatus(subos_name),
                    .policy = client.QuerySubosConfig(subos_name),
                    .sessions = client.QuerySubosPs(subos_name),
                    .requests = client.QuerySubosRequests(subos_name),
                    .report = client.QuerySubosReport(subos_name),
                    .info = client.QuerySubosInfo(subos_name),
                    .doctor = client.QuerySubosDoctor(subos_name),
                };
            });
            self->selected_subos_status = std::move(bundle.status);
            self->selected_subos_policy = std::move(bundle.policy);
            self->selected_subos_sessions = std::move(bundle.sessions);
            self->selected_subos_requests = std::move(bundle.requests);
            self->selected_subos_report = std::move(bundle.report);
            self->selected_subos_info = std::move(bundle.info);
            self->subos_doctor_snapshot = std::move(bundle.doctor);
            self->loaded_subos_governance_name = subos_name;
            self->loading_subos_governance = false;
        });
    }

    void QuerySubosRollbackListAsync(TaskScope tasks, std::string name) {
        if (name.empty()) return;
        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client, name]() -> Task<void> {
            auto text = co_await RunWorker([client, name]() {
                return client.QuerySubosRollbackList(name);
            });
            self->subos_rollback_list_output = std::move(text);
        });
    }

    void DiffSubosAsync(TaskScope tasks, std::string lhs, std::string rhs) {
        if (lhs.empty() || rhs.empty()) return;
        if (loading_subos_diff.Get()) return;
        loading_subos_diff = true;
        subos_diff_output = std::string{"正在对比 " + lhs + " 与 " + rhs + "..."};
        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client, lhs, rhs]() -> Task<void> {
            auto diff = co_await RunWorker([client, lhs, rhs]() {
                return client.DiffSubos(lhs, rhs);
            });
            self->subos_diff_output = diff.empty() ? std::string{"(无对比输出)"} : std::move(diff);
            self->loading_subos_diff = false;
        });
    }

    void RunSubosExecWithOptions(
        TaskScope tasks,
        ToastHandle toast,
        std::string subos_name,
        std::vector<std::string> argv,
        bool sandbox,
        std::string cwd,
        std::string timeout
    ) {
        if (argv.empty()) {
            toast.Show("请输入要执行的命令");
            return;
        }
        if (running_subos_exec.Get()) {
            toast.Show("已有 SubOS 命令正在运行");
            return;
        }
        running_subos_exec = true;
        subos_exec_output = std::string{"正在执行...\n"};
        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client, subos_name, argv = std::move(argv), sandbox, cwd, timeout]() mutable -> Task<void> {
            std::string rendered = co_await RunWorker([client, subos_name, argv = std::move(argv), sandbox, cwd, timeout]() {
                const CommandOutcome out = client.ExecInSubos(subos_name, argv, sandbox, cwd, timeout);
                std::string text;
                for (const auto& ev : out.events) {
                    if (ev.kind == "data" && ev.data_kind == "subos_exec_output" && !ev.message.empty()) {
                        text += ev.message;
                    } else if (ev.kind == "error" && !ev.message.empty()) {
                        if (!text.empty() && text.back() != '\n') text.push_back('\n');
                        text += "[error] " + ev.message + "\n";
                    }
                }
                if (text.empty()) {
                    text = out.raw_output.empty() ? out.error_message : out.raw_output;
                }
                if (!out.ok && !out.error_message.empty() && text.find(out.error_message) == std::string::npos) {
                    if (!text.empty() && text.back() != '\n') text.push_back('\n');
                    text += "[exit " + std::to_string(out.exit_code) + "] " + out.error_message;
                }
                return text;
            });
            self->subos_exec_output = rendered.empty() ? std::string{"(命令结束，无标准输出)"} : std::move(rendered);
            self->running_subos_exec = false;
        });
    }

    void RunScriptAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string script_file,
        std::vector<std::string> script_args
    ) {
        if (script_file.empty()) {
            toast.Show("请输入脚本路径");
            return;
        }
        if (running_script.Get()) {
            toast.Show("脚本正在执行中");
            return;
        }
        running_script = true;
        script_output = std::string{"正在运行脚本: " + script_file + "\n"};
        auto self = shared_from_this();
        const XlingsClient client = client_;
        tasks.Launch([self, client, toast, script_file, script_args = std::move(script_args)]() mutable -> Task<void> {
            CommandOutcome out = co_await RunWorker([client, script_file, script_args = std::move(script_args)]() {
                return client.RunScript(script_file, script_args);
            });
            std::string rendered = out.raw_output;
            if (!out.ok && !out.error_message.empty()) {
                if (!rendered.empty() && rendered.back() != '\n') rendered.push_back('\n');
                rendered += "[exit " + std::to_string(out.exit_code) + "] " + out.error_message;
            }
            self->script_output = rendered.empty() ? std::string{"(脚本完成，无输出)"} : std::move(rendered);
            self->running_script = false;
            toast.Show(out.ok ? "脚本运行完成" : ("脚本退出码 " + std::to_string(out.exit_code)));
        });
    }

    void StartSubosSessionAsync(TaskScope tasks, ToastHandle toast, std::string name) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "启动 SubOS 会话: " + name,
            [client, name](auto on_ev, auto, auto ctrl) {
                return client.StartSubosSession(name, std::move(on_ev), std::move(ctrl));
            },
            [name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshCore(t, true);
                s->LoadSubosGovernance(t, name, true);
            }
        );
    }

    void StopSubosSessionAsync(TaskScope tasks, ToastHandle toast, std::string name) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "停止 SubOS 会话: " + name,
            [client, name](auto on_ev, auto, auto ctrl) {
                return client.StopSubosSession(name, std::move(on_ev), std::move(ctrl));
            },
            [name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshCore(t, true);
                s->LoadSubosGovernance(t, name, true);
            }
        );
    }

    void CreateSubosAdvancedAsync(TaskScope tasks, ToastHandle toast, SubosCreateOptions opts) {
        const XlingsClient client = client_;
        const std::string title = opts.dry_run
                                      ? ("预览创建 SubOS: " + opts.name)
                                      : ("创建 SubOS: " + opts.name);
        ExecuteOperation(
            tasks,
            toast,
            title,
            [client, opts](auto on_ev, auto, auto ctrl) {
                return client.CreateSubosAdvanced(opts, std::move(on_ev), std::move(ctrl));
            },
            [opts](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome& out) {
                if (!opts.dry_run && out.ok) {
                    s->selected_subos = opts.name;
                    s->loaded_subos_detail_name_.clear();
                    s->loaded_subos_governance_name = std::string{};
                    s->RefreshCore(t, true);
                } else if (opts.dry_run && !out.raw_output.empty()) {
                    s->subos_exec_output = out.raw_output;
                }
            }
        );
    }

    void UpdateSubosConfigAsync(TaskScope tasks, ToastHandle toast, std::string name, SubosPolicyPatch patch) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "更新 SubOS 策略: " + name,
            [client, name, patch](auto on_ev, auto, auto ctrl) {
                return client.UpdateSubosConfig(name, patch, std::move(on_ev), std::move(ctrl));
            },
            [name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->LoadSubosGovernance(t, name, true);
            }
        );
    }

    void ApproveSubosRequestAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string req_id) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "批准沙箱请求 #" + req_id,
            [client, name, req_id](auto on_ev, auto, auto ctrl) {
                return client.ApproveSubosRequest(name, req_id, std::move(on_ev), std::move(ctrl));
            },
            [name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->LoadSubosGovernance(t, name, true);
            }
        );
    }

    void DenySubosRequestAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string req_id) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "拒绝沙箱请求 #" + req_id,
            [client, name, req_id](auto on_ev, auto, auto ctrl) {
                return client.DenySubosRequest(name, req_id, std::move(on_ev), std::move(ctrl));
            },
            [name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->LoadSubosGovernance(t, name, true);
            }
        );
    }

    void RunSubosDoctorFixAsync(TaskScope tasks, ToastHandle toast, std::string name = {}) {
        const XlingsClient client = client_;
        const std::string label = name.empty() ? "修复所有 SubOS 策略包" : ("修复 SubOS 策略包: " + name);
        ExecuteOperation(
            tasks,
            toast,
            label,
            [client, name](auto on_ev, auto, auto ctrl) {
                return client.RunSubosDoctorFix(name, std::move(on_ev), std::move(ctrl));
            },
            [name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                const std::string target = name.empty() ? s->selected_subos.Get() : name;
                if (!target.empty()) s->LoadSubosGovernance(t, target, true);
            }
        );
    }

    void UpgradeSubosAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string name,
        std::string to_version,
        bool dry_run
    ) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            dry_run ? ("预览升级 SubOS: " + name) : ("升级 SubOS: " + name),
            [client, name, to_version, dry_run](auto on_ev, auto, auto ctrl) {
                return client.UpgradeSubos(name, to_version, dry_run, std::move(on_ev), std::move(ctrl));
            },
            [name, dry_run](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                if (!dry_run) {
                    s->loaded_subos_detail_name_.clear();
                    s->RefreshCore(t, true);
                    s->LoadSubosGovernance(t, name, true);
                }
            }
        );
    }

    void RollbackSubosAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string generation) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "回滚 SubOS: " + name,
            [client, name, generation](auto on_ev, auto, auto ctrl) {
                return client.RollbackSubos(name, generation, std::move(on_ev), std::move(ctrl));
            },
            [name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->loaded_subos_detail_name_.clear();
                s->RefreshCore(t, true);
                s->LoadSubosGovernance(t, name, true);
                s->QuerySubosRollbackListAsync(t, name);
            }
        );
    }

    void RebindSubosRuntimeAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string binding) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "重绑定运行时 " + name + " -> " + binding,
            [client, name, binding](auto on_ev, auto, auto ctrl) {
                return client.RebindSubosRuntime(name, binding, std::move(on_ev), std::move(ctrl));
            },
            [name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->loaded_subos_detail_name_.clear();
                s->RefreshCore(t, true);
                s->LoadSubosGovernance(t, name, true);
            }
        );
    }

    void ExportSubosAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string name,
        std::string format,
        std::string output_path,
        std::string disk_size,
        bool with_data
    ) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "导出 SubOS: " + name,
            [client, name, format, output_path, disk_size, with_data](auto on_ev, auto, auto ctrl) {
                return client.ExportSubos(
                    name, format, output_path, disk_size, with_data, std::move(on_ev), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>&, TaskScope, const CommandOutcome&) {}
        );
    }

    void PackSubosAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::string name,
        std::string as_pkg,
        std::string out_dir
    ) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "打包 SubOS 为 xpkg: " + name,
            [client, name, as_pkg, out_dir](auto on_ev, auto, auto ctrl) {
                return client.PackSubos(name, as_pkg, out_dir, std::move(on_ev), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>&, TaskScope, const CommandOutcome&) {}
        );
    }

    void CopySubosFilesAsync(TaskScope tasks, ToastHandle toast, std::string src, std::string dst) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "SubOS 文件拷贝",
            [client, src, dst](auto on_ev, auto, auto ctrl) {
                return client.CopySubosFiles(src, dst, std::move(on_ev), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>&, TaskScope, const CommandOutcome&) {}
        );
    }

    void SetConfigThemeAsync(TaskScope tasks, ToastHandle toast, std::string theme_name) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "设置 CLI 主题 -> " + theme_name,
            [client, theme_name](auto, auto on_line, auto ctrl) {
                return client.SetConfigTheme(theme_name, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshXlingsConfig(t, true);
                s->RefreshConfigThemes(t, true);
            }
        );
    }

    void SetConfigRuntimeAsync(TaskScope tasks, ToastHandle toast, std::string runtime_spec) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "设置默认运行时 -> " + runtime_spec,
            [client, runtime_spec](auto, auto on_line, auto ctrl) {
                return client.SetConfigRuntime(runtime_spec, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshXlingsConfig(t, true);
            }
        );
    }

    void AddArchiveDirAsync(TaskScope tasks, ToastHandle toast, std::string dir_path) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "添加离线归档目录",
            [client, dir_path](auto, auto on_line, auto ctrl) {
                return client.AddArchiveDir(dir_path, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshXlingsConfig(t, true);
            }
        );
    }

    void RemoveArchiveDirAsync(TaskScope tasks, ToastHandle toast, std::string dir_path) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "移除离线归档目录",
            [client, dir_path](auto, auto on_line, auto ctrl) {
                return client.RemoveArchiveDir(dir_path, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshXlingsConfig(t, true);
            }
        );
    }

    void ClearLocalXpkgsAsync(TaskScope tasks, ToastHandle toast, std::string mode) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            mode == "all" ? "清空所有本地配方" : "清理过期本地配方",
            [client, mode](auto, auto on_line, auto ctrl) {
                return client.ClearLocalXpkgs(mode, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshIndexes(t, true);
            }
        );
    }

    void RunSelfMigrateAsync(TaskScope tasks, ToastHandle toast) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "迁移旧目录布局 (self migrate)",
            [client](auto, auto on_line, auto ctrl) {
                return client.RunSelfMigrate(std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshCore(t, true);
                if (s->xlings_config_loaded.Get()) s->RefreshXlingsConfig(t, true);
            }
        );
    }

    void RunSelfInitAsync(TaskScope tasks, ToastHandle toast, std::string home_dir, bool force) {
        const XlingsClient client = client_;
        ExecuteOperation(
            tasks,
            toast,
            "初始化 xlings 目录 (self init)",
            [client, home_dir, force](auto, auto on_line, auto ctrl) {
                return client.RunSelfInit(home_dir, force, std::move(on_line), std::move(ctrl));
            },
            [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
                s->RefreshCore(t, true);
                if (s->xlings_config_loaded.Get()) s->RefreshXlingsConfig(t, true);
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
                s->RefreshXlingsConfig(t, true);
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
                s->RefreshXlingsConfig(t, true);
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
                s->RefreshXlingsConfig(t, true);
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
                s->RefreshXlingsConfig(t, true);
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
                if (fix && !dry_run) {
                    s->RefreshCore(t, true);
                    if (s->xlings_config_loaded.Get()) s->RefreshXlingsConfig(t, true);
                }
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
                if (!dry_run) {
                    s->RefreshCore(t, true);
                    if (s->xlings_config_loaded.Get()) s->RefreshXlingsConfig(t, true);
                }
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
                s->RefreshCore(t, true);
                if (s->xlings_config_loaded.Get()) s->RefreshXlingsConfig(t, true);
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
                s->RefreshProfiles(t, true);
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
                s->RefreshProfiles(t, true);
                s->RefreshCore(t, true);
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
                s->RefreshCore(t, true);
            }
        );
    }

public:
    void CheckForAppUpdateAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::shared_ptr<HttpClient> http
    ) {
        if (skip_initial_refresh) {
            app_update_state.Update([](AppUpdateState& st) {
                st.status_message = "测试模式下跳过联网检查";
                st.error_message.clear();
            });
            return;
        }
        const AppUpdateState cur = app_update_state.Get();
        if (cur.phase == UpdatePhase::Checking ||
            cur.phase == UpdatePhase::Downloading ||
            cur.phase == UpdatePhase::Verifying) {
            toast.Show("正在检查或下载更新，请稍候");
            return;
        }
        if (!http) {
            app_update_state.Update([](AppUpdateState& st) {
                st.phase = UpdatePhase::Error;
                st.error_message = "HTTP 网络客户端不可用";
                st.status_message = st.error_message;
            });
            toast.Show("HTTP 网络客户端不可用");
            return;
        }

        app_update_task_.Cancel();
        app_update_state.Update([](AppUpdateState& st) {
            st.phase = UpdatePhase::Checking;
            st.error_message.clear();
            st.status_message = "正在检查 GitHub Releases 最新版本...";
        });

        auto self = shared_from_this();
        app_update_task_ = tasks.Launch([self, http = std::move(http), toast]() -> Task<void> {
            const HttpRequest req{
                .url = std::string(kAppReleaseApiUrl),
                .headers = {
                    {"accept", "application/vnd.github+json"},
                    {"user-agent", "Hxlings/" + std::string(kAppVersion)},
                },
                .timeout = std::chrono::milliseconds{15000},
            };
            const auto resp = co_await http->SendAsync(req);
            if (!resp.Succeeded()) {
                const std::string err = "检查更新失败: " + resp.Error().message;
                self->app_update_state.Update([&](AppUpdateState& st) {
                    st.phase = UpdatePhase::Error;
                    st.error_message = err;
                    st.status_message = err;
                    st.checked_at = CurrentLocalTimestampNow();
                });
                self->ShowToastSafe(err, toast);
                co_return;
            }

            const HttpResponse& http_val = resp.Value();
            if (http_val.status_code < 200 || http_val.status_code >= 300) {
                const std::string err = "GitHub API 返回 HTTP " + std::to_string(http_val.status_code);
                self->app_update_state.Update([&](AppUpdateState& st) {
                    st.phase = UpdatePhase::Error;
                    st.error_message = err;
                    st.status_message = err;
                    st.checked_at = CurrentLocalTimestampNow();
                });
                self->ShowToastSafe(err, toast);
                co_return;
            }

            const std::string_view body_sv(
                reinterpret_cast<const char*>(http_val.body.data()),
                http_val.body.size()
            );
            auto parsed = ParseGitHubLatestReleaseJson(body_sv);
            if (!parsed.has_value()) {
                const std::string err = "解析 GitHub Release 响应失败";
                self->app_update_state.Update([&](AppUpdateState& st) {
                    st.phase = UpdatePhase::Error;
                    st.error_message = err;
                    st.status_message = err;
                    st.checked_at = CurrentLocalTimestampNow();
                });
                self->ShowToastSafe(err, toast);
                co_return;
            }

            auto [selected_asset, install_kind] = SelectPlatformReleaseAsset(*parsed);
            const bool newer = IsNewerAppVersion(kAppVersion, parsed->version);
            const std::string now_ts = CurrentLocalTimestampNow();

            if (!newer) {
                self->app_update_state.Update([&](AppUpdateState& st) {
                    st.phase = UpdatePhase::UpToDate;
                    st.latest = parsed;
                    st.selected_asset = selected_asset;
                    st.install_kind = install_kind;
                    st.error_message.clear();
                    st.checked_at = now_ts;
                    st.status_message = "当前已是最新版本 (v" + std::string(kAppVersion) + ")";
                });
                self->ShowToastSafe("当前已是最新版本", toast);
                co_return;
            }

            const std::string tag = parsed->tag_name;
            const std::uint64_t total_sz = selected_asset.has_value() ? selected_asset->size_bytes : 0;
            self->app_update_state.Update([&](AppUpdateState& st) {
                st.phase = UpdatePhase::Available;
                st.latest = std::move(parsed);
                st.selected_asset = std::move(selected_asset);
                st.install_kind = install_kind;
                st.downloaded_bytes = 0;
                st.total_bytes = total_sz;
                st.downloaded_file_path.clear();
                st.sha256_verified = false;
                st.error_message.clear();
                st.checked_at = now_ts;
                st.status_message = "发现新版本 " + tag;
            });
            self->ShowToastSafe("发现新版本 " + tag, toast);
        });
    }

    void DownloadAppUpdateAsync(
        TaskScope tasks,
        ToastHandle toast,
        std::shared_ptr<HttpClient> http
    ) {
        if (skip_initial_refresh) {
            app_update_state.Update([](AppUpdateState& st) {
                st.status_message = "测试模式下跳过联网下载";
                st.error_message.clear();
            });
            return;
        }
        const AppUpdateState cur = app_update_state.Get();
        if (cur.phase == UpdatePhase::Checking ||
            cur.phase == UpdatePhase::Downloading ||
            cur.phase == UpdatePhase::Verifying) {
            toast.Show("已有更新任务正在进行");
            return;
        }
        if (!cur.latest.has_value() || !cur.selected_asset.has_value() ||
            !IsSafeExternalUrl(cur.selected_asset->download_url)) {
            toast.Show("未找到适配当前平台的更新安装包");
            return;
        }
        if (!http) {
            toast.Show("HTTP 网络客户端不可用");
            return;
        }

        const AppReleaseInfo latest = *cur.latest;
        const ReleaseAssetInfo asset = *cur.selected_asset;
        const UpdateInstallKind install_kind = cur.install_kind;
        const std::filesystem::path dir = ResolveUpdateDownloadDir();
        const std::filesystem::path part_path = dir / (asset.name + ".part");
        const std::filesystem::path final_path = dir / asset.name;

        app_update_task_.Cancel();
        active_update_part_path_ = part_path;
        app_update_state.Update([&](AppUpdateState& st) {
            st.phase = UpdatePhase::Downloading;
            st.downloaded_bytes = 0;
            st.total_bytes = asset.size_bytes;
            st.downloaded_file_path.clear();
            st.sha256_verified = false;
            st.error_message.clear();
            st.status_message = "正在下载 " + asset.name + "...";
        });

        auto self = shared_from_this();
        app_update_task_ = tasks.Launch(
            [self, http = std::move(http), toast, latest, asset, install_kind, dir, part_path, final_path]() -> Task<void> {
                auto fail_update = [&](std::string msg) {
                    std::error_code rm_ec;
                    std::filesystem::remove(part_path, rm_ec);
                    self->active_update_part_path_.clear();
                    self->app_update_state.Update([&](AppUpdateState& st) {
                        st.phase = UpdatePhase::Error;
                        st.error_message = msg;
                        st.status_message = msg;
                    });
                    self->ShowToastSafe(std::move(msg), toast);
                };

                std::error_code dir_ec;
                std::filesystem::create_directories(dir, dir_ec);
                if (dir_ec) {
                    fail_update("无法创建更新缓存目录: " + dir_ec.message());
                    co_return;
                }

                std::string expected_sha256;
                if (!latest.checksums_url.empty()) {
                    const auto sum_resp = co_await http->SendAsync(HttpRequest{
                        .url = latest.checksums_url,
                        .headers = {{"user-agent", "Hxlings/" + std::string(kAppVersion)}},
                        .timeout = std::chrono::milliseconds{15000},
                    });
                    if (!sum_resp.Succeeded() || sum_resp.Value().status_code < 200 ||
                        sum_resp.Value().status_code >= 300) {
                        fail_update("下载 SHA256SUMS.txt 失败，已中止更新以保障完整性");
                        co_return;
                    }
                    const std::string_view sum_sv(
                        reinterpret_cast<const char*>(sum_resp.Value().body.data()),
                        sum_resp.Value().body.size()
                    );
                    expected_sha256 = ParseSha256SumsText(sum_sv, asset.name);
                    if (expected_sha256.empty()) {
                        fail_update("校验清单中未找到目标文件 " + asset.name + " 的 SHA-256");
                        co_return;
                    }
                }

                File part_file(mcpplibs::os::fs::utf8(part_path));
                auto out_opened = co_await part_file.OpenWriteAsync(FileWriteMode::Truncate);
                if (!out_opened.Succeeded()) {
                    fail_update("无法打开临时下载文件: " + out_opened.Error().message);
                    co_return;
                }
                AsyncOutputStream out_stream = std::move(out_opened).Value();

                auto http_opened = co_await http->SendStreamAsync(HttpRequest{
                    .url = asset.download_url,
                    .headers = {{"user-agent", "Hxlings/" + std::string(kAppVersion)}},
                    .timeout = std::nullopt,
                });
                if (!http_opened.Succeeded()) {
                    (void)co_await out_stream.CloseAsync();
                    fail_update("下载更新包失败: " + http_opened.Error().message);
                    co_return;
                }
                HttpResponseStream http_stream = std::move(http_opened).Value();
                if (http_stream.StatusCode() < 200 || http_stream.StatusCode() >= 300) {
                    (void)co_await out_stream.CloseAsync();
                    fail_update("更新包下载返回 HTTP " + std::to_string(http_stream.StatusCode()));
                    co_return;
                }

                std::uint64_t expected_total = asset.size_bytes;
                if (expected_total == 0) {
                    for (const auto& hdr : http_stream.Headers()) {
                        if (hdr.name == "content-length" || hdr.name == "Content-Length") {
                            std::uint64_t parsed_len = 0;
                            const auto [ptr, ec] = std::from_chars(
                                hdr.value.data(), hdr.value.data() + hdr.value.size(), parsed_len);
                            if (ec == std::errc{} && ptr == hdr.value.data() + hdr.value.size()) {
                                expected_total = parsed_len;
                            }
                            break;
                        }
                    }
                }

                Sha256Hasher hasher;
                std::uint64_t downloaded = 0;
                auto last_ui_tick = std::chrono::steady_clock::now();

                while (true) {
                    auto read_res = co_await http_stream.Body().ReadAsync(64u * 1024u);
                    if (!read_res.Succeeded()) {
                        (void)co_await out_stream.CloseAsync();
                        fail_update("读取更新数据流失败: " + read_res.Error().message);
                        co_return;
                    }
                    Bytes chunk = std::move(read_res).Value();
                    if (chunk.empty()) {
                        break;
                    }
                    hasher.Update(chunk);
                    downloaded += static_cast<std::uint64_t>(chunk.size());

                    auto write_res = co_await out_stream.WriteAsync(std::move(chunk));
                    if (!write_res.Succeeded()) {
                        (void)co_await out_stream.CloseAsync();
                        fail_update("写入本地更新文件失败: " + write_res.Error().message);
                        co_return;
                    }

                    const auto now = std::chrono::steady_clock::now();
                    if (now - last_ui_tick >= std::chrono::milliseconds{100}) {
                        last_ui_tick = now;
                        self->app_update_state.Update([downloaded, expected_total](AppUpdateState& st) {
                            st.downloaded_bytes = downloaded;
                            if (expected_total > 0) st.total_bytes = expected_total;
                        });
                    }
                }

                auto close_res = co_await out_stream.CloseAsync();
                if (!close_res.Succeeded()) {
                    fail_update("落盘关闭下载文件失败: " + close_res.Error().message);
                    co_return;
                }

                self->app_update_state.Update([downloaded, expected_total](AppUpdateState& st) {
                    st.phase = UpdatePhase::Verifying;
                    st.downloaded_bytes = downloaded;
                    st.total_bytes = expected_total > 0 ? expected_total : downloaded;
                    st.status_message = "正在校验 SHA-256...";
                });

                const std::string actual_sha256 = hasher.FinalHex();
                if (!expected_sha256.empty() && actual_sha256 != expected_sha256) {
                    fail_update("SHA-256 校验不匹配（期望 " + expected_sha256.substr(0, 12) +
                                "...，实际 " + actual_sha256.substr(0, 12) + "...）");
                    co_return;
                }

                std::error_code mv_ec;
                std::filesystem::remove(final_path, mv_ec);
                mv_ec.clear();
                std::filesystem::rename(part_path, final_path, mv_ec);
                if (mv_ec) {
                    fail_update("重命名更新文件失败: " + mv_ec.message());
                    co_return;
                }

                self->active_update_part_path_.clear();
                const std::string final_utf8 = mcpplibs::os::fs::utf8(final_path);
                const bool verified = !expected_sha256.empty();
                self->app_update_state.Update([&](AppUpdateState& st) {
                    st.phase = UpdatePhase::ReadyToInstall;
                    st.downloaded_bytes = downloaded;
                    if (st.total_bytes == 0) st.total_bytes = downloaded;
                    st.downloaded_file_path = final_utf8;
                    st.sha256_verified = verified;
                    st.error_message.clear();
                    st.status_message = verified
                                            ? ("已下载并验证 SHA-256: " + asset.name)
                                            : ("已下载更新包: " + asset.name);
                });
                self->ShowToastSafe("更新包已下载并校验完成，可点击“立即安装更新”", toast);
            }
        );
    }

    void CancelAppUpdate() {
        app_update_task_.Cancel();
        if (!active_update_part_path_.empty()) {
            std::error_code ec;
            std::filesystem::remove(active_update_part_path_, ec);
            active_update_part_path_.clear();
        }
        app_update_state.Update([](AppUpdateState& st) {
            if (st.phase == UpdatePhase::Downloading ||
                st.phase == UpdatePhase::Verifying ||
                st.phase == UpdatePhase::Checking) {
                st.phase = st.latest.has_value() ? UpdatePhase::Available : UpdatePhase::Idle;
                st.downloaded_bytes = 0;
                st.error_message.clear();
                st.status_message = "已取消更新操作";
            }
        });
    }

    void ApplyAppUpdateAsync(ToastHandle toast, ApplicationHandle application) {
        if (skip_initial_refresh) {
            app_update_state.Update([](AppUpdateState& st) {
                st.status_message = "测试模式下跳过安装启动";
            });
            return;
        }
        const AppUpdateState snap = app_update_state.Get();
        if (snap.downloaded_file_path.empty()) {
            toast.Show("请先下载更新包");
            return;
        }
        app_update_state.Update([](AppUpdateState& st) {
            st.phase = UpdatePhase::Installing;
            st.status_message = "正在启动更新安装流程...";
        });
        const bool releases_for_relaunch =
            snap.install_kind == UpdateInstallKind::LinuxAppImage ||
            snap.install_kind == UpdateInstallKind::WindowsSetupExe ||
            snap.install_kind == UpdateInstallKind::WindowsMsi;
        if (releases_for_relaunch) {
            ReleaseProcessSingleInstance(false);
        }
        const ApplyUpdateResult res = ApplyDownloadedUpdate(
            mcpplibs::os::fs::from_utf8(snap.downloaded_file_path),
            snap.install_kind
        );
        app_update_state.Update([&](AppUpdateState& st) {
            st.phase = res.ok ? UpdatePhase::ReadyToInstall : UpdatePhase::Error;
            st.status_message = res.install_hint.empty() ? res.message : (res.message + "\n" + res.install_hint);
            st.error_message = res.ok ? std::string{} : res.message;
        });
        ShowToastSafe(res.message, toast);
        if (res.ok && res.should_quit_app) {
            ReleaseProcessSingleInstance(true);
            force_quit_requested = true;
            application.Quit();
        } else if (releases_for_relaunch) {
            (void)AcquireProcessSingleInstance();
        }
    }

private:
    XlingsClient client_;
    std::shared_ptr<ActiveProcessControl> active_process_;
    WorkerSequence mutation_sequence_;
    TaskHandle search_task_;
    TaskHandle installed_detail_task_;
    TaskHandle store_detail_task_;
    TaskHandle plan_preview_task_;
    TaskHandle remove_plan_task_;
    TaskHandle subos_detail_task_;
    TaskHandle active_operation_task_;
    TaskHandle app_update_task_;
    std::filesystem::path active_update_part_path_;
    std::atomic<std::uint64_t> search_seq_{0};
    std::atomic<std::uint64_t> core_refresh_seq_{0};
    std::uint64_t consumed_focus_search_nonce_ = 0;
    std::uint64_t consumed_batch_update_nonce_ = 0;
    std::uint64_t consumed_batch_cleanup_nonce_ = 0;
    std::string loaded_subos_detail_name_;
};

} // namespace hxlings

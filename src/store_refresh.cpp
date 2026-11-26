module hxlings.store;

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

namespace hxlings {

void XlingsStore::RefreshCore(TaskScope tasks, bool force) {
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

void XlingsStore::EnsureInstalledDetailLoaded(TaskScope tasks, bool force) {
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

void XlingsStore::EnsureSubosDetailLoaded(TaskScope tasks, bool force) {
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

void XlingsStore::RefreshIndexes(TaskScope tasks, bool force) {
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

void XlingsStore::RefreshXlingsConfig(TaskScope tasks, bool force) {
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

void XlingsStore::RefreshProfiles(TaskScope tasks, bool force) {
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

void XlingsStore::RefreshAll(TaskScope tasks) {
    RefreshCore(tasks, true);
    const std::size_t nav = active_nav.Get();
    const std::size_t stab = settings_tab.Get();
    if (nav == 4 || luban_loaded.Get()) {
        RefreshLuban(tasks, true);
    }
    if (nav == 5 || indexes_loaded.Get()) {
        RefreshIndexes(tasks, true);
    }
    if (nav == 3 && !selected_subos.Get().empty() && !loaded_subos_governance_name.Get().empty()) {
        LoadSubosGovernance(tasks, selected_subos.Get(), true);
    }
    if ((nav == 6 && stab == 1) || xlings_config_loaded.Get()) {
        RefreshXlingsConfig(tasks, true);
        RefreshConfigThemes(tasks, true);
    }
    if ((nav == 6 && stab == 2) || profiles_loaded.Get()) {
        RefreshProfiles(tasks, true);
        RefreshNetworkEndpoints(tasks, true);
    }
}

void XlingsStore::LoadInstalledDetail(TaskScope tasks, const PkgGroup& group) {
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

void XlingsStore::RunSearch(TaskScope tasks, std::string keyword, std::string category) {
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

void XlingsStore::LoadStoreDetail(TaskScope tasks, const std::string& package_target) {
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

void XlingsStore::PreviewInstallPlan(TaskScope tasks, std::vector<std::string> targets, bool global, bool for_store) {
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

void XlingsStore::RefreshConfigThemes(TaskScope tasks, bool force) {
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

void XlingsStore::RefreshNetworkEndpoints(TaskScope tasks, bool force) {
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

void XlingsStore::ProbeNetworkUrlAsync(TaskScope tasks, ToastHandle toast, std::string url, bool from_custom) {
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

} // namespace hxlings

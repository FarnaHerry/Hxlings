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

void XlingsStore::InstallPackageAsync(
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

void XlingsStore::RemovePackageAsync(
    TaskScope tasks,
    ToastHandle toast,
    std::string target,
    std::string older_than,
    bool dry_run) {
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

void XlingsStore::PreviewSingleRemoveOlderThan(TaskScope tasks, std::string pkg_key, std::string older_than) {
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

void XlingsStore::ScanExpiredVersions(TaskScope tasks) {
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

void XlingsStore::BatchRemoveExpiredAsync(TaskScope tasks, ToastHandle toast, std::vector<ExpiredPackagePlan> targets) {
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

void XlingsStore::BatchRemoveTargetsAsync(TaskScope tasks, ToastHandle toast, std::vector<std::string> full_targets) {
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

void XlingsStore::ScanUpdatablePackages(TaskScope tasks) {
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

void XlingsStore::SyncIndexAndScanUpdatesAsync(TaskScope tasks, ToastHandle toast) {
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

void XlingsStore::BatchUpdatePackagesAsync(
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

void XlingsStore::ActivateVersionAsync(TaskScope tasks, ToastHandle toast, std::string plain_name, std::string version) {
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

void XlingsStore::UpdatePackagesAsync(TaskScope tasks, ToastHandle toast, std::string target) {
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

void XlingsStore::AddRepoAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string url) {
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

void XlingsStore::RemoveRepoAsync(TaskScope tasks, ToastHandle toast, std::string name) {
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

void XlingsStore::SyncIndexAsync(TaskScope tasks, ToastHandle toast, std::string name) {
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

void XlingsStore::PinIndexVersionAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string version) {
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

void XlingsStore::AddLocalXpkgAsync(TaskScope tasks, ToastHandle toast, std::string path) {
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

void XlingsStore::RemoveLocalXpkgAsync(TaskScope tasks, ToastHandle toast, std::string name) {
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

void XlingsStore::ExecuteBatchRemovalSteps(
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

} // namespace hxlings

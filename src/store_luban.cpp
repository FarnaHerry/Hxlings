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

void XlingsStore::RefreshLuban(TaskScope tasks, bool force) {
    if (skip_initial_refresh) return;
    if (loading_luban.Get()) return;
    if (!force && luban_loaded.Get()) return;

    loading_luban = true;
    auto self = shared_from_this();
    tasks.Launch([self, tasks, force]() -> Task<void> {
        LubanOverviewSnapshot snap = co_await RunWorker([]() -> LubanOverviewSnapshot {
            LubanClient cli;
            LubanOverviewSnapshot out;
            out.luban_available = cli.IsAvailable();
            out.luban_os_available = cli.IsLubanOsInstalled();
            out.luban_binary_path = cli.LubanBinaryPath();
            out.luban_os_binary_path = cli.LubanOsBinaryPath();
            if (out.luban_available) {
                out.host_status = cli.QueryHostStatus();
                out.envs = cli.ListEnvironments();
                out.catalog = cli.SearchCatalog();
            }
            return out;
        });

        self->luban_client_ = LubanClient(snap.luban_binary_path, snap.luban_os_binary_path);
        self->luban_available = snap.luban_available;
        self->luban_os_available = snap.luban_os_available;
        self->luban_host_status = std::move(snap.host_status);
        self->luban_envs = std::move(snap.envs);
        self->luban_catalog = std::move(snap.catalog);

        const auto& envs = self->luban_envs.Get();
        std::string cur_sel = self->selected_luban_env.Get();
        const bool exists = std::ranges::any_of(
            envs, [&](const LubanEnvironment& e) { return e.name == cur_sel; });
        if (!exists) {
            cur_sel = envs.empty() ? std::string{} : envs.front().name;
            self->selected_luban_env = cur_sel;
            self->loaded_luban_detail_name_.clear();
            self->selected_luban_status = std::optional<LubanEnvStatusDetail>{};
            self->selected_luban_config = std::optional<LubanEnvConfigDetail>{};
            self->selected_luban_history = std::string{};
            self->selected_luban_logs = std::string{};
        }

        self->loading_luban = false;
        self->luban_loaded = true;

        if (snap.luban_available && !cur_sel.empty() && self->active_nav.Get() == 4) {
            self->LoadLubanDetail(tasks, cur_sel, force || !exists);
        }
    });
}

void XlingsStore::LoadLubanDetail(TaskScope tasks, std::string env_name, bool force) {
    if (env_name.empty()) {
        luban_detail_task_.Cancel();
        selected_luban_env = std::string{};
        loaded_luban_detail_name_.clear();
        selected_luban_status = std::optional<LubanEnvStatusDetail>{};
        selected_luban_config = std::optional<LubanEnvConfigDetail>{};
        selected_luban_history = std::string{};
        selected_luban_logs = std::string{};
        loading_luban_detail = false;
        return;
    }
    if (skip_initial_refresh) {
        selected_luban_env = env_name;
        return;
    }
    if (!force &&
        loaded_luban_detail_name_ == env_name &&
        selected_luban_env.Get() == env_name &&
        !loading_luban_detail.Get()) {
        return;
    }

    luban_detail_task_.Cancel();
    selected_luban_env = env_name;
    loading_luban_detail = true;

    auto self = shared_from_this();
    LubanClient cli = luban_client_;
    if (!cli.IsAvailable()) {
        cli.RefreshDiscoveredPaths();
    }
    luban_detail_task_ = tasks.Launch([self, cli, env_name]() -> Task<void> {
        LubanDetailSnapshot snap = co_await RunWorker([cli, env_name]() -> LubanDetailSnapshot {
            LubanDetailSnapshot s;
            s.name = env_name;
            if (cli.IsAvailable()) {
                s.status = cli.QueryEnvStatus(env_name);
                s.config = cli.QueryEnvConfig(env_name);
                s.history = cli.QueryEnvHistory(env_name);
                s.logs = cli.QueryEnvLogs(env_name);
            }
            return s;
        });

        if (self->selected_luban_env.Get() == snap.name) {
            if (snap.config.has_value()) {
                self->luban_config_cache.Update([&](std::map<std::string, LubanEnvConfigDetail>& m) {
                    m[snap.name] = *snap.config;
                });
            }
            self->loaded_luban_detail_name_ = snap.name;
            self->selected_luban_status = std::move(snap.status);
            self->selected_luban_config = std::move(snap.config);
            self->selected_luban_history = std::move(snap.history);
            self->selected_luban_logs = std::move(snap.logs);
            self->loading_luban_detail = false;
        }
    });
}

void XlingsStore::CreateLubanEnvAsync(TaskScope tasks, ToastHandle toast, LubanCreateOptions opts) {
    LubanClient cli = luban_client_;
    if (!cli.IsAvailable()) cli.RefreshDiscoveredPaths();
    const std::string title = opts.dry_run
                                  ? ("预览创建 Luban 环境: " + opts.name)
                                  : ("创建 Luban 环境: " + opts.name);
    ExecuteOperation(
        tasks,
        toast,
        title,
        [cli, opts](auto on_ev, auto, auto ctrl) {
            return cli.CreateEnvironment(opts, std::move(on_ev), std::move(ctrl));
        },
        [opts](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome& out) {
            if (!opts.dry_run && out.ok) {
                s->selected_luban_env = opts.name;
                s->loaded_luban_detail_name_.clear();
                s->RefreshLuban(t, true);
            }
        }
    );
}

void XlingsStore::ConfigureLubanEnvAsync(TaskScope tasks, ToastHandle toast, std::string name, LubanConfigPatch patch) {
    if (patch.Empty()) {
        ShowToastSafe("未检测到配置变更", toast);
        return;
    }
    LubanClient cli = luban_client_;
    if (!cli.IsAvailable()) cli.RefreshDiscoveredPaths();
    ExecuteOperation(
        tasks,
        toast,
        "更新 Luban 环境配置: " + name,
        [cli, name, patch](auto on_ev, auto, auto ctrl) {
            return cli.ConfigureEnvironment(name, patch, std::move(on_ev), std::move(ctrl));
        },
        [name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
            s->loaded_luban_detail_name_.clear();
            s->RefreshLuban(t, true);
            s->LoadLubanDetail(t, name, true);
        }
    );
}

void XlingsStore::RunLubanCommandInlineAsync(
    TaskScope tasks,
    ToastHandle toast,
    std::string name,
    std::vector<std::string> cmd_argv
) {
    if (cmd_argv.empty()) {
        toast.Show("请输入要在环境中执行的命令");
        return;
    }
    if (running_luban_cmd.Get()) {
        toast.Show("已有 Luban 命令正在执行");
        return;
    }
    running_luban_cmd = true;
    luban_run_output = std::string{"正在运行 `luban run " + name + "` ...\n"};

    auto self = shared_from_this();
    LubanClient cli = luban_client_;
    if (!cli.IsAvailable()) cli.RefreshDiscoveredPaths();
    tasks.Launch([self, toast, cli, name = std::move(name), cmd_argv = std::move(cmd_argv)]() mutable -> Task<void> {
        CommandOutcome out = co_await RunWorker([cli, name, cmd_argv]() {
            return cli.RunCommand(name, cmd_argv);
        });
        std::string rendered = out.raw_output;
        if (!out.ok && !out.error_message.empty() &&
            rendered.find(out.error_message) == std::string::npos) {
            if (!rendered.empty() && rendered.back() != '\n') rendered.push_back('\n');
            rendered += "[exit " + std::to_string(out.exit_code) + "] " + out.error_message;
        }
        self->luban_run_output = rendered.empty() ? std::string{"(命令完成，无标准输出)"} : std::move(rendered);
        self->running_luban_cmd = false;
        self->ShowToastSafe(
            out.ok ? "Luban 命令执行完成 (exit 0)" : ("Luban 命令退出码 " + std::to_string(out.exit_code)),
            toast
        );
    });
}

void XlingsStore::StopLubanEnvAsync(TaskScope tasks, ToastHandle toast, std::string name) {
    LubanClient cli = luban_client_;
    if (!cli.IsAvailable()) cli.RefreshDiscoveredPaths();
    ExecuteOperation(
        tasks,
        toast,
        "停止 Luban 环境: " + name,
        [cli, name](auto on_ev, auto, auto ctrl) {
            return cli.StopEnvironment(name, std::move(on_ev), std::move(ctrl));
        },
        [name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
            s->loaded_luban_detail_name_.clear();
            s->RefreshLuban(t, true);
            s->LoadLubanDetail(t, name, true);
        }
    );
}

void XlingsStore::UpgradeLubanEnvAsync(
    TaskScope tasks,
    ToastHandle toast,
    std::string name,
    std::string target_ver,
    bool dry_run) {
    LubanClient cli = luban_client_;
    if (!cli.IsAvailable()) cli.RefreshDiscoveredPaths();
    ExecuteOperation(
        tasks,
        toast,
        dry_run ? ("预览升级 Luban 环境: " + name) : ("升级 Luban 环境: " + name),
        [cli, name, target_ver, dry_run](auto on_ev, auto, auto ctrl) {
            return cli.UpgradeEnvironment(name, target_ver, dry_run, std::move(on_ev), std::move(ctrl));
        },
        [name, dry_run](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
            if (!dry_run) {
                s->loaded_luban_detail_name_.clear();
                s->RefreshLuban(t, true);
                s->LoadLubanDetail(t, name, true);
            }
        }
    );
}

void XlingsStore::RollbackLubanEnvAsync(
    TaskScope tasks,
    ToastHandle toast,
    std::string name,
    std::string gen) {
    LubanClient cli = luban_client_;
    if (!cli.IsAvailable()) cli.RefreshDiscoveredPaths();
    ExecuteOperation(
        tasks,
        toast,
        "回滚 Luban 环境: " + name,
        [cli, name, gen](auto on_ev, auto, auto ctrl) {
            return cli.RollbackEnvironment(name, gen, std::move(on_ev), std::move(ctrl));
        },
        [name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
            s->loaded_luban_detail_name_.clear();
            s->RefreshLuban(t, true);
            s->LoadLubanDetail(t, name, true);
        }
    );
}

void XlingsStore::ExportLubanEnvAsync(TaskScope tasks, ToastHandle toast, LubanExportOptions opts) {
    LubanClient cli = luban_client_;
    if (!cli.IsAvailable()) cli.RefreshDiscoveredPaths();
    ExecuteOperation(
        tasks,
        toast,
        "导出 Luban 环境镜像: " + opts.name,
        [cli, opts](auto on_ev, auto, auto ctrl) {
            return cli.ExportEnvironment(opts, std::move(on_ev), std::move(ctrl));
        },
        [](const std::shared_ptr<XlingsStore>&, TaskScope, const CommandOutcome&) {}
    );
}

void XlingsStore::ImportLubanEnvAsync(
    TaskScope tasks,
    ToastHandle toast,
    std::string archive_path,
    std::string custom_name) {
    LubanClient cli = luban_client_;
    if (!cli.IsAvailable()) cli.RefreshDiscoveredPaths();
    ExecuteOperation(
        tasks,
        toast,
        "导入 Luban 环境归档",
        [cli, archive_path, custom_name](auto on_ev, auto, auto ctrl) {
            return cli.ImportEnvironment(archive_path, custom_name, std::move(on_ev), std::move(ctrl));
        },
        [custom_name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome& out) {
            if (out.ok && !custom_name.empty()) {
                s->selected_luban_env = custom_name;
            }
            s->loaded_luban_detail_name_.clear();
            s->RefreshLuban(t, true);
        }
    );
}

void XlingsStore::RemoveLubanEnvAsync(TaskScope tasks, ToastHandle toast, std::string name) {
    LubanClient cli = luban_client_;
    if (!cli.IsAvailable()) cli.RefreshDiscoveredPaths();
    ExecuteOperation(
        tasks,
        toast,
        "删除 Luban 环境: " + name,
        [cli, name](auto on_ev, auto, auto ctrl) {
            return cli.RemoveEnvironment(name, std::move(on_ev), std::move(ctrl));
        },
        [name](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome& out) {
            if (out.ok && s->selected_luban_env.Get() == name) {
                s->selected_luban_env = std::string{};
                s->loaded_luban_detail_name_.clear();
            }
            s->RefreshLuban(t, true);
        }
    );
}

void XlingsStore::SetupLubanHostAsync(TaskScope tasks, ToastHandle toast) {
    LubanClient cli = luban_client_;
    if (!cli.IsAvailable()) cli.RefreshDiscoveredPaths();
    ExecuteOperation(
        tasks,
        toast,
        "初始化 Luban 宿主隔离环境 (luban setup)",
        [cli](auto on_ev, auto, auto ctrl) {
            return cli.SetupHost(std::move(on_ev), std::move(ctrl));
        },
        [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
            s->RefreshLuban(t, true);
        }
    );
}

void XlingsStore::InstallLubanComponentAsync(TaskScope tasks, ToastHandle toast, std::string pkg_spec) {
    const XlingsClient client = client_;
    ExecuteOperation(
        tasks,
        toast,
        "安装 Luban 组件 " + pkg_spec,
        [client, pkg_spec](auto on_ev, auto, auto ctrl) {
            return client.InstallPackages(
                {pkg_spec}, true, true, std::string{}, false, std::move(on_ev), std::move(ctrl));
        },
        [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
            s->RefreshCore(t, true);
            s->RefreshLuban(t, true);
        }
    );
}

void XlingsStore::LaunchLubanOsApp(ToastHandle toast) {
    if (skip_initial_refresh) {
        ShowToastSafe("测试模式下跳过启动 LubanOS", toast);
        return;
    }
    luban_client_.RefreshDiscoveredPaths();
    luban_os_available = luban_client_.IsLubanOsInstalled();
    if (!luban_client_.IsLubanOsInstalled()) {
        ShowToastSafe("未检测到 luban-os，请先点击安装 LubanOS", toast);
        return;
    }
    if (luban_client_.LaunchLubanOsGui()) {
        ShowToastSafe("正在启动 LubanOS 桌面窗口...", toast);
    } else {
        ShowToastSafe("启动 LubanOS 失败，请检查安装状态", toast);
    }
}

void XlingsStore::RefreshSavedLubanStacks(TaskScope tasks) {
    if (skip_initial_refresh) return;
    auto self = shared_from_this();
    tasks.Launch([self]() -> Task<void> {
        std::vector<SavedLubanStackInfo> items = co_await RunWorker([]() {
            return ListSavedLubanStacks();
        });
        self->saved_luban_stacks = std::move(items);
    });
}

bool XlingsStore::SaveLubanStackDraft(
    TaskScope tasks,
    ToastHandle toast,
    std::string name,
    std::string content,
    std::string format
) {
    if (skip_initial_refresh) {
        ShowToastSafe("测试模式下跳过保存本地蓝图", toast);
        return false;
    }
    const auto saved = SaveLubanStackFile(name, content, format);
    if (!saved.has_value()) {
        ShowToastSafe("保存本地蓝图失败", toast);
        return false;
    }
    ShowToastSafe("已保存蓝图: " + mcpplibs::os::fs::utf8(saved->filename()), toast);
    RefreshSavedLubanStacks(tasks);
    return true;
}

void XlingsStore::DeleteSavedLubanStack(
    TaskScope tasks,
    ToastHandle toast,
    std::filesystem::path path,
    std::string display_name
) {
    if (skip_initial_refresh) {
        ShowToastSafe("测试模式下跳过删除本地蓝图", toast);
        return;
    }
    if (DeleteLubanStackFile(path)) {
        ShowToastSafe("已删除蓝图: " + display_name, toast);
        RefreshSavedLubanStacks(tasks);
    } else {
        ShowToastSafe("删除蓝图失败", toast);
    }
}

void XlingsStore::SyncLubanConfigsForStackAsync(
    TaskScope tasks,
    ToastHandle toast,
    std::vector<std::string> env_names
) {
    if (skip_initial_refresh || env_names.empty()) return;
    auto self = shared_from_this();
    LubanClient cli = luban_client_;
    if (!cli.IsAvailable()) cli.RefreshDiscoveredPaths();
    tasks.Launch([self, toast, cli, env_names = std::move(env_names)]() mutable -> Task<void> {
        std::map<std::string, LubanEnvConfigDetail> synced = co_await RunWorker(
            [cli, env_names = std::move(env_names)]() {
                std::map<std::string, LubanEnvConfigDetail> out;
                if (!cli.IsAvailable()) return out;
                for (const auto& name : env_names) {
                    if (auto cfg = cli.QueryEnvConfig(name); cfg.has_value()) {
                        out[name] = std::move(*cfg);
                    }
                }
                return out;
            });
        if (!synced.empty()) {
            self->luban_config_cache.Update([&](std::map<std::string, LubanEnvConfigDetail>& m) {
                for (auto& [k, v] : synced) {
                    m[k] = std::move(v);
                }
            });
            self->ShowToastSafe("已同步现有环境配置，Diff 计划已更新", toast);
        } else {
            self->ShowToastSafe("未发现需要同步的已存在环境配置", toast);
        }
    });
}

void XlingsStore::ApplyLubanStackAsync(
    TaskScope tasks,
    ToastHandle toast,
    LubanStackSpec spec,
    bool dry_run) {
    LubanClient cli = luban_client_;
    if (!cli.IsAvailable()) cli.RefreshDiscoveredPaths();
    const std::string title = dry_run
                                  ? ("预演多环境蓝图: " + spec.name)
                                  : ("应用多环境蓝图: " + spec.name);
    ExecuteOperation(
        tasks,
        toast,
        title,
        [cli, spec, dry_run](auto on_ev, auto on_line, auto ctrl) -> CommandOutcome {
            if (!cli.IsAvailable()) {
                CommandOutcome fail;
                fail.ok = false;
                fail.exit_code = 127;
                fail.error_message = "未检测到 luban 可执行文件，请先安装 luban";
                return fail;
            }

            const std::vector<LubanEnvironment> live_envs = cli.ListEnvironments();
            std::map<std::string, LubanEnvConfigDetail> live_configs;
            for (const auto& desired : spec.environments) {
                const bool exists = std::ranges::any_of(
                    live_envs, [&](const LubanEnvironment& e) { return e.name == desired.name; });
                if (exists) {
                    if (auto cfg = cli.QueryEnvConfig(desired.name); cfg.has_value()) {
                        live_configs[desired.name] = std::move(*cfg);
                    }
                }
            }

            const LubanStackPlan plan = BuildLubanStackPlan(spec, live_envs, live_configs);
            on_line(
                "[编排计划] " + spec.name +
                ": 新建 " + std::to_string(plan.create_count) +
                " / 更新配置 " + std::to_string(plan.configure_count) +
                " / 已就绪 " + std::to_string(plan.unchanged_count)
            );

            if (dry_run) {
                for (const auto& act : plan.actions) {
                    on_line("  - [" + act.env_name + "] " + act.summary);
                }
                CommandOutcome ok_out;
                ok_out.ok = true;
                ok_out.exit_code = 0;
                return ok_out;
            }

            for (const auto& act : plan.actions) {
                if (ctrl && ctrl->cancel_requested.load(std::memory_order_acquire)) {
                    CommandOutcome cancelled;
                    cancelled.ok = false;
                    cancelled.exit_code = 130;
                    cancelled.error_message = "Cancelled";
                    return cancelled;
                }

                if (act.kind == LubanStackActionKind::UpToDate) {
                    on_line("[跳过] " + act.env_name + ": 已就绪且配置一致");
                    continue;
                }

                if (act.kind == LubanStackActionKind::Create) {
                    on_line("[新建] 正在创建环境 " + act.env_name + " ...");
                    LubanCreateOptions opts{
                        .name = act.spec.name,
                        .edition = act.spec.edition,
                        .desktop = act.spec.desktop,
                        .enter_mode = act.spec.enter_mode,
                        .mode = act.spec.mode,
                        .persist = act.spec.persist,
                        .shell = act.spec.shell,
                        .policy = act.spec.policy,
                        .tz = act.spec.tz,
                        .runtime = act.spec.runtime,
                        .user = act.spec.user,
                        .proxy = act.spec.proxy,
                        .carrier = act.spec.carrier,
                        .no_password = act.spec.no_password,
                        .dry_run = false,
                        .xpkgs = act.spec.xpkgs,
                        .allows = act.spec.allows,
                    };
                    CommandOutcome step_out = cli.CreateEnvironment(opts, on_ev, ctrl);
                    if (!step_out.ok) return step_out;

                    LubanConfigPatch post_patch;
                    if (!act.spec.fetch.empty()) post_patch.fetch = act.spec.fetch;
                    if (!act.spec.observe.empty()) post_patch.observe = act.spec.observe;
                    if (!act.spec.mounts.empty()) post_patch.mounts = act.spec.mounts;
                    if (!post_patch.Empty()) {
                        on_line("[配置] 为新建环境 " + act.env_name + " 应用附加策略/挂载...");
                        CommandOutcome post_out = cli.ConfigureEnvironment(act.env_name, post_patch, on_ev, ctrl);
                        if (!post_out.ok) return post_out;
                    }
                    continue;
                }

                if (act.kind == LubanStackActionKind::Configure && !act.config_patch.Empty()) {
                    on_line("[更新] 正在对齐环境 " + act.env_name + " 配置: " + act.summary);
                    CommandOutcome cfg_out = cli.ConfigureEnvironment(act.env_name, act.config_patch, on_ev, ctrl);
                    if (!cfg_out.ok) return cfg_out;
                }
            }

            CommandOutcome done_out;
            done_out.ok = true;
            done_out.exit_code = 0;
            return done_out;
        },
        [dry_run](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
            if (!dry_run) {
                s->loaded_luban_detail_name_.clear();
                s->RefreshLuban(t, true);
            }
        }
    );
}

void XlingsStore::StopLubanStackAsync(TaskScope tasks, ToastHandle toast, LubanStackSpec spec) {
    LubanClient cli = luban_client_;
    if (!cli.IsAvailable()) cli.RefreshDiscoveredPaths();
    ExecuteOperation(
        tasks,
        toast,
        "停止蓝图内环境: " + spec.name,
        [cli, spec](auto on_ev, auto on_line, auto ctrl) -> CommandOutcome {
            if (!cli.IsAvailable()) {
                CommandOutcome fail;
                fail.ok = false;
                fail.exit_code = 127;
                fail.error_message = "未检测到 luban 可执行文件，请先安装 luban";
                return fail;
            }
            const std::vector<LubanEnvironment> live_envs = cli.ListEnvironments();
            for (const auto& desired : spec.environments) {
                if (ctrl && ctrl->cancel_requested.load(std::memory_order_acquire)) {
                    CommandOutcome cancelled;
                    cancelled.ok = false;
                    cancelled.exit_code = 130;
                    cancelled.error_message = "Cancelled";
                    return cancelled;
                }
                const auto it = std::ranges::find_if(
                    live_envs, [&](const LubanEnvironment& e) { return e.name == desired.name; });
                if (it != live_envs.end() && it->running) {
                    on_line("[停止] 正在停止运行中环境 " + desired.name + " ...");
                    CommandOutcome stop_out = cli.StopEnvironment(desired.name, on_ev, ctrl);
                    if (!stop_out.ok) return stop_out;
                } else {
                    on_line("[跳过] " + desired.name + ": 未在运行");
                }
            }
            CommandOutcome done_out;
            done_out.ok = true;
            done_out.exit_code = 0;
            return done_out;
        },
        [](const std::shared_ptr<XlingsStore>& s, TaskScope t, const CommandOutcome&) {
            s->loaded_luban_detail_name_.clear();
            s->RefreshLuban(t, true);
        }
    );
}

} // namespace hxlings

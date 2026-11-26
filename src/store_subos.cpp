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

void XlingsStore::LoadSubosDetail(TaskScope tasks, const std::string& subos_name, const std::string& event_kind) {
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

void XlingsStore::RunSubosExec(TaskScope tasks, ToastHandle toast, std::string subos_name, std::vector<std::string> argv, bool sandbox) {
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

void XlingsStore::CreateSubosAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string custom_dir) {
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

void XlingsStore::SwitchSubosAsync(TaskScope tasks, ToastHandle toast, std::string name) {
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

void XlingsStore::RemoveSubosAsync(TaskScope tasks, ToastHandle toast, std::string name) {
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

void XlingsStore::LoadSubosGovernance(TaskScope tasks, std::string subos_name, bool force) {
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

void XlingsStore::QuerySubosRollbackListAsync(TaskScope tasks, std::string name) {
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

void XlingsStore::DiffSubosAsync(TaskScope tasks, std::string lhs, std::string rhs) {
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

void XlingsStore::RunSubosExecWithOptions(
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

void XlingsStore::StartSubosSessionAsync(TaskScope tasks, ToastHandle toast, std::string name) {
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

void XlingsStore::StopSubosSessionAsync(TaskScope tasks, ToastHandle toast, std::string name) {
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

void XlingsStore::CreateSubosAdvancedAsync(TaskScope tasks, ToastHandle toast, SubosCreateOptions opts) {
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

void XlingsStore::UpdateSubosConfigAsync(TaskScope tasks, ToastHandle toast, std::string name, SubosPolicyPatch patch) {
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

void XlingsStore::ApproveSubosRequestAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string req_id) {
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

void XlingsStore::DenySubosRequestAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string req_id) {
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

void XlingsStore::RunSubosDoctorFixAsync(TaskScope tasks, ToastHandle toast, std::string name) {
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

void XlingsStore::UpgradeSubosAsync(
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

void XlingsStore::RollbackSubosAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string generation) {
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

void XlingsStore::RebindSubosRuntimeAsync(TaskScope tasks, ToastHandle toast, std::string name, std::string binding) {
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

void XlingsStore::ExportSubosAsync(
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

void XlingsStore::PackSubosAsync(
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

void XlingsStore::CopySubosFilesAsync(TaskScope tasks, ToastHandle toast, std::string src, std::string dst) {
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

} // namespace hxlings

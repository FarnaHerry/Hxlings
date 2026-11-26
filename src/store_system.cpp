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

void XlingsStore::RunScriptAsync(
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

void XlingsStore::SetConfigThemeAsync(TaskScope tasks, ToastHandle toast, std::string theme_name) {
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

void XlingsStore::SetConfigRuntimeAsync(TaskScope tasks, ToastHandle toast, std::string runtime_spec) {
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

void XlingsStore::AddArchiveDirAsync(TaskScope tasks, ToastHandle toast, std::string dir_path) {
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

void XlingsStore::RemoveArchiveDirAsync(TaskScope tasks, ToastHandle toast, std::string dir_path) {
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

void XlingsStore::ClearLocalXpkgsAsync(TaskScope tasks, ToastHandle toast, std::string mode) {
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

void XlingsStore::RunSelfMigrateAsync(TaskScope tasks, ToastHandle toast) {
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

void XlingsStore::RunSelfInitAsync(TaskScope tasks, ToastHandle toast, std::string home_dir, bool force) {
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

void XlingsStore::SetMirrorAsync(TaskScope tasks, ToastHandle toast, std::string mirror) {
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

void XlingsStore::SetLangAsync(TaskScope tasks, ToastHandle toast, std::string lang) {
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

void XlingsStore::SetUiModeAsync(TaskScope tasks, ToastHandle toast, std::string mode) {
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

void XlingsStore::SetInteractiveAsync(TaskScope tasks, ToastHandle toast, bool enabled) {
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

void XlingsStore::RunDoctorAsync(TaskScope tasks, ToastHandle toast, bool deep, bool fix, bool dry_run, bool isolation) {
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

void XlingsStore::RunSelfCleanAsync(TaskScope tasks, ToastHandle toast, bool dry_run) {
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

void XlingsStore::RunSelfUpdateAsync(TaskScope tasks, ToastHandle toast) {
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

void XlingsStore::CommitProfileAsync(TaskScope tasks, ToastHandle toast, std::string reason) {
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

void XlingsStore::RollbackProfileAsync(TaskScope tasks, ToastHandle toast, std::string generation) {
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

} // namespace hxlings

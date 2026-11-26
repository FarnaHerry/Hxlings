export module hxlings.page.subos.dialogs;

import std;
import huxerui;
import xlings.types;
import xlings.parser;
import hxlings.theme;
import hxlings.icons;
import hxlings.store;
import hxlings.components;

using namespace huxerui;

export namespace hxlings::subos_detail {


bool IsValidSubosName(std::string_view name) {
    if (name.empty() || name == "current") return false;
    return std::ranges::all_of(name, [](unsigned char ch) {
        return std::isalnum(ch) != 0 || ch == '_' || ch == '-';
    });
}

std::vector<std::string> SplitCommandArgv(std::string_view line) {
    std::vector<std::string> argv;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) != 0) ++i;
        if (i >= line.size()) break;
        std::string token;
        char quote = 0;
        while (i < line.size()) {
            const char ch = line[i];
            if (quote != 0) {
                if (ch == quote) {
                    quote = 0;
                    ++i;
                } else if (ch == '\\' && quote == '"' && i + 1 < line.size()) {
                    token.push_back(line[i + 1]);
                    i += 2;
                } else {
                    token.push_back(ch);
                    ++i;
                }
            } else {
                if (std::isspace(static_cast<unsigned char>(ch)) != 0) break;
                if (ch == '"' || ch == '\'') {
                    quote = ch;
                    ++i;
                } else {
                    token.push_back(ch);
                    ++i;
                }
            }
        }
        if (!token.empty()) argv.push_back(std::move(token));
    }
    return argv;
}

} // namespace hxlings::subos_detail


export namespace hxlings {

[[huxerui::composable]]
View SubosOptionChips(
    const std::vector<std::pair<std::string, std::string>>& options,
    const std::string& selected,
    std::function<void(std::string)> on_select,
    std::string key_prefix
) {
    std::vector<View> items;
    items.reserve(options.size());
    for (std::size_t i = 0; i < options.size(); ++i) {
        const std::string& val = options[i].first;
        const std::string& label = options[i].second;
        const bool is_sel = (selected == val);
        items.push_back(
            ActionButton(
                label,
                is_sel ? ButtonTone::Primary : ButtonTone::Secondary,
                [on_select, val] { on_select(val); },
                true,
                std::nullopt,
                true
            ).Key(key_prefix + "-" + std::to_string(i))
        );
    }
    return Flow(std::move(items)).With(Spacing(6.0F));
}

[[huxerui::composable]]
View NewSubosDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast
) {
    auto name_input = UseState(TextEditingValue::FromText(""));
    auto dir_input = UseState(TextEditingValue::FromText(""));
    auto from_input = UseState(TextEditingValue::FromText(""));
    auto sandbox_opt = UseState(std::string{});
    auto runtime_input = UseState(TextEditingValue::FromText(""));
    auto abi_opt = UseState(std::string{});
    auto storage_opt = UseState(std::string{});
    auto proxy_input = UseState(TextEditingValue::FromText(""));
    auto rootfs_opt = UseState(false);
    auto dry_run_opt = UseState(false);
    auto err_msg = UseState(std::string{});

    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    auto submit = [ctx, store, tasks, toast, name_input, dir_input, from_input, sandbox_opt,
                   runtime_input, abi_opt, storage_opt, proxy_input, rootfs_opt, dry_run_opt, err_msg]() mutable {
        const std::string name = name_input.Get().text;
        const std::string dir = dir_input.Get().text;
        if (!subos_detail::IsValidSubosName(name)) {
            err_msg = std::string("名称仅支持字母、数字、_ 或 -，且不能为 'current'");
            return;
        }
        SubosCreateOptions opts{
            .name = name,
            .from_subos = from_input.Get().text,
            .sandbox = sandbox_opt.Get(),
            .runtime = runtime_input.Get().text,
            .abi = abi_opt.Get(),
            .storage = storage_opt.Get(),
            .proxy = proxy_input.Get().text,
            .rootfs = rootfs_opt.Get(),
            .dry_run = dry_run_opt.Get(),
        };
        const bool has_advanced = !opts.from_subos.empty() || !opts.sandbox.empty() ||
                                  !opts.runtime.empty() || !opts.abi.empty() ||
                                  !opts.storage.empty() || !opts.proxy.empty() ||
                                  opts.rootfs || opts.dry_run;
        ctx.Dismiss();
        if (has_advanced) {
            store->CreateSubosAdvancedAsync(tasks, toast, std::move(opts));
        } else {
            store->CreateSubosAsync(tasks, toast, name, dir);
        }
    };

    const std::vector<std::pair<std::string, std::string>> sandbox_opts{
        {"", "默认"}, {"dev", "dev"}, {"private", "private"}, {"locked", "locked"}
    };
    const std::vector<std::pair<std::string, std::string>> abi_opts{
        {"", "默认"}, {"native", "native"}, {"linux", "linux"}
    };
    const std::vector<std::pair<std::string, std::string>> storage_opts{
        {"", "默认"}, {"shared", "shared"}, {"tmpfs", "tmpfs"}, {"image", "image"}
    };

    View body = ScrollView(
        Column {
            TextField(name_input)
                .Label("SubOS 名称 *")
                .Placeholder("例如: cpp23-dev, rust-lab")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([name_input, err_msg](const TextEditingValue& v) mutable {
                    name_input = v;
                    err_msg = std::string{};
                })
                .OnSubmitted(submit),
            TextField(dir_input)
                .Label("自定义目录 (基础创建可选)")
                .Placeholder("留空默认创建在 $XLINGS_HOME/subos/<name>")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([dir_input](const TextEditingValue& v) mutable { dir_input = v; }),
            Text("高级创建参数 (subos new)", TextRole::Label)
                .Style(TextStyle{.font = Font::System(13.0F).WithWeight(FontWeight::SemiBold)}),
            TextField(from_input)
                .Label("派生来源 (--from)")
                .Placeholder("例如: default 或 subos:luban-core")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([from_input](const TextEditingValue& v) mutable { from_input = v; }),
            Text("隔离预设 (--sandbox)", TextRole::Label),
            SubosOptionChips(
                sandbox_opts,
                sandbox_opt.Get(),
                [sandbox_opt](std::string v) mutable { sandbox_opt = std::move(v); },
                "new-subos-sb"
            ),
            TextField(runtime_input)
                .Label("运行时绑定 (--runtime)")
                .Placeholder("例如: glibc@2.44")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([runtime_input](const TextEditingValue& v) mutable { runtime_input = v; }),
            Text("目标 ABI (--abi)", TextRole::Label),
            SubosOptionChips(
                abi_opts,
                abi_opt.Get(),
                [abi_opt](std::string v) mutable { abi_opt = std::move(v); },
                "new-subos-abi"
            ),
            Text("存储模式 (--storage)", TextRole::Label),
            SubosOptionChips(
                storage_opts,
                storage_opt.Get(),
                [storage_opt](std::string v) mutable { storage_opt = std::move(v); },
                "new-subos-storage"
            ),
            TextField(proxy_input)
                .Label("SOCKS5h 代理 (--proxy)")
                .Placeholder("例如: socks5h://127.0.0.1:1080")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([proxy_input](const TextEditingValue& v) mutable { proxy_input = v; }),
            Checkbox("作为根 SubOS 创建 (--rootfs)", rootfs_opt.Get())
                .OnChanged([rootfs_opt](bool v) mutable { rootfs_opt = v; }),
            Checkbox("仅预览 (--dry-run，配合 --from 查看分层计划)", dry_run_opt.Get())
                .OnChanged([dry_run_opt](bool v) mutable { dry_run_opt = v; }),
            err_msg.Get().empty()
                ? View{}
                : Text(err_msg.Get(), TextRole::Label).Style(TextStyle{.foreground = sp.danger}),
        }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Frame{.max_height = 340.0F}, ScrollBar{});

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton(
            dry_run_opt.Get() ? "预览创建计划" : "创建 SubOS",
            ButtonTone::Primary,
            submit,
            true,
            icons::PlusIcon()
        ),
    }.With(Spacing(10.0F));

    return DialogCardShell("新建 SubOS 子系统", std::move(body), std::move(actions), 520.0F)
        .Key("new-subos-dialog");
}

[[huxerui::composable]]
View SubosDiffDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    std::string default_lhs,
    std::vector<SubosEntry> suboses
) {
    auto lhs_input = UseState(TextEditingValue::FromText(std::move(default_lhs)));
    auto rhs_input = UseState(TextEditingValue::FromText(
        suboses.size() > 1 ? suboses[1].name : std::string("default")));
    const bool loading = store->loading_subos_diff.Get();
    const std::string& diff_text = store->subos_diff_output.Get();

    std::vector<View> quick_picks;
    for (std::size_t i = 0; i < suboses.size(); ++i) {
        const std::string& name = suboses[i].name;
        quick_picks.push_back(
            ActionButton(
                "设为 B: " + name,
                rhs_input.Get().text == name ? ButtonTone::Primary : ButtonTone::Secondary,
                [rhs_input, name]() mutable { rhs_input = TextEditingValue::FromText(name); },
                true,
                std::nullopt,
                true
            ).Key("diff-pick-b-" + std::to_string(i))
        );
    }

    View body = ScrollView(
        Column {
            TextField(lhs_input)
                .Label("基准 SubOS A *")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([lhs_input](const TextEditingValue& v) mutable { lhs_input = v; }),
            TextField(rhs_input)
                .Label("对比 SubOS B *")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([rhs_input](const TextEditingValue& v) mutable { rhs_input = v; }),
            quick_picks.empty() ? View{} : Flow(std::move(quick_picks)).With(Spacing(6.0F)),
            CodeBox(diff_text, 100.0F, 200.0F),
        }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Frame{.max_height = 350.0F}, ScrollBar{});

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("关闭", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        RefreshButton(
            "开始对比",
            [store, tasks, lhs_input, rhs_input] {
                store->DiffSubosAsync(tasks, lhs_input.Get().text, rhs_input.Get().text);
            },
            loading,
            ButtonTone::Primary,
            true,
            false
        ),
    }.With(Spacing(10.0F));

    return DialogCardShell("对比 SubOS 包差异 (subos diff)", std::move(body), std::move(actions), 540.0F)
        .Key("subos-diff-dialog");
}

[[huxerui::composable]]
View SubosCpDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    std::string cur_subos
) {
    auto src_input = UseState(TextEditingValue::FromText(""));
    auto dst_input = UseState(TextEditingValue::FromText(cur_subos + ":/tmp/"));

    auto submit = [ctx, store, tasks, toast, src_input, dst_input]() {
        const std::string src = src_input.Get().text;
        const std::string dst = dst_input.Get().text;
        if (src.empty() || dst.empty()) {
            toast.Show("源路径与目标路径均不能为空");
            return;
        }
        ctx.Dismiss();
        store->CopySubosFilesAsync(tasks, toast, src, dst);
    };

    View body = Column {
        Text("格式：宿主路径 或 <subos_name>:<容器内路径>", TextRole::Body),
        TextField(src_input)
            .Label("源路径 *")
            .Placeholder("/path/on/host 或 default:/etc/file")
            .Variant(TextFieldVariant::Outlined)
            .OnChanged([src_input](const TextEditingValue& v) mutable { src_input = v; }),
        TextField(dst_input)
            .Label("目标路径 *")
            .Placeholder("box:/tmp/file 或 /path/on/host")
            .Variant(TextFieldVariant::Outlined)
            .OnChanged([dst_input](const TextEditingValue& v) mutable { dst_input = v; })
            .OnSubmitted(submit),
    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch));

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton("执行拷贝", ButtonTone::Primary, submit, true, icons::CheckIcon()),
    }.With(Spacing(10.0F));

    return DialogCardShell("宿主与 SubOS 文件互拷 (subos cp)", std::move(body), std::move(actions), 500.0F);
}

[[huxerui::composable]]
View SubosUpgradeDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    std::string cur_subos
) {
    auto to_input = UseState(TextEditingValue::FromText(""));
    auto dry_run_opt = UseState(false);

    auto submit = [ctx, store, tasks, toast, cur_subos, to_input, dry_run_opt]() {
        ctx.Dismiss();
        store->UpgradeSubosAsync(tasks, toast, cur_subos, to_input.Get().text, dry_run_opt.Get());
    };

    View body = Column {
        Text("将基于 edition 的 SubOS `" + cur_subos + "` 升级到新版（用户自带包会保留）。", TextRole::Body),
        TextField(to_input)
            .Label("目标版本 / 替代包 (--to，可留空)")
            .Placeholder("留空则升级至最新版")
            .Variant(TextFieldVariant::Outlined)
            .OnChanged([to_input](const TextEditingValue& v) mutable { to_input = v; }),
        Checkbox("仅预览计划 (--dry-run)", dry_run_opt.Get())
            .OnChanged([dry_run_opt](bool v) mutable { dry_run_opt = v; }),
    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch));

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton(dry_run_opt.Get() ? "预览升级" : "执行升级", ButtonTone::Primary, submit, true, icons::RefreshIcon()),
    }.With(Spacing(10.0F));

    return DialogCardShell("升级 SubOS (" + cur_subos + ")", std::move(body), std::move(actions), 480.0F);
}

[[huxerui::composable]]
View SubosRollbackDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    std::string cur_subos
) {
    auto gen_input = UseState(TextEditingValue::FromText(""));
    const std::string& gen_list = store->subos_rollback_list_output.Get();

    Lifecycle([store, tasks, cur_subos] {
        if (!store->skip_initial_refresh) {
            store->QuerySubosRollbackListAsync(tasks, cur_subos);
        }
    }, std::hash<std::string>{}(cur_subos));

    auto submit = [ctx, store, tasks, toast, cur_subos, gen_input]() {
        ctx.Dismiss();
        store->RollbackSubosAsync(tasks, toast, cur_subos, gen_input.Get().text);
    };

    View body = ScrollView(
        Column {
            Text("根 SubOS `" + cur_subos + "` 的历史代列表 (--list):", TextRole::Label),
            CodeBox(gen_list, 90.0F, 160.0F),
            TextField(gen_input)
                .Label("指定回滚代数 (--to <N>，留空回滚到上一代)")
                .Placeholder("例如: 2")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([gen_input](const TextEditingValue& v) mutable { gen_input = v; })
                .OnSubmitted(submit),
        }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Frame{.max_height = 340.0F}, ScrollBar{});

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton("执行回滚", ButtonTone::Danger, submit, true, icons::RefreshIcon()),
    }.With(Spacing(10.0F));

    return DialogCardShell("回滚根 SubOS (" + cur_subos + ")", std::move(body), std::move(actions), 500.0F);
}

[[huxerui::composable]]
View SubosRuntimeDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    std::string cur_subos
) {
    auto binding_input = UseState(TextEditingValue::FromText(""));

    auto submit = [ctx, store, tasks, toast, cur_subos, binding_input]() {
        const std::string binding = binding_input.Get().text;
        if (binding.empty()) {
            toast.Show("请输入运行时绑定，例如 glibc@2.44");
            return;
        }
        ctx.Dismiss();
        store->RebindSubosRuntimeAsync(tasks, toast, cur_subos, binding);
    };

    View body = Column {
        Text("调用 `xlings subos runtime <binding> " + cur_subos + "` 重新绑定基础运行时。", TextRole::Body),
        TextField(binding_input)
            .Label("运行时绑定 *")
            .Placeholder("例如: glibc@2.39")
            .Variant(TextFieldVariant::Outlined)
            .OnChanged([binding_input](const TextEditingValue& v) mutable { binding_input = v; })
            .OnSubmitted(submit),
    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch));

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton("应用重绑定", ButtonTone::Primary, submit, true, icons::CheckIcon()),
    }.With(Spacing(10.0F));

    return DialogCardShell("重新绑定 SubOS 运行时", std::move(body), std::move(actions), 480.0F);
}

[[huxerui::composable]]
View SubosExportPackDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    std::string cur_subos
) {
    auto mode_opt = UseState(std::string("export"));
    auto format_opt = UseState(std::string("tar"));
    auto path_input = UseState(TextEditingValue::FromText("./" + cur_subos + "-rootfs.tar"));
    auto size_input = UseState(TextEditingValue::FromText("4G"));
    auto with_data_opt = UseState(false);
    auto as_pkg_input = UseState(TextEditingValue::FromText("custom:" + cur_subos + "@0.1.0"));
    auto out_dir_input = UseState(TextEditingValue::FromText("."));

    const std::string mode = mode_opt.Get();
    const std::string format = format_opt.Get();

    const std::vector<std::pair<std::string, std::string>> modes{
        {"export", "导出根镜像 (subos export)"}, {"pack", "打包为 xpkg (subos pack)"}
    };
    const std::vector<std::pair<std::string, std::string>> formats{
        {"tar", "--tar"}, {"rootfs", "--rootfs"}, {"disk", "--disk"}
    };

    auto submit = [ctx, store, tasks, toast, cur_subos, mode, format,
                   path_input, size_input, with_data_opt, as_pkg_input, out_dir_input]() {
        if (mode == "export") {
            const std::string out_path = path_input.Get().text;
            if (out_path.empty()) {
                toast.Show("请输入导出目标路径");
                return;
            }
            ctx.Dismiss();
            store->ExportSubosAsync(
                tasks, toast, cur_subos, format, out_path, size_input.Get().text, with_data_opt.Get());
        } else {
            ctx.Dismiss();
            store->PackSubosAsync(tasks, toast, cur_subos, as_pkg_input.Get().text, out_dir_input.Get().text);
        }
    };

    View detail_fields = (mode == "export")
                             ? Column {
                                   Text("导出格式", TextRole::Label),
                                   SubosOptionChips(
                                       formats,
                                       format,
                                       [format_opt](std::string v) mutable { format_opt = std::move(v); },
                                       "exp-fmt"
                                   ),
                                   TextField(path_input)
                                       .Label("输出路径 *")
                                       .Variant(TextFieldVariant::Outlined)
                                       .OnChanged([path_input](const TextEditingValue& v) mutable { path_input = v; }),
                                   format == "disk"
                                       ? TextField(size_input)
                                             .Label("磁盘镜像大小 (--size)")
                                             .Variant(TextFieldVariant::Outlined)
                                             .OnChanged([size_input](const TextEditingValue& v) mutable { size_input = v; })
                                       : View{},
                                   Checkbox("包含宿主数据目录 (--with-data)", with_data_opt.Get())
                                       .OnChanged([with_data_opt](bool v) mutable { with_data_opt = v; }),
                               }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
                             : Column {
                                   TextField(as_pkg_input)
                                       .Label("目标包声明 (--as <NS:NAME@VERSION>)")
                                       .Variant(TextFieldVariant::Outlined)
                                       .OnChanged([as_pkg_input](const TextEditingValue& v) mutable { as_pkg_input = v; }),
                                   TextField(out_dir_input)
                                       .Label("输出目录 (--out <DIR>)")
                                       .Variant(TextFieldVariant::Outlined)
                                       .OnChanged([out_dir_input](const TextEditingValue& v) mutable { out_dir_input = v; }),
                               }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch));

    View body = ScrollView(
        Column {
            SubosOptionChips(
                modes,
                mode,
                [mode_opt](std::string v) mutable { mode_opt = std::move(v); },
                "exp-mode"
            ),
            std::move(detail_fields),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Frame{.max_height = 330.0F}, ScrollBar{});

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton(
            mode == "export" ? "开始导出" : "开始打包",
            ButtonTone::Primary,
            submit,
            true,
            icons::DownloadIcon()
        ),
    }.With(Spacing(10.0F));

    return DialogCardShell("导出 / 打包 SubOS (" + cur_subos + ")", std::move(body), std::move(actions), 520.0F)
        .Key("subos-export-dialog");
}

} // namespace hxlings

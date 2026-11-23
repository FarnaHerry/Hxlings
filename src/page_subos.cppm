export module hxlings.page.subos;

import std;
import huxerui;
import xlings.types;
import xlings.parser;
import hxlings.theme;
import hxlings.icons;
import hxlings.store;
import hxlings.components;

using namespace huxerui;

namespace hxlings::detail {

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

} // namespace hxlings::detail

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
        if (!detail::IsValidSubosName(name)) {
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
        ActionButton(
            loading ? "对比中..." : "开始对比",
            ButtonTone::Primary,
            [store, tasks, lhs_input, rhs_input] {
                store->DiffSubosAsync(tasks, lhs_input.Get().text, rhs_input.Get().text);
            },
            !loading,
            icons::RefreshIcon()
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

[[huxerui::composable]]
View SubosPolicyTabView(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    DialogHandle dialog,
    std::string cur_name
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const auto& st = store->selected_subos_status.Get();
    const auto& req_pol = store->selected_subos_policy.Get();
    const auto& requests = store->selected_subos_requests.Get();
    const bool loading = store->loading_subos_governance.Get();

    const SubosPolicySnapshot& base_pol = !req_pol.source.empty() || !req_pol.net.empty() || !req_pol.extends.empty()
                                              ? req_pol
                                              : st.requested;

    auto sandbox_edit = UseState(base_pol.extends);
    auto net_edit = UseState(base_pol.net);
    auto proxy_edit = UseState(TextEditingValue::FromText(base_pol.proxy));
    auto fetch_edit = UseState(base_pol.fetch_default);
    auto idx_edit = UseState(base_pol.index_update);
    auto observe_edit = UseState(base_pol.observe_level);
    auto allow_edit = UseState(TextEditingValue::FromText(""));
    auto disallow_edit = UseState(TextEditingValue::FromText(""));
    auto env_pass_edit = UseState(TextEditingValue::FromText(""));
    auto mount_edit = UseState(TextEditingValue::FromText(""));
    auto unmount_edit = UseState(TextEditingValue::FromText(""));
    auto no_degrade_edit = UseState(base_pol.no_degrade);

    const std::size_t pol_hash = std::hash<std::string>{}(
        cur_name + "|" + base_pol.source + "|" + base_pol.extends + "|" + base_pol.net + "|" +
        base_pol.proxy + "|" + base_pol.fetch_default + "|" + base_pol.index_update + "|" +
        base_pol.observe_level + (base_pol.no_degrade ? "|1" : "|0")
    );

    Lifecycle([store, tasks, cur_name] {
        if (!store->skip_initial_refresh) {
            store->LoadSubosGovernance(tasks, cur_name, false);
        }
    }, std::hash<std::string>{}(cur_name));

    Lifecycle([base_pol, sandbox_edit, net_edit, proxy_edit, fetch_edit,
               idx_edit, observe_edit, no_degrade_edit]() mutable {
        sandbox_edit = base_pol.extends;
        net_edit = base_pol.net;
        proxy_edit = TextEditingValue::FromText(base_pol.proxy);
        fetch_edit = base_pol.fetch_default;
        idx_edit = base_pol.index_update;
        observe_edit = base_pol.observe_level;
        no_degrade_edit = base_pol.no_degrade;
    }, pol_hash);

    std::vector<View> gate_rows;
    for (std::size_t i = 0; i < st.gates.size(); ++i) {
        const auto& g = st.gates[i];
        gate_rows.push_back(
            Row {
                Column {
                    Row {
                        Text(g.gate, TextRole::Body)
                            .Style(TextStyle{.font = Font::Monospace(12.5F).WithWeight(FontWeight::SemiBold)}),
                        StatusBadge(
                            g.enforced ? "已实施" : "未实施",
                            g.enforced ? sp.success : sp.warning,
                            g.enforced ? sp.success_bg : sp.warning_bg
                        ),
                        StatusBadge(
                            g.supported ? "支持" : "不支持",
                            g.supported ? sp.info : sp.danger,
                            g.supported ? sp.info_bg : sp.danger_bg
                        ),
                    }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                    (g.route.empty() && g.reason.empty())
                        ? View{}
                        : Text(
                              (g.route.empty() ? "" : ("路径: " + g.route)) +
                                  (!g.route.empty() && !g.reason.empty() ? " · " : "") +
                                  g.reason,
                              TextRole::Label
                          ).Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                             .With(Grow(1.0F)),
                }.With(Spacing(4.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                Background(theme.colors.surface_container_low),
                CornerRadius(8.0F)
            ).Key("subos-gate-" + std::to_string(i))
        );
    }

    std::vector<View> req_rows;
    for (std::size_t i = 0; i < requests.size(); ++i) {
        const auto& r = requests[i];
        req_rows.push_back(
            Row {
                Column {
                    Row {
                        StatusBadge(r.kind.empty() ? std::string("req") : r.kind, sp.warning, sp.warning_bg),
                        Text("#" + r.id + " " + r.resource, TextRole::Body)
                            .Style(TextStyle{.font = Font::Monospace(12.5F).WithWeight(FontWeight::SemiBold)})
                            .With(Grow(1.0F)),
                    }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                    (r.detail.empty() && r.requested_at.empty())
                        ? View{}
                        : Text(
                              (r.requested_at.empty() ? std::string{} : FormatLocalTimestamp(r.requested_at)) +
                                  (!r.requested_at.empty() && !r.detail.empty() ? " · " : "") +
                                  r.detail,
                              TextRole::Label
                          ).Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                             .With(Grow(1.0F)),
                }.With(Spacing(4.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                Flow {
                    ActionButton(
                        "批准",
                        ButtonTone::Primary,
                        [store, tasks, toast, cur_name, id = r.id] {
                            store->ApproveSubosRequestAsync(tasks, toast, cur_name, id);
                        },
                        !r.id.empty(),
                        icons::CheckIcon(),
                        true
                    ),
                    ActionButton(
                        "拒绝",
                        ButtonTone::Danger,
                        [store, tasks, toast, cur_name, id = r.id] {
                            store->DenySubosRequestAsync(tasks, toast, cur_name, id);
                        },
                        !r.id.empty(),
                        icons::TrashIcon(),
                        true
                    ),
                }.With(Spacing(6.0F)),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                Spacing(8.0F),
                CrossAlign(CrossAxisAlignment::Center),
                Background(theme.colors.surface_container_low),
                CornerRadius(8.0F)
            ).Key("subos-req-" + std::to_string(i))
        );
    }

    const std::vector<std::pair<std::string, std::string>> sb_opts{
        {"dev", "dev"}, {"private", "private"}, {"locked", "locked"}
    };
    const std::vector<std::pair<std::string, std::string>> net_opts{
        {"host", "host"}, {"nat", "nat"}, {"none", "none"}, {"proxy", "proxy"}
    };
    const std::vector<std::pair<std::string, std::string>> action_opts{
        {"auto", "auto"}, {"ask", "ask"}, {"deny", "deny"}
    };
    const std::vector<std::pair<std::string, std::string>> obs_opts{
        {"off", "off"}, {"basic", "basic"}, {"standard", "standard"}, {"full", "full"}
    };

    return ScrollView(
        Column {
            Card(
                Column {
                    SectionHeader(
                        "隔离状态与内核门控 (subos status)",
                        "后端: " + (st.backend.empty() ? std::string("未加载") : st.backend) +
                            " · 策略来源: " + (st.policy_source.empty() ? std::string("默认") : st.policy_source),
                        ActionButton(
                            loading ? "加载中..." : "刷新策略",
                            ButtonTone::Secondary,
                            [store, tasks, cur_name] { store->LoadSubosGovernance(tasks, cur_name, true); },
                            !loading,
                            icons::RefreshIcon(),
                            true
                        )
                    ),
                    Flow {
                        StatusBadge(
                            st.enters ? "允许进入 (enters: true)" : "进入受阻 / 未检查",
                            st.enters ? sp.success : sp.warning,
                            st.enters ? sp.success_bg : sp.warning_bg
                        ),
                        base_pol.extends.empty()
                            ? View{}
                            : StatusBadge("继承: " + base_pol.extends, sp.info, sp.info_bg),
                        base_pol.net.empty()
                            ? View{}
                            : StatusBadge("网络: " + base_pol.net, sp.info, sp.info_bg),
                        base_pol.observe_level.empty()
                            ? View{}
                            : StatusBadge("观测: " + base_pol.observe_level, sp.info, sp.info_bg),
                    }.With(Spacing(6.0F)),
                    gate_rows.empty()
                        ? Text("暂无门控详情（点击右上角刷新策略查询实时状态）", TextRole::Body)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        : View(Column(std::move(gate_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader(
                        "沙箱策略编辑器 (subos config)",
                        "修改当前 SubOS 的网络、权限、观测、环境变量透传与目录挂载策略"
                    ),
                    Text("沙箱预设 (--sandbox)", TextRole::Label),
                    SubosOptionChips(
                        sb_opts, sandbox_edit.Get(), [sandbox_edit](std::string v) mutable { sandbox_edit = std::move(v); }, "pol-sb"),
                    Text("网络模式 (--net)", TextRole::Label),
                    SubosOptionChips(
                        net_opts, net_edit.Get(), [net_edit](std::string v) mutable { net_edit = std::move(v); }, "pol-net"),
                    TextField(proxy_edit)
                        .Label("代理端点 (--proxy)")
                        .Placeholder("例如: socks5h://127.0.0.1:1080")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([proxy_edit](const TextEditingValue& v) mutable { proxy_edit = v; }),
                    Text("缺失包自动获取 (--fetch)", TextRole::Label),
                    SubosOptionChips(
                        action_opts, fetch_edit.Get(), [fetch_edit](std::string v) mutable { fetch_edit = std::move(v); }, "pol-fetch"),
                    Text("沙箱内索引更新 (--index-update)", TextRole::Label),
                    SubosOptionChips(
                        action_opts, idx_edit.Get(), [idx_edit](std::string v) mutable { idx_edit = std::move(v); }, "pol-idx"),
                    Text("审计观测级别 (--observe)", TextRole::Label),
                    SubosOptionChips(
                        obs_opts, observe_edit.Get(), [observe_edit](std::string v) mutable { observe_edit = std::move(v); }, "pol-obs"),
                    TextField(allow_edit)
                        .Label("授予能力 (--allow: display/audio/camera/gpu/ssh-agent)")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([allow_edit](const TextEditingValue& v) mutable { allow_edit = v; }),
                    TextField(disallow_edit)
                        .Label("撤销能力 (--disallow)")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([disallow_edit](const TextEditingValue& v) mutable { disallow_edit = v; }),
                    TextField(env_pass_edit)
                        .Label("放行宿主环境变量 (--env-pass，逗号分隔)")
                        .Placeholder("例如: HTTP_PROXY,HTTPS_PROXY,TERM")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([env_pass_edit](const TextEditingValue& v) mutable { env_pass_edit = v; }),
                    TextField(mount_edit)
                        .Label("持久挂载宿主路径 (--mount HOST[:INSIDE][:ro|rw])")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([mount_edit](const TextEditingValue& v) mutable { mount_edit = v; }),
                    TextField(unmount_edit)
                        .Label("移除挂载路径 (--unmount PATH)")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([unmount_edit](const TextEditingValue& v) mutable { unmount_edit = v; }),
                    Checkbox("缺失任意强约束门控时禁止降级进入 (--no-degrade)", no_degrade_edit.Get())
                        .OnChanged([no_degrade_edit](bool v) mutable { no_degrade_edit = v; }),
                    Flow {
                        ActionButton(
                            "应用策略更改",
                            ButtonTone::Primary,
                            [store, tasks, toast, cur_name, sandbox_edit, net_edit, proxy_edit, fetch_edit,
                             idx_edit, observe_edit, allow_edit, disallow_edit, env_pass_edit,
                             mount_edit, unmount_edit, no_degrade_edit] {
                                SubosPolicyPatch patch{
                                    .sandbox = sandbox_edit.Get(),
                                    .net = net_edit.Get(),
                                    .proxy = proxy_edit.Get().text,
                                    .fetch_default = fetch_edit.Get(),
                                    .index_update = idx_edit.Get(),
                                    .observe_level = observe_edit.Get(),
                                    .allow_cap = allow_edit.Get().text,
                                    .disallow_cap = disallow_edit.Get().text,
                                    .env_pass_csv = env_pass_edit.Get().text,
                                    .mount_spec = mount_edit.Get().text,
                                    .unmount_spec = unmount_edit.Get().text,
                                    .no_degrade = no_degrade_edit.Get(),
                                };
                                store->UpdateSubosConfigAsync(tasks, toast, cur_name, std::move(patch));
                            },
                            true,
                            icons::CheckIcon(),
                            true
                        ),
                        ActionButton(
                            "升级策略包 (--policy-upgrade)",
                            ButtonTone::Secondary,
                            [store, tasks, toast, cur_name] {
                                SubosPolicyPatch patch;
                                patch.policy_upgrade = true;
                                store->UpdateSubosConfigAsync(tasks, toast, cur_name, std::move(patch));
                            },
                            true,
                            icons::RefreshIcon(),
                            true
                        ),
                        ActionButton(
                            "重置策略 (--reset)",
                            ButtonTone::Danger,
                            [dialog, store, tasks, toast, cur_name] {
                                dialog.Show(
                                    "重置 SubOS 策略",
                                    "确定删除 `" + cur_name + "` 的自定义策略文件并恢复默认策略吗？",
                                    "确认重置",
                                    "取消",
                                    [store, tasks, toast, cur_name] {
                                        SubosPolicyPatch patch;
                                        patch.reset = true;
                                        store->UpdateSubosConfigAsync(tasks, toast, cur_name, std::move(patch));
                                    },
                                    {}
                                );
                            },
                            true,
                            icons::TrashIcon(),
                            true
                        ),
                    }.With(Spacing(8.0F)),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader(
                        "待审批沙箱请求 (" + std::to_string(requests.size()) + ")",
                        "来自分离沙箱内部的操作申请 (`subos requests` / `approve` / `deny`)"
                    ),
                    req_rows.empty()
                        ? Text("当前没有等待审批的沙箱请求", TextRole::Body)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        : View(Column(std::move(req_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Grow(1.0F), ScrollBar{});
}

[[huxerui::composable]]
View SubosSessionsTabView(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    std::string cur_name
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const auto& sessions = store->selected_subos_sessions.Get();
    const auto& doctor = store->subos_doctor_snapshot.Get();
    const auto& report = store->selected_subos_report.Get();
    const std::string& info_text = store->selected_subos_info.Get();
    const bool loading = store->loading_subos_governance.Get();

    Lifecycle([store, tasks, cur_name] {
        if (!store->skip_initial_refresh) {
            store->LoadSubosGovernance(tasks, cur_name, false);
        }
    }, std::hash<std::string>{}(cur_name));

    std::vector<View> ps_rows;
    for (std::size_t i = 0; i < sessions.size(); ++i) {
        const auto& s = sessions[i];
        ps_rows.push_back(
            Row {
                Column {
                    Row {
                        Text(s.id.empty() ? std::string("session") : s.id, TextRole::Body)
                            .Style(TextStyle{.font = Font::Monospace(12.5F).WithWeight(FontWeight::SemiBold)}),
                        s.backend.empty() ? View{} : StatusBadge(s.backend, sp.info, sp.info_bg),
                        s.state.empty() ? View{} : StatusBadge(s.state, sp.success, sp.success_bg),
                    }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Text(
                        "PID: " + std::to_string(s.pid) +
                            (s.started.empty() ? "" : (" · 启动: " + FormatLocalTimestamp(s.started))) +
                            (s.command.empty() ? "" : (" · " + s.command)),
                        TextRole::Label
                    ).Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        .With(Grow(1.0F)),
                }.With(Spacing(4.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                Background(theme.colors.surface_container_low),
                CornerRadius(8.0F)
            ).Key("subos-ps-" + std::to_string(i))
        );
    }

    std::vector<View> finding_rows;
    for (std::size_t i = 0; i < doctor.findings.size(); ++i) {
        const auto& f = doctor.findings[i];
        const bool is_err = (f.level == "error" || f.level == "fatal");
        finding_rows.push_back(
            Row {
                Column {
                    Row {
                        StatusBadge(
                            f.level.empty() ? std::string("info") : f.level,
                            is_err ? sp.danger : sp.warning,
                            is_err ? sp.danger_bg : sp.warning_bg
                        ),
                        Text((f.instance.empty() ? "" : (f.instance + " · ")) + f.check, TextRole::Body)
                            .Style(TextStyle{.font = Font::System(13.0F).WithWeight(FontWeight::SemiBold)})
                            .With(Grow(1.0F)),
                    }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                    f.detail.empty()
                        ? View{}
                        : Text(f.detail, TextRole::Label)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                              .With(Grow(1.0F)),
                    f.fix.empty()
                        ? View{}
                        : Text("建议修复: " + f.fix, TextRole::Label)
                              .Style(TextStyle{.foreground = theme.colors.primary})
                              .With(Grow(1.0F)),
                }.With(Spacing(4.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                Background(theme.colors.surface_container_low),
                CornerRadius(8.0F)
            ).Key("subos-doc-" + std::to_string(i))
        );
    }

    std::vector<View> report_rows;
    for (std::size_t i = 0; i < report.sessions.size(); ++i) {
        const auto& rs = report.sessions[i];
        report_rows.push_back(
            Row {
                Column {
                    Row {
                        Text(rs.id.empty() ? std::string("session") : rs.id, TextRole::Body)
                            .Style(TextStyle{.font = Font::Monospace(12.5F).WithWeight(FontWeight::SemiBold)}),
                        StatusBadge("exit " + std::to_string(rs.exit_code),
                                    rs.exit_code == 0 ? sp.success : sp.danger,
                                    rs.exit_code == 0 ? sp.success_bg : sp.danger_bg),
                        StatusBadge(std::to_string(rs.duration_ms) + " ms", sp.info, sp.info_bg),
                    }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Text(
                        (rs.started.empty() ? std::string{} : ("开始: " + FormatLocalTimestamp(rs.started) + " · ")) +
                            "命令: " + std::to_string(rs.commands) +
                            " · 执行: " + std::to_string(rs.executions) +
                            " · 拒绝: " + std::to_string(rs.denied.size()) +
                            " · 变更文件: " + std::to_string(rs.changed.size()),
                        TextRole::Label
                    ).Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        .With(Grow(1.0F)),
                }.With(Spacing(4.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                Background(theme.colors.surface_container_low),
                CornerRadius(8.0F)
            ).Key("subos-rep-" + std::to_string(i))
        );
    }

    return ScrollView(
        Column {
            Card(
                Column {
                    SectionHeader(
                        "运行中会话 (subos ps)",
                        "共 " + std::to_string(sessions.size()) + " 个活跃后台会话",
                        ActionButton(
                            loading ? "刷新中..." : "刷新数据",
                            ButtonTone::Secondary,
                            [store, tasks, cur_name] { store->LoadSubosGovernance(tasks, cur_name, true); },
                            !loading,
                            icons::RefreshIcon(),
                            true
                        )
                    ),
                    ps_rows.empty()
                        ? Text("当前没有运行中的 SubOS 会话", TextRole::Body)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        : View(Column(std::move(ps_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader(
                        "SubOS 诊断 (subos doctor)",
                        "错误数: " + std::to_string(doctor.errors) + " · 检测项: " + std::to_string(doctor.findings.size()),
                        ActionButton(
                            "修复策略包 (--fix)",
                            ButtonTone::Primary,
                            [store, tasks, toast, cur_name] { store->RunSubosDoctorFixAsync(tasks, toast, cur_name); },
                            true,
                            icons::WrenchIcon(),
                            true
                        )
                    ),
                    finding_rows.empty()
                        ? Text("诊断未报告异常发现", TextRole::Body)
                              .Style(TextStyle{.foreground = sp.success})
                        : View(Column(std::move(finding_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader("SubOS 环境详情面板", "汇总命令数、已安装包数、GL 分发与异常告警 (来自 subos_info)"),
                    CodeBox(info_text, 90.0F, 180.0F),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader(
                        "会话与变更报告 (subos report)",
                        "策略变更 " + std::to_string(report.policy_changes.size()) +
                            " 次 · 历史会话 " + std::to_string(report.sessions.size()) + " 条"
                    ),
                    report_rows.empty()
                        ? Text("暂无历史会话报告记录", TextRole::Body)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        : View(Column(std::move(report_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Grow(1.0F), ScrollBar{});
}

[[huxerui::composable]]
View SubosPage() {
    const auto store = UseService<XlingsStore>();
    const TaskScope tasks = UseApplicationTaskScope();
    const TaskScope ui_tasks = UseTaskScope();
    const ToastHandle toast = UseToast();
    const DialogHandle dialog = UseDialog();
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const auto clipboard = UseApplication().Clipboard();

    auto detail_tab = UseState<std::size_t>(0);
    auto shim_filter = UseState(TextEditingValue::FromText(""));
    auto exec_cmd = UseState(TextEditingValue::FromText("xlings --version"));
    auto exec_sandbox = UseState(false);
    auto exec_cwd = UseState(TextEditingValue::FromText(""));
    auto exec_timeout = UseState(TextEditingValue::FromText(""));
    auto event_kind = UseState(std::string{});
    const ViewHandle shim_filter_handle = UseViewHandle();

    Lifecycle([store, ui_tasks, shim_filter_handle] {
        if (store->active_nav.Get() == 3 && store->ConsumeFocusSearchNonce()) {
            ui_tasks.Post([shim_filter_handle] {
                (void)shim_filter_handle.RequestFocus(true);
            });
        }
    }, store->focus_search_nonce.Get());

    Lifecycle([store, tasks] {
        if (!store->skip_initial_refresh && store->active_nav.Get() == 3) {
            store->EnsureSubosDetailLoaded(tasks, false);
        }
    }, store->active_nav.Get(), store->subos_list.Get().size());

    const auto& suboses = store->subos_list.Get();
    const std::string selected_name = store->selected_subos.Get();
    const auto& shims = store->active_shims.Get();
    const auto& env_vars = store->selected_subos_env.Get();
    const auto& events = store->selected_subos_events.Get();
    const std::string& exec_out = store->subos_exec_output.Get();
    const bool loading_detail = store->loading_subos_detail.Get();
    const bool running_exec = store->running_subos_exec.Get();

    const SubosEntry* selected_entry = nullptr;
    for (const auto& s : suboses) {
        if (s.name == selected_name) {
            selected_entry = &s;
            break;
        }
    }

    View toolbar = Row {
        Column {
            Text("SubOS 子系统环境", TextRole::Title)
                .Style(TextStyle{
                    .font = Font::System(18.0F).WithWeight(FontWeight::Bold),
                    .foreground = theme.colors.on_surface,
                }),
            Text("每个 SubOS 拥有独立工具版本映射与环境变量隔离，底层软件载荷全局共享", TextRole::Label)
                .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
        }.With(Spacing(3.0F), Grow(1.0F)),
        Flow {
            ActionButton(
                "对比 Diff",
                ButtonTone::Secondary,
                [dialog, store, tasks, selected_name, suboses] {
                    dialog.Show([store, tasks, selected_name, suboses](DialogContext ctx) {
                        return SubosDiffDialogContent(ctx, store, tasks, selected_name, suboses);
                    });
                },
                !suboses.empty(),
                icons::RefreshIcon(),
                true
            ),
            ActionButton(
                "拷贝文件",
                ButtonTone::Secondary,
                [dialog, store, tasks, toast, selected_name] {
                    dialog.Show([store, tasks, toast, selected_name](DialogContext ctx) {
                        return SubosCpDialogContent(
                            ctx, store, tasks, toast,
                            selected_name.empty() ? std::string("default") : selected_name);
                    });
                },
                !suboses.empty(),
                icons::CopyIcon(),
                true
            ),
            ActionButton(
                "新建 SubOS",
                ButtonTone::Primary,
                [dialog, store, tasks, toast] {
                    dialog.Show([store, tasks, toast](DialogContext ctx) {
                        return NewSubosDialogContent(ctx, store, tasks, toast);
                    });
                },
                true,
                icons::PlusIcon(),
                false
            ),
        }.With(Spacing(8.0F)),
    }.With(
        Padding(EdgeInsets::Symmetric(20.0F, 12.0F)),
        Spacing(12.0F),
        Background(theme.colors.surface),
        CrossAlign(CrossAxisAlignment::Center)
    );

    std::vector<View> subos_cards;
    for (std::size_t i = 0; i < suboses.size(); ++i) {
        const auto& s = suboses[i];
        const bool is_sel = s.name == selected_name;
        const bool can_delete = !s.active && s.name != "default";

        subos_cards.push_back(
            Column {
                Row {
                    Text(s.name, TextRole::Title)
                        .Style(TextStyle{
                            .font = Font::System(15.0F).WithWeight(FontWeight::Bold),
                            .foreground = is_sel ? theme.colors.primary : theme.colors.on_surface,
                        })
                        .With(Grow(1.0F)),
                    s.active ? StatusBadge("当前活跃", sp.success, sp.success_bg) : View{},
                }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                Row {
                    StatusBadge(std::to_string(s.packages) + " 包", sp.info, sp.info_bg),
                    StatusBadge(std::to_string(s.commands) + " 命令", theme.colors.on_surface_variant, sp.muted_bg),
                    s.abi.empty() ? View{} : StatusBadge(s.abi, theme.colors.on_surface_variant, sp.muted_bg),
                }.With(Spacing(6.0F)),
                Text(s.dir, TextRole::Label)
                    .Style(TextStyle{.font = Font::Monospace(11.5F), .foreground = theme.colors.on_surface_variant}),
                Row {
                    !s.active
                        ? ActionButton(
                              "设为活跃",
                              ButtonTone::Primary,
                              [store, tasks, toast, name = s.name] {
                                  store->SwitchSubosAsync(tasks, toast, name);
                              },
                              true,
                              icons::CheckIcon(),
                              true
                          )
                        : StatusBadge("已激活", sp.success, sp.success_bg),
                    Spacer{}.With(Grow(1.0F)),
                    ActionButton(
                        "删除",
                        ButtonTone::Danger,
                        [store, tasks, toast, dialog, name = s.name] {
                            dialog.Show(
                                "删除 SubOS",
                                "确定要删除 SubOS `" + name + "` 及其主目录中的所有文件吗？此操作不可撤销。",
                                "删除",
                                "取消",
                                [store, tasks, toast, name] { store->RemoveSubosAsync(tasks, toast, name); },
                                {}
                            );
                        },
                        can_delete,
                        icons::TrashIcon(),
                        true
                    ),
                }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
            }.With(
                Padding(14.0F),
                Spacing(8.0F),
                Background(is_sel ? theme.colors.primary_container : sp.card_bg),
                Border{is_sel ? theme.colors.primary : sp.border_soft, is_sel ? 1.5F : 1.0F},
                CornerRadius(12.0F),
                Focusable(true),
                PointerCursor(PointerCursorKind::Hand),
                CrossAlign(CrossAxisAlignment::Stretch)
            )
            .OnClick([store, tasks, event_kind, name = s.name] {
                store->LoadSubosDetail(tasks, name, event_kind.Get());
            })
            .Key("subos-" + s.name + "-" + std::to_string(i))
        );
    }

    View left_pane = subos_cards.empty()
                         ? EmptyState(icons::SubosIcon(), "未发现 SubOS", "点击右上角新建一个 SubOS")
                         : View(ScrollView(
                               Column(std::move(subos_cards))
                                   .With(Padding(14.0F), Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
                           ).ScrollAxis(Axis::Vertical)
                                    .With(Grow(1.0F), ScrollBar{}));

    View right_pane = View{};
    if (selected_entry == nullptr) {
        right_pane = EmptyState(icons::SubosIcon(), "选择一个 SubOS 查看详情");
    } else {
        const SubosEntry cur = *selected_entry;
        const std::size_t tab_idx = detail_tab.Get();

        View tab_content = View{};
        if (tab_idx == 0) {
            if (!cur.active) {
                tab_content = EmptyState(
                    icons::TerminalIcon(),
                    "该 SubOS 当前未激活",
                    "xlings list_subos_shims 仅返回当前活跃 SubOS 的 Shims；点击左侧“设为活跃”可激活并查看"
                );
            } else {
                std::string needle = shim_filter.Get().text;
                for (char& ch : needle) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

                std::vector<View> shim_chips;
                for (std::size_t i = 0; i < shims.size(); ++i) {
                    const auto& sh = shims[i];
                    if (!needle.empty()) {
                        std::string low = sh;
                        for (char& ch : low) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                        if (low.find(needle) == std::string::npos) continue;
                    }
                    shim_chips.push_back(
                        ActionButton(
                            sh,
                            ButtonTone::Secondary,
                            [sh, clipboard, toast] {
                                if (clipboard && clipboard->SupportsSynchronousText() &&
                                    clipboard->WriteText(sh)) {
                                    toast.Show("已复制命令: " + sh);
                                } else {
                                    toast.Show("复制到剪贴板失败");
                                }
                            },
                            true,
                            std::nullopt,
                            true
                        ).Key("shim-" + sh + "-" + std::to_string(i))
                    );
                }

                tab_content = Column {
                    Row {
                        TextField(shim_filter)
                            .Placeholder("过滤 Shims 命令名...")
                            .Variant(TextFieldVariant::Outlined)
                            .LeadingIcon(icons::SearchIcon())
                            .OnChanged([shim_filter](const TextEditingValue& v) mutable { shim_filter = v; })
                            .With(Grow(1.0F), shim_filter_handle),
                        StatusBadge("共 " + std::to_string(shims.size()) + " 个", sp.info, sp.info_bg),
                    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Text("点击任意命令可复制到剪贴板", TextRole::Label)
                        .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
                    ScrollView(
                        Flow(std::move(shim_chips)).With(Spacing(6.0F), Padding(4.0F))
                    ).ScrollAxis(Axis::Vertical)
                        .With(Grow(1.0F), ScrollBar{}),
                }.With(Grow(1.0F), Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch));
            }
        } else if (tab_idx == 1) {
            if (env_vars.empty()) {
                tab_content = EmptyState(
                    icons::SettingsIcon(),
                    "无额外环境变量注入",
                    "当前 SubOS 未通过包声明额外的环境变量 (subos_env 为空)"
                );
            } else {
                std::vector<View> var_rows;
                for (std::size_t i = 0; i < env_vars.size(); ++i) {
                    const auto& ev = env_vars[i];
                    var_rows.push_back(
                        Row {
                            StatusBadge(ev.op, sp.info, sp.info_bg),
                            Text(ev.var, TextRole::Body)
                                .Style(TextStyle{.font = Font::Monospace(12.5F).WithWeight(FontWeight::Bold),
                                                 .foreground = theme.colors.on_surface})
                                .With(Frame{.width = 180.0F}),
                            SelectionArea(
                                Text(ev.value, TextRole::Body)
                                    .Style(TextStyle{.font = Font::Monospace(12.5F), .foreground = theme.colors.on_surface_variant})
                            ).With(Grow(1.0F)),
                        }.With(
                            Padding(10.0F),
                            Spacing(10.0F),
                            Background(sp.card_elevated_bg),
                            CornerRadius(8.0F),
                            CrossAlign(CrossAxisAlignment::Center)
                        ).Key("senv-" + ev.var + "-" + std::to_string(i))
                    );
                }
                tab_content = ScrollView(
                    Column(std::move(var_rows)).With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))
                ).ScrollAxis(Axis::Vertical)
                    .With(Grow(1.0F), ScrollBar{});
            }
        } else if (tab_idx == 2) {
            auto run_exec = [store, tasks, toast, name = cur.name, exec_cmd, exec_sandbox, exec_cwd, exec_timeout] {
                auto argv = detail::SplitCommandArgv(exec_cmd.Get().text);
                store->RunSubosExecWithOptions(
                    tasks,
                    toast,
                    name,
                    std::move(argv),
                    exec_sandbox.Get(),
                    exec_cwd.Get().text,
                    exec_timeout.Get().text
                );
            };
            tab_content = Column {
                Text("在 SubOS `" + cur.name + "` 中执行命令 (interface subos_exec)", TextRole::Label)
                    .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
                Row {
                    TextField(exec_cmd)
                        .Placeholder("例如: node --version 或 env")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([exec_cmd](const TextEditingValue& v) mutable { exec_cmd = v; })
                        .OnSubmitted(run_exec)
                        .With(Grow(1.0F)),
                    ActionButton(
                        running_exec ? "运行中..." : "运行",
                        ButtonTone::Primary,
                        run_exec,
                        !running_exec,
                        icons::PlayIcon(),
                        false
                    ),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Center)),
                Row {
                    TextField(exec_cwd)
                        .Placeholder("工作目录 --cwd（可选）")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([exec_cwd](const TextEditingValue& v) mutable { exec_cwd = v; })
                        .With(Grow(1.0F)),
                    TextField(exec_timeout)
                        .Placeholder("超时 --timeout 如 30s")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([exec_timeout](const TextEditingValue& v) mutable { exec_timeout = v; })
                        .With(Frame{.width = 140.0F}),
                }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                Checkbox("启用文件系统沙箱 (--sandbox)", exec_sandbox)
                    .OnChanged([exec_sandbox](bool v) mutable { exec_sandbox = v; }),
                CodeBox(exec_out, 150.0F, 420.0F).With(Grow(1.0F)),
            }.With(Grow(1.0F), Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch));
        } else if (tab_idx == 3) {
            tab_content = SubosPolicyTabView(store, tasks, toast, dialog, cur.name);
        } else if (tab_idx == 4) {
            tab_content = SubosSessionsTabView(store, tasks, toast, cur.name);
        } else {
            const std::array<std::string, 7> kinds{"", "ops", "lifecycle", "perm", "exec", "net", "fs"};
            const std::string cur_kind = event_kind.Get();
            std::vector<View> kind_chips;
            for (const auto& k : kinds) {
                kind_chips.push_back(
                    Chip(k.empty() ? std::string("全部") : k, cur_kind == k)
                        .OnChanged([store, tasks, event_kind, name = cur.name, k](bool) mutable {
                            event_kind = k;
                            store->LoadSubosDetail(tasks, name, k);
                        })
                        .Key("evk-" + (k.empty() ? std::string("all") : k))
                );
            }

            std::vector<View> ev_rows;
            for (std::size_t i = 0; i < events.size(); ++i) {
                const auto& ev = events[i];
                ev_rows.push_back(
                    Column {
                        Row {
                            ev.kind.empty() ? View{} : StatusBadge(ev.kind, sp.info, sp.info_bg),
                            Text(ev.timestamp.empty() ? ev.session : FormatLocalTimestamp(ev.timestamp), TextRole::Label)
                                .Style(TextStyle{.font = Font::Monospace(11.5F), .foreground = theme.colors.on_surface_variant}),
                        }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                        Text(ev.summary, TextRole::Body)
                            .Style(TextStyle{.font = Font::Monospace(12.0F), .foreground = theme.colors.on_surface}),
                    }.With(
                        Padding(10.0F),
                        Spacing(4.0F),
                        Background(sp.card_elevated_bg),
                        CornerRadius(8.0F),
                        CrossAlign(CrossAxisAlignment::Stretch)
                    ).Key("sev-" + std::to_string(i))
                );
            }

            tab_content = Column {
                Row {
                    Flow(std::move(kind_chips)).With(Spacing(6.0F)).With(Grow(1.0F)),
                    ActionButton(
                        "刷新事件",
                        ButtonTone::Secondary,
                        [store, tasks, name = cur.name, event_kind] {
                            store->LoadSubosDetail(tasks, name, event_kind.Get());
                        },
                        true,
                        icons::RefreshIcon(),
                        true
                    ),
                }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                ev_rows.empty()
                    ? EmptyState(icons::SubosIcon(), "暂无审计事件", "在沙箱或会话中运行命令后，审计记录将显示在此处")
                    : View(ScrollView(
                               Column(std::move(ev_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))
                           ).ScrollAxis(Axis::Vertical)
                               .With(Grow(1.0F), ScrollBar{})),
            }.With(Grow(1.0F), Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch));
        }

        right_pane = Column {
            Card(
                Column {
                    Row {
                        Text(cur.name, TextRole::Title)
                            .Style(TextStyle{
                                .font = Font::System(19.0F).WithWeight(FontWeight::Bold),
                                .foreground = theme.colors.on_surface,
                            })
                            .With(Grow(1.0F)),
                        loading_detail ? View(ProgressCircle().With(Frame{.width = 18.0F, .height = 18.0F})) : View{},
                        cur.active ? StatusBadge("当前活跃", sp.success, sp.success_bg) : View{},
                    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                    KeyValueRow("目录", cur.dir, false, true),
                    KeyValueRow("架构 / 视图", cur.abi + " · " + cur.kind + " (" + cur.view + ")"),
                    Flow {
                        ActionButton(
                            "启动会话",
                            ButtonTone::Secondary,
                            [store, tasks, toast, name = cur.name] { store->StartSubosSessionAsync(tasks, toast, name); },
                            true,
                            icons::PlayIcon(),
                            true
                        ),
                        ActionButton(
                            "停止会话",
                            ButtonTone::Secondary,
                            [store, tasks, toast, name = cur.name] { store->StopSubosSessionAsync(tasks, toast, name); },
                            true,
                            icons::TrashIcon(),
                            true
                        ),
                        ActionButton(
                            "升级",
                            ButtonTone::Secondary,
                            [dialog, store, tasks, toast, name = cur.name] {
                                dialog.Show([store, tasks, toast, name](DialogContext ctx) {
                                    return SubosUpgradeDialogContent(ctx, store, tasks, toast, name);
                                });
                            },
                            true,
                            icons::RefreshIcon(),
                            true
                        ),
                        ActionButton(
                            "回滚",
                            ButtonTone::Secondary,
                            [dialog, store, tasks, toast, name = cur.name] {
                                dialog.Show([store, tasks, toast, name](DialogContext ctx) {
                                    return SubosRollbackDialogContent(ctx, store, tasks, toast, name);
                                });
                            },
                            true,
                            icons::WrenchIcon(),
                            true
                        ),
                        ActionButton(
                            "运行时",
                            ButtonTone::Secondary,
                            [dialog, store, tasks, toast, name = cur.name] {
                                dialog.Show([store, tasks, toast, name](DialogContext ctx) {
                                    return SubosRuntimeDialogContent(ctx, store, tasks, toast, name);
                                });
                            },
                            true,
                            icons::ShieldIcon(),
                            true
                        ),
                        ActionButton(
                            "导出 / 打包",
                            ButtonTone::Secondary,
                            [dialog, store, tasks, toast, name = cur.name] {
                                dialog.Show([store, tasks, toast, name](DialogContext ctx) {
                                    return SubosExportPackDialogContent(ctx, store, tasks, toast, name);
                                });
                            },
                            true,
                            icons::DownloadIcon(),
                            true
                        ),
                    }.With(Spacing(6.0F)),
                }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                14.0F
            ),
            Tabs({"Shims", "环境", "执行", "策略", "会话", "审计"}, tab_idx)
                .OnChanged([detail_tab](std::size_t idx) mutable { detail_tab = idx; }),
            Card(std::move(tab_content), 16.0F).With(Grow(1.0F)),
        }.With(
            Grow(1.0F),
            Padding(16.0F),
            Spacing(12.0F),
            CrossAlign(CrossAxisAlignment::Stretch)
        );
    }

    return Column {
        std::move(toolbar),
        Divider(),
        ResizableSplit(std::move(left_pane), std::move(right_pane), 260.0F, 210.0F, 440.0F),
    }.With(Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch));
}

} // namespace hxlings

export module hxlings.page.subos;

import std;
import huxerui;
import xlings.types;
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
View NewSubosDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast
) {
    auto name_input = UseState(TextEditingValue::FromText(""));
    auto dir_input = UseState(TextEditingValue::FromText(""));
    auto err_msg = UseState(std::string{});

    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    auto submit = [ctx, store, tasks, toast, name_input, dir_input, err_msg]() mutable {
        const std::string name = name_input.Get().text;
        const std::string dir = dir_input.Get().text;
        if (!detail::IsValidSubosName(name)) {
            err_msg = std::string("名称仅支持字母、数字、_ 或 -，且不能为 'current'");
            return;
        }
        ctx.Dismiss();
        store->CreateSubosAsync(tasks, toast, name, dir);
    };

    View body = Column {
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
            .Label("自定义目录 (可选)")
            .Placeholder("留空默认创建在 $XLINGS_HOME/subos/<name>")
            .Variant(TextFieldVariant::Outlined)
            .OnChanged([dir_input](const TextEditingValue& v) mutable { dir_input = v; }),
        err_msg.Get().empty()
            ? View{}
            : Text(err_msg.Get(), TextRole::Label).Style(TextStyle{.foreground = sp.danger}),
    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch));

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton("创建 SubOS", ButtonTone::Primary, submit, true, icons::PlusIcon()),
    }.With(Spacing(10.0F));

    return DialogCardShell("新建 SubOS 子系统", std::move(body), std::move(actions), 460.0F);
}

[[huxerui::composable]]
View SubosPage() {
    const auto store = UseService<XlingsStore>();
    const TaskScope tasks = UseApplicationTaskScope();
    const ToastHandle toast = UseToast();
    const DialogHandle dialog = UseDialog();
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const auto clipboard = UseApplication().Clipboard();

    auto detail_tab = UseState<std::size_t>(0);
    auto shim_filter = UseState(TextEditingValue::FromText(""));
    auto exec_cmd = UseState(TextEditingValue::FromText("xlings --version"));
    auto exec_sandbox = UseState(false);
    auto event_kind = UseState(std::string{});

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
    }.With(
        Padding(EdgeInsets::Symmetric(20.0F, 12.0F)),
        Spacing(12.0F),
        Background(theme.colors.surface),
        CrossAlign(CrossAxisAlignment::Center)
    );

    std::vector<View> subos_cards;
    for (const auto& s : suboses) {
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
            .Key("subos-" + s.name)
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
                for (const auto& sh : shims) {
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
                                if (clipboard && clipboard->IsAvailable()) {
                                    clipboard->WriteText(sh);
                                    toast.Show("已复制命令: " + sh);
                                }
                            },
                            true,
                            std::nullopt,
                            true
                        ).Key("shim-" + sh)
                    );
                }

                tab_content = Column {
                    Row {
                        TextField(shim_filter)
                            .Placeholder("过滤 Shims 命令名...")
                            .Variant(TextFieldVariant::Outlined)
                            .LeadingIcon(icons::SearchIcon())
                            .OnChanged([shim_filter](const TextEditingValue& v) mutable { shim_filter = v; })
                            .With(Grow(1.0F)),
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
            auto run_exec = [store, tasks, toast, name = cur.name, exec_cmd, exec_sandbox] {
                auto argv = detail::SplitCommandArgv(exec_cmd.Get().text);
                store->RunSubosExec(tasks, toast, name, std::move(argv), exec_sandbox.Get());
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
                Checkbox("启用文件系统沙箱 (--sandbox)", exec_sandbox)
                    .OnChanged([exec_sandbox](bool v) mutable { exec_sandbox = v; }),
                CodeBox(exec_out, 180.0F, 420.0F).With(Grow(1.0F)),
            }.With(Grow(1.0F), Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch));
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
                            Text(ev.timestamp.empty() ? ev.session : ev.timestamp, TextRole::Label)
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
                }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                14.0F
            ),
            Tabs({"Shims", "环境变量", "执行命令", "审计事件"}, tab_idx)
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

export module hxlings.page.luban.env_tab;

import std;
import huxerui;
import hxlings.theme;
import hxlings.icons;
import hxlings.store;
import hxlings.components;
import hxlings.luban.types;
import hxlings.luban.stack;

import hxlings.page.luban.dialogs;

using namespace huxerui;


export namespace hxlings {

[[huxerui::composable]]
View LubanEnvironmentsTabView(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    DialogHandle dialog,
    ViewHandle filter_handle
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    const auto& host = store->luban_host_status.Get();
    const auto& envs = store->luban_envs.Get();
    const std::string selected_name = store->selected_luban_env.Get();
    const auto status_opt = store->selected_luban_status.Get();
    const auto config_opt = store->selected_luban_config.Get();
    const std::string& history_text = store->selected_luban_history.Get();
    const std::string& logs_text = store->selected_luban_logs.Get();
    const bool loading_detail = store->loading_luban_detail.Get();
    const std::string& run_output = store->luban_run_output.Get();
    const bool running_cmd = store->running_luban_cmd.Get();

    auto env_filter = UseState(TextEditingValue::FromText(""));
    auto detail_tab = UseState<std::size_t>(0);
    auto run_cmd_input = UseState(TextEditingValue::FromText("uname -a"));

    const LubanEnvConfigDetail base_cfg = config_opt.value_or(LubanEnvConfigDetail{});
    auto cfg_mode = UseState(base_cfg.mode);
    auto cfg_persist = UseState(base_cfg.persist);
    auto cfg_observe = UseState(base_cfg.observe_level);
    auto cfg_fetch = UseState(base_cfg.fetch_policy);
    auto cfg_policy = UseState(TextEditingValue::FromText(base_cfg.policy_extends));
    auto cfg_proxy = UseState(TextEditingValue::FromText(base_cfg.proxy));
    auto cfg_allow = UseState(TextEditingValue::FromText(""));
    auto cfg_mount = UseState(TextEditingValue::FromText(""));

    const std::string sync_fingerprint = selected_name + "|" + base_cfg.name + "|" + base_cfg.mode + "|" +
                                         base_cfg.persist + "|" + base_cfg.observe_level + "|" +
                                         base_cfg.fetch_policy + "|" + base_cfg.policy_extends + "|" + base_cfg.proxy;
    const std::size_t cfg_hash = std::hash<std::string>{}(sync_fingerprint);

    Lifecycle([base_cfg, cfg_mode, cfg_persist, cfg_observe, cfg_fetch, cfg_policy, cfg_proxy,
               cfg_allow, cfg_mount]() mutable {
        cfg_mode = base_cfg.mode;
        cfg_persist = base_cfg.persist;
        cfg_observe = base_cfg.observe_level;
        cfg_fetch = base_cfg.fetch_policy;
        cfg_policy = TextEditingValue::FromText(base_cfg.policy_extends);
        cfg_proxy = TextEditingValue::FromText(base_cfg.proxy);
        cfg_allow = TextEditingValue::FromText("");
        cfg_mount = TextEditingValue::FromText("");
    }, cfg_hash);

    const LubanEnvironment* selected_env = nullptr;
    for (const auto& e : envs) {
        if (e.name == selected_name) {
            selected_env = &e;
            break;
        }
    }

    std::vector<View> host_items;
    host_items.push_back(StatusBadge(
        host.backend_name.empty() ? std::string("后端待初始化") : ("后端 " + host.backend_name),
        host.backend_name.empty() ? sp.warning : sp.info,
        host.backend_name.empty() ? sp.warning_bg : sp.info_bg
    ));
    host_items.push_back(StatusBadge(
        host.sandbox_ready ? "Sandbox 就绪" : "Sandbox 未就绪",
        host.sandbox_ready ? sp.success : sp.danger,
        host.sandbox_ready ? sp.success_bg : sp.danger_bg
    ));
    host_items.push_back(StatusBadge(
        host.machine_ready ? "KVM 就绪" : "KVM 未就绪",
        host.machine_ready ? sp.success : sp.warning,
        host.machine_ready ? sp.success_bg : sp.warning_bg
    ));
    host_items.push_back(StatusBadge(
        host.desktop_ready ? "Desktop 就绪" : "Desktop 未安装",
        host.desktop_ready ? sp.success : theme.colors.on_surface_variant,
        host.desktop_ready ? sp.success_bg : sp.muted_bg
    ));
    if (!host.missing_tools.empty()) {
        const auto& missing = host.missing_tools.front();
        const std::string pkg_spec = missing.package.empty() ? ("xim:" + missing.name) : missing.package;
        host_items.push_back(
            ActionButton(
                "补齐 " + missing.name,
                ButtonTone::Secondary,
                [store, tasks, toast, pkg_spec] {
                    store->InstallLubanComponentAsync(tasks, toast, pkg_spec);
                },
                true,
                icons::DownloadIcon(),
                true
            ).Key("luban-missing-tool-btn")
        );
    }
    host_items.push_back(
        ActionButton(
            "新建环境",
            ButtonTone::Primary,
            [dialog, store, tasks, toast] {
                dialog.Show([store, tasks, toast](DialogContext ctx) {
                    return NewLubanEnvDialogContent(ctx, store, tasks, toast);
                });
            },
            true,
            icons::PlusIcon(),
            true
        )
    );
    host_items.push_back(
        ActionButton(
            "导入镜像",
            ButtonTone::Secondary,
            [dialog, store, tasks, toast] {
                dialog.Show([store, tasks, toast](DialogContext ctx) {
                    return LubanImportDialogContent(ctx, store, tasks, toast);
                });
            },
            true,
            icons::DownloadIcon(),
            true
        )
    );
    host_items.push_back(
        ActionButton(
            "初始化隔离",
            ButtonTone::Secondary,
            [store, tasks, toast] { store->SetupLubanHostAsync(tasks, toast); },
            true,
            icons::WrenchIcon(),
            true
        )
    );

    View host_strip = Row {
        Flow(std::move(host_items)).With(Spacing(6.0F)).With(Grow(1.0F)),
    }.With(
        Padding(EdgeInsets::Symmetric(16.0F, 8.0F)),
        Background(sp.card_elevated_bg),
        CrossAlign(CrossAxisAlignment::Center)
    );

    const std::string needle = luban_detail::ToLowerCopy(luban_detail::TrimLubanText(env_filter.Get().text));
    std::vector<View> env_cards;
    for (std::size_t i = 0; i < envs.size(); ++i) {
        const auto& env = envs[i];
        if (!needle.empty()) {
            const std::string hay = luban_detail::ToLowerCopy(env.name + " " + env.edition + " " + env.kind);
            if (hay.find(needle) == std::string::npos) continue;
        }
        const bool is_sel = (env.name == selected_name);

        std::vector<View> card_badges;
        if (!env.kind.empty()) {
            card_badges.push_back(StatusBadge(env.kind, sp.info, sp.info_bg));
        }
        if (!env.edition.empty()) {
            card_badges.push_back(StatusBadge(env.edition, theme.colors.on_surface_variant, sp.muted_bg));
        }
        card_badges.push_back(StatusBadge(
            std::to_string(env.packages) + " 包",
            theme.colors.on_surface_variant,
            sp.muted_bg
        ));
        for (const auto& dk : env.desktops) {
            if (!dk.empty()) {
                card_badges.push_back(StatusBadge("桌面:" + dk, sp.info, sp.info_bg));
            }
        }

        env_cards.push_back(
            Column {
                Row {
                    Text(env.name, TextRole::Title)
                        .Style(TextStyle{
                            .font = Font::System(14.5F).WithWeight(FontWeight::Bold),
                            .foreground = is_sel ? theme.colors.primary : theme.colors.on_surface,
                        })
                        .With(Grow(1.0F)),
                    StatusBadge(
                        env.running ? "运行中" : "已停止",
                        env.running ? sp.success : theme.colors.on_surface_variant,
                        env.running ? sp.success_bg : sp.muted_bg
                    ),
                }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                Flow(std::move(card_badges)).With(Spacing(5.0F)),
                env.shell.empty()
                    ? View{}
                    : Text("Shell: " + env.shell, TextRole::Label)
                          .Style(TextStyle{.font = Font::Monospace(11.5F), .foreground = theme.colors.on_surface_variant}),
            }.With(
                Padding(12.0F),
                Spacing(6.0F),
                Background(is_sel ? theme.colors.primary_container : sp.card_bg),
                Border{is_sel ? theme.colors.primary : sp.border_soft, is_sel ? 1.5F : 1.0F},
                CornerRadius(11.0F),
                Focusable(true),
                PointerCursor(PointerCursorKind::Hand),
                CrossAlign(CrossAxisAlignment::Stretch)
            )
            .OnClick([store, tasks, name = env.name] {
                store->LoadLubanDetail(tasks, name, false);
            })
            .Key("luban-env-" + env.name + "-" + std::to_string(i))
        );
    }

    View left_pane = Column {
        SearchBox(env_filter, {
            .placeholder = "筛选环境...",
            .focus_handle = filter_handle,
            .input_key = "luban-filter-input",
        }),
        env_cards.empty()
            ? EmptyState(
                  icons::LubanIcon(),
                  envs.empty() ? "暂无 Luban 环境" : "无匹配环境",
                  envs.empty() ? "可点击上方「新建环境」或切换到目录页从模板创建" : "尝试更换筛选关键词"
              )
            : View(ScrollView(
                       Column(std::move(env_cards)).With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))
                   ).ScrollAxis(Axis::Vertical)
                       .With(Grow(1.0F), ScrollBar{})),
    }.With(Grow(1.0F), Padding(12.0F), Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch));

    View right_pane = View{};
    if (selected_env == nullptr) {
        right_pane = EmptyState(icons::LubanIcon(), "选择一个 Luban 环境查看详情");
    } else {
        const LubanEnvironment cur = *selected_env;
        const std::size_t dtab = detail_tab.Get();
        const LubanEnvStatusDetail st_detail = status_opt.value_or(LubanEnvStatusDetail{});

        View tab_body = View{};
        if (dtab == 0) {
            const std::vector<std::pair<std::string, std::string>> edit_modes{
                {"sandbox", "sandbox"}, {"machine", "machine"}
            };
            const std::vector<std::pair<std::string, std::string>> edit_persists{
                {"all", "all"}, {"home", "home"}, {"none", "none"}
            };
            const std::vector<std::pair<std::string, std::string>> edit_obs{
                {"off", "off"}, {"basic", "basic"}, {"standard", "standard"}, {"full", "full"}
            };
            const std::vector<std::pair<std::string, std::string>> edit_fetch{
                {"auto", "auto"}, {"ask", "ask"}, {"deny", "deny"}
            };

            const std::string desktop_summary = !st_detail.desktop.empty()
                                                    ? st_detail.desktop
                                                    : luban_detail::JoinCsvItems(
                                                          st_detail.desktops.empty() ? cur.desktops : st_detail.desktops);

            tab_body = ScrollView(
                Column {
                    KeyValueRow("模式 / 持久化",
                                luban_detail::DisplayVal(st_detail.mode.empty() ? base_cfg.mode : st_detail.mode) + " · " +
                                    luban_detail::DisplayVal(st_detail.persist.empty() ? base_cfg.persist : st_detail.persist)),
                    KeyValueRow("入口 / 网络",
                                luban_detail::DisplayVal(st_detail.enter) + " · " +
                                    luban_detail::DisplayVal(st_detail.network.empty() ? base_cfg.net : st_detail.network)),
                    KeyValueRow("Shell / 桌面",
                                luban_detail::DisplayVal(st_detail.shell.empty() ? cur.shell : st_detail.shell) + " · " +
                                    luban_detail::DisplayVal(desktop_summary)),
                    KeyValueRow("载体 / 挂载",
                                luban_detail::DisplayVal(st_detail.carrier.empty() ? cur.carrier : st_detail.carrier) +
                                    (base_cfg.mounts.empty() ? "" : (" · " + luban_detail::JoinCsvItems(base_cfg.mounts))),
                                false,
                                true),
                    st_detail.notes.empty()
                        ? View{}
                        : KeyValueRow("环境说明", luban_detail::JoinCsvItems(st_detail.notes)),
                    st_detail.unmet.empty()
                        ? View{}
                        : KeyValueRow("未满足依赖", luban_detail::JoinCsvItems(st_detail.unmet), true),
                    Divider(),
                    Text("在线配置修改 (`luban config " + cur.name + "`)", TextRole::Label)
                        .Style(TextStyle{.font = Font::System(13.0F).WithWeight(FontWeight::SemiBold)}),
                    Text("运行模式 (--mode)", TextRole::Label),
                    LubanOptionChips(
                        edit_modes, cfg_mode.Get(), [cfg_mode](std::string v) mutable { cfg_mode = std::move(v); }, "cfg-mode"),
                    Text("持久化 (--persist)", TextRole::Label),
                    LubanOptionChips(
                        edit_persists,
                        cfg_persist.Get(),
                        [cfg_persist](std::string v) mutable { cfg_persist = std::move(v); },
                        "cfg-persist"
                    ),
                    Text("审计观测 (--observe)", TextRole::Label),
                    LubanOptionChips(
                        edit_obs,
                        cfg_observe.Get(),
                        [cfg_observe](std::string v) mutable { cfg_observe = std::move(v); },
                        "cfg-obs"
                    ),
                    Text("包拉取策略 (--fetch)", TextRole::Label),
                    LubanOptionChips(
                        edit_fetch,
                        cfg_fetch.Get(),
                        [cfg_fetch](std::string v) mutable { cfg_fetch = std::move(v); },
                        "cfg-fetch"
                    ),
                    TextField(cfg_policy)
                        .Label("隔离策略 (--policy)")
                        .Placeholder("例如: dev / strict / locked")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([cfg_policy](const TextEditingValue& v) mutable { cfg_policy = v; }),
                    TextField(cfg_proxy)
                        .Label("网络代理 (--proxy)")
                        .Placeholder("例如: socks5h://127.0.0.1:1080")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([cfg_proxy](const TextEditingValue& v) mutable { cfg_proxy = v; }),
                    TextField(cfg_allow)
                        .Label("新增授权能力 (--allow，逗号分隔)")
                        .Placeholder("例如: display, gpu")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([cfg_allow](const TextEditingValue& v) mutable { cfg_allow = v; }),
                    TextField(cfg_mount)
                        .Label("追加宿主挂载 (--mount，逗号分隔)")
                        .Placeholder("例如: /path/src:/workspace:rw")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([cfg_mount](const TextEditingValue& v) mutable { cfg_mount = v; }),
                    Row {
                        ActionButton(
                            "保存配置",
                            ButtonTone::Primary,
                            [store, tasks, toast, cur_name = cur.name, base_cfg, cfg_mode, cfg_persist, cfg_observe,
                             cfg_fetch, cfg_policy, cfg_proxy, cfg_allow, cfg_mount] {
                                LubanConfigPatch patch;
                                if (!cfg_mode.Get().empty() && cfg_mode.Get() != base_cfg.mode) {
                                    patch.mode = cfg_mode.Get();
                                }
                                if (!cfg_persist.Get().empty() && cfg_persist.Get() != base_cfg.persist) {
                                    patch.persist = cfg_persist.Get();
                                }
                                if (!cfg_observe.Get().empty() && cfg_observe.Get() != base_cfg.observe_level) {
                                    patch.observe = cfg_observe.Get();
                                }
                                if (!cfg_fetch.Get().empty() && cfg_fetch.Get() != base_cfg.fetch_policy) {
                                    patch.fetch = cfg_fetch.Get();
                                }
                                const std::string pol = luban_detail::TrimLubanText(cfg_policy.Get().text);
                                if (!pol.empty() && pol != base_cfg.policy_extends) {
                                    patch.policy = pol;
                                }
                                const std::string prx = luban_detail::TrimLubanText(cfg_proxy.Get().text);
                                if (prx != base_cfg.proxy && !prx.empty()) {
                                    patch.proxy = prx;
                                }
                                patch.allow = luban_detail::SplitCsvItems(cfg_allow.Get().text);
                                patch.mounts = luban_detail::SplitCsvItems(cfg_mount.Get().text);
                                store->ConfigureLubanEnvAsync(tasks, toast, cur_name, std::move(patch));
                            },
                            true,
                            icons::CheckIcon(),
                            true
                        ),
                        Spacer{}.With(Grow(1.0F)),
                    },
                }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ).ScrollAxis(Axis::Vertical)
                .With(Grow(1.0F), ScrollBar{});
        } else if (dtab == 1) {
            auto run_submit = [store, tasks, toast, cur_name = cur.name, run_cmd_input]() {
                store->RunLubanCommandInlineAsync(
                    tasks, toast, cur_name, luban_detail::SplitLubanCommandArgv(run_cmd_input.Get().text));
            };

            tab_body = ScrollView(
                Column {
                    SectionHeader(
                        "环境内指令运行 (`luban run " + cur.name + "`)",
                        "向隔离环境传递命令并即时收集标准输出"
                    ),
                    Row {
                        TextField(run_cmd_input)
                            .Label("命令行")
                            .Placeholder("例如: xlings --version 或 bash -lc 'uname -a'")
                            .Variant(TextFieldVariant::Outlined)
                            .OnChanged([run_cmd_input](const TextEditingValue& v) mutable { run_cmd_input = v; })
                            .OnSubmitted(run_submit)
                            .With(Grow(1.0F)),
                        ActionButton(
                            "运行命令",
                            ButtonTone::Primary,
                            run_submit,
                            true,
                            icons::PlayIcon(),
                            false,
                            running_cmd
                        ).Key("luban-run-btn"),
                    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Flow {
                        ActionButton(
                            "uname -a",
                            ButtonTone::Secondary,
                            [run_cmd_input]() mutable { run_cmd_input = TextEditingValue::FromText("uname -a"); },
                            true,
                            std::nullopt,
                            true
                        ),
                        ActionButton(
                            "xlings --version",
                            ButtonTone::Secondary,
                            [run_cmd_input]() mutable {
                                run_cmd_input = TextEditingValue::FromText("xlings --version");
                            },
                            true,
                            std::nullopt,
                            true
                        ),
                        ActionButton(
                            "env",
                            ButtonTone::Secondary,
                            [run_cmd_input]() mutable { run_cmd_input = TextEditingValue::FromText("env"); },
                            true,
                            std::nullopt,
                            true
                        ),
                    }.With(Spacing(6.0F)),
                    CodeBox(run_output, 120.0F, 220.0F),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ).ScrollAxis(Axis::Vertical)
                .With(Grow(1.0F), ScrollBar{});
        } else {
            tab_body = ScrollView(
                Column {
                    SectionHeader(
                        "环境代历史 (`luban history " + cur.name + "`)",
                        "记录发行版升级、快照和回滚代数",
                        RefreshButton(
                            "刷新详情",
                            [store, tasks, cur_name = cur.name] {
                                store->LoadLubanDetail(tasks, cur_name, true);
                            },
                            loading_detail
                        ).Key("luban-refresh-detail-btn")
                    ),
                    CodeBox(history_text, 76.0F, 130.0F),
                    SectionHeader(
                        "聚合日志 (`luban logs " + cur.name + "`)",
                        "隔离事件与最近运行日志"
                    ),
                    CodeBox(logs_text, 96.0F, 190.0F),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ).ScrollAxis(Axis::Vertical)
                .With(Grow(1.0F), ScrollBar{});
        }

        right_pane = Column {
            Card(
                Column {
                    Row {
                        Text(cur.name, TextRole::Title)
                            .Style(TextStyle{
                                .font = Font::System(18.0F).WithWeight(FontWeight::Bold),
                                .foreground = theme.colors.on_surface,
                            }),
                        StatusBadge(
                            cur.running ? "运行中" : "已停止",
                            cur.running ? sp.success : theme.colors.on_surface_variant,
                            cur.running ? sp.success_bg : sp.muted_bg
                        ),
                        cur.kind.empty() ? View{} : StatusBadge(cur.kind, sp.info, sp.info_bg),
                        Spacer{}.With(Grow(1.0F)),
                        LoadingIndicatorSlot(loading_detail, 16.0F),
                    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Flow {
                        ActionButton(
                            "停止",
                            ButtonTone::Secondary,
                            [store, tasks, toast, name = cur.name] { store->StopLubanEnvAsync(tasks, toast, name); },
                            true,
                            icons::TrashIcon(),
                            true
                        ),
                        ActionButton(
                            "升级",
                            ButtonTone::Secondary,
                            [dialog, store, tasks, toast, name = cur.name] {
                                dialog.Show([store, tasks, toast, name](DialogContext ctx) {
                                    return LubanUpgradeDialogContent(ctx, store, tasks, toast, name);
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
                                    return LubanRollbackDialogContent(ctx, store, tasks, toast, name);
                                });
                            },
                            true,
                            icons::WrenchIcon(),
                            true
                        ),
                        ActionButton(
                            "导出镜像",
                            ButtonTone::Secondary,
                            [dialog, store, tasks, toast, name = cur.name] {
                                dialog.Show([store, tasks, toast, name](DialogContext ctx) {
                                    return LubanExportDialogContent(ctx, store, tasks, toast, name);
                                });
                            },
                            true,
                            icons::DownloadIcon(),
                            true
                        ),
                        ActionButton(
                            "删除",
                            ButtonTone::Danger,
                            [dialog, store, tasks, toast, name = cur.name] {
                                dialog.Show(
                                    "删除 Luban 环境",
                                    "确定要删除环境 `" + name + "` 及其持久化数据吗？此操作不可撤销。",
                                    "删除",
                                    "取消",
                                    [store, tasks, toast, name] { store->RemoveLubanEnvAsync(tasks, toast, name); },
                                    {}
                                );
                            },
                            true,
                            icons::TrashIcon(),
                            true
                        ),
                    }.With(Spacing(6.0F)),
                }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                12.0F
            ),
            Tabs({"概览与配置", "执行命令", "历史与日志"}, dtab)
                .OnChanged([detail_tab](std::size_t idx) mutable { detail_tab = idx; }),
            Card(std::move(tab_body), 14.0F).With(Grow(1.0F)),
        }.With(
            Grow(1.0F),
            Padding(14.0F),
            Spacing(10.0F),
            CrossAlign(CrossAxisAlignment::Stretch)
        );
    }

    return Column {
        std::move(host_strip),
        Divider(),
        ResizableSplit(std::move(left_pane), std::move(right_pane), 260.0F, 210.0F, 420.0F),
    }.With(Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch));
}

} // namespace hxlings
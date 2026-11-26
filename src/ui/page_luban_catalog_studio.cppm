export module hxlings.page.luban.studio;

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
View LubanCatalogTabView(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    DialogHandle dialog
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const auto& catalog = store->luban_catalog.Get();

    auto build_cards = [&](const std::vector<LubanCatalogItem>& items, bool is_desktop, std::string key_prefix) {
        std::vector<View> cards;
        for (std::size_t i = 0; i < items.size(); ++i) {
            const auto& item = items[i];
            std::vector<View> badges;
            if (!item.version.empty()) {
                badges.push_back(StatusBadge("v" + item.version, sp.info, sp.info_bg));
            }
            if (!item.status.empty()) {
                const bool stable = (item.status == "stable" || item.status == "ready" || item.status == "available");
                badges.push_back(StatusBadge(
                    item.status,
                    stable ? sp.success : sp.warning,
                    stable ? sp.success_bg : sp.warning_bg
                ));
            }
            if (item.tier > 0) {
                badges.push_back(StatusBadge("Tier " + std::to_string(item.tier), theme.colors.on_surface_variant, sp.muted_bg));
            }
            if (!item.gpu_hint.empty()) {
                badges.push_back(StatusBadge("GPU: " + item.gpu_hint, theme.colors.on_surface_variant, sp.muted_bg));
            }
            for (const auto& hl : item.highlights) {
                if (!hl.empty()) {
                    badges.push_back(StatusBadge(hl, theme.colors.on_surface_variant, sp.muted_bg));
                }
            }

            const std::string title = item.title_zh.empty() ? item.name : (item.title_zh + " (" + item.name + ")");
            const std::string desc = !item.summary_zh.empty() ? item.summary_zh : item.description;

            cards.push_back(
                Column {
                    Row {
                        Column {
                            Text(title, TextRole::Title)
                                .Style(TextStyle{
                                    .font = Font::System(15.0F).WithWeight(FontWeight::Bold),
                                    .foreground = theme.colors.on_surface,
                                }),
                            item.ref.empty()
                                ? View{}
                                : Text(item.ref, TextRole::Label)
                                      .Style(TextStyle{.font = Font::Monospace(11.5F), .foreground = theme.colors.on_surface_variant}),
                        }.With(Spacing(3.0F), Grow(1.0F)),
                        ActionButton(
                            is_desktop ? "创建桌面环境" : "以此创建环境",
                            ButtonTone::Primary,
                            [dialog, store, tasks, toast, is_desktop, name = item.name] {
                                dialog.Show([store, tasks, toast, is_desktop, name](DialogContext ctx) {
                                    return NewLubanEnvDialogContent(
                                        ctx,
                                        store,
                                        tasks,
                                        toast,
                                        is_desktop ? std::string("desktop") : name,
                                        is_desktop ? name : std::string{}
                                    );
                                });
                            },
                            item.available,
                            icons::PlusIcon(),
                            true
                        ),
                    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Flow(std::move(badges)).With(Spacing(6.0F)),
                    desc.empty()
                        ? View{}
                        : Text(desc, TextRole::Body)
                              .Style(TextStyle{.font = Font::System(13.0F), .foreground = theme.colors.on_surface_variant}),
                }.With(
                    Padding(12.0F),
                    Spacing(8.0F),
                    Background(theme.colors.surface_container_low),
                    Border{sp.border_soft, 1.0F},
                    CornerRadius(10.0F),
                    CrossAlign(CrossAxisAlignment::Stretch)
                ).Key(key_prefix + item.name + "-" + std::to_string(i))
            );
        }
        return cards;
    };

    auto dist_cards = build_cards(catalog.distributions, false, "luban-dist-");
    auto desk_cards = build_cards(catalog.desktops, true, "luban-desk-");

    return ScrollView(
        Column {
            Card(
                Column {
                    SectionHeader(
                        "官方发行版 (Distributions · " + std::to_string(catalog.distributions.size()) + ")",
                        "来自 `luban search --json`，支持一键预填生成定制隔离环境"
                    ),
                    dist_cards.empty()
                        ? EmptyState(icons::LubanIcon(), "未获取到发行版目录", "点击右上角「刷新 Luban」同步最新官方目录")
                        : View(Column(std::move(dist_cards)).With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader(
                        "官方桌面环境 (Desktops · " + std::to_string(catalog.desktops.size()) + ")",
                        "配合 `xim:luban-os` 提供隔离 Linux 桌面会话"
                    ),
                    desk_cards.empty()
                        ? EmptyState(icons::StoreIcon(), "未获取到桌面目录", "请确认当前架构与 Luban 索引支持情况")
                        : View(Column(std::move(desk_cards)).With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
        }.With(Padding(18.0F), Spacing(14.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Grow(1.0F), ScrollBar{})
        .Key("luban-stack-scroll");
}

[[huxerui::composable]]
View LubanStacksStudioTabView(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    DialogHandle dialog
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const auto clipboard = UseApplication().Clipboard();

    const auto presets = BuiltinLubanStacks();
    const std::string initial_text = presets.empty() ? std::string{} : SerializeLubanStackToml(presets.front());

    auto format_state = UseState(std::string("toml"));
    auto draft_input = UseState(TextEditingValue::FromText(initial_text));

    Lifecycle([store, tasks] {
        if (!store->skip_initial_refresh) {
            store->RefreshSavedLubanStacks(tasks);
        }
    });

    const std::string fmt = format_state.Get();
    const std::string raw_text = draft_input.Get().text;
    const LubanStackParseResult parsed = ParseLubanStackAuto(raw_text);
    const auto& envs = store->luban_envs.Get();
    const auto& cfg_cache = store->luban_config_cache.Get();
    const auto& saved_stacks = store->saved_luban_stacks.Get();
    const bool luban_avail = store->luban_available.Get();
    const bool task_busy = store->task_running.Get();

    const LubanStackPlan plan = parsed.ok
                                    ? BuildLubanStackPlan(parsed.spec, envs, cfg_cache)
                                    : LubanStackPlan{};

    std::vector<View> preset_buttons;
    preset_buttons.reserve(presets.size());
    for (std::size_t i = 0; i < presets.size(); ++i) {
        const auto& st = presets[i];
        preset_buttons.push_back(
            ActionButton(
                st.name + " (" + std::to_string(st.environments.size()) + ")",
                ButtonTone::Secondary,
                [st, format_state, draft_input, toast]() mutable {
                    const std::string next = (format_state.Get() == "json")
                                                 ? SerializeLubanStackJson(st)
                                                 : SerializeLubanStackToml(st);
                    draft_input = TextEditingValue::FromText(next);
                    toast.Show("已载入模板: " + st.name);
                },
                true,
                std::nullopt,
                true
            ).Key("luban-stack-preset-" + std::to_string(i))
        );
    }

    std::vector<View> saved_rows;
    saved_rows.reserve(saved_stacks.size());
    for (std::size_t i = 0; i < saved_stacks.size(); ++i) {
        const auto& info = saved_stacks[i];
        std::string fmt_upper = luban_detail::ToLowerCopy(info.format) == "toml" ? "TOML" : "JSON";
        saved_rows.push_back(
            Row {
                Column {
                    Flow {
                        StatusBadge(fmt_upper, sp.info, sp.info_bg),
                        Text(info.display_name, TextRole::Body)
                            .Style(TextStyle{
                                .font = Font::System(13.5F).WithWeight(FontWeight::SemiBold),
                                .foreground = theme.colors.on_surface,
                            }),
                        StatusBadge(
                            std::to_string(info.env_count) + " 环境",
                            theme.colors.on_surface_variant,
                            sp.muted_bg
                        ),
                    }.With(Spacing(6.0F)),
                    info.description.empty()
                        ? View{}
                        : Text(info.description, TextRole::Label)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
                }.With(Spacing(4.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                Flow {
                    ActionButton(
                        "加载",
                        ButtonTone::Secondary,
                        [info, format_state, draft_input, toast]() mutable {
                            const auto content = LoadLubanStackFile(info.path);
                            if (!content.has_value()) {
                                toast.Show("读取蓝图文件失败: " + info.filename);
                                return;
                            }
                            format_state = (info.format == "json") ? "json" : "toml";
                            draft_input = TextEditingValue::FromText(*content);
                            toast.Show("已加载本地蓝图: " + info.display_name);
                        },
                        true,
                        std::nullopt,
                        true
                    ).Key("luban-saved-load-" + std::to_string(i)),
                    ActionButton(
                        "删除",
                        ButtonTone::Danger,
                        [dialog, store, tasks, toast, info] {
                            dialog.Show(
                                "删除本地蓝图",
                                "确定删除本地蓝图 `" + info.filename + "` 吗？",
                                "删除",
                                "取消",
                                [store, tasks, toast, info] {
                                    store->DeleteSavedLubanStack(tasks, toast, info.path, info.display_name);
                                },
                                {}
                            );
                        },
                        true,
                        icons::TrashIcon(),
                        true
                    ).Key("luban-saved-del-" + std::to_string(i)),
                }.With(Spacing(6.0F)),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                Spacing(8.0F),
                CrossAlign(CrossAxisAlignment::Center),
                Background(theme.colors.surface_container_low),
                CornerRadius(8.0F)
            ).Key("luban-saved-stack-" + info.id + "-" + std::to_string(i))
        );
    }

    std::vector<View> err_lines;
    if (!parsed.ok) {
        err_lines.push_back(
            Text("清单校验未通过:", TextRole::Label)
                .Style(TextStyle{
                    .font = Font::System(12.5F).WithWeight(FontWeight::SemiBold),
                    .foreground = sp.danger,
                })
        );
        for (std::size_t i = 0; i < parsed.errors.size(); ++i) {
            err_lines.push_back(
                Text("• " + parsed.errors[i], TextRole::Label)
                    .Style(TextStyle{.foreground = sp.danger})
                    .Key("luban-stack-err-" + std::to_string(i))
            );
        }
    }

    View validation_box = parsed.ok
                              ? View(Row {
                                    StatusBadge(
                                        "语法有效 (" + parsed.detected_format + ") · " +
                                            std::to_string(parsed.spec.environments.size()) + " 个环境",
                                        sp.success,
                                        sp.success_bg
                                    ).Key("luban-stack-valid-badge"),
                                    Spacer{}.With(Grow(1.0F)),
                                })
                              : View(Column(std::move(err_lines))
                                    .With(
                                        Padding(10.0F),
                                        Spacing(4.0F),
                                        Background(sp.danger_bg),
                                        Border{WithAlpha(sp.danger, 0.35F), 1.0F},
                                        CornerRadius(8.0F),
                                        CrossAlign(CrossAxisAlignment::Stretch)
                                    )
                                    .Key("luban-stack-errors"));

    std::vector<View> plan_rows;
    if (parsed.ok) {
        plan_rows.reserve(plan.actions.size());
        for (std::size_t i = 0; i < plan.actions.size(); ++i) {
            const auto& act = plan.actions[i];
            std::string badge_txt = "+ 新建";
            Color fg = sp.success;
            Color bg = sp.success_bg;
            if (act.kind == LubanStackActionKind::Configure) {
                badge_txt = "~ 更新配置";
                fg = sp.warning;
                bg = sp.warning_bg;
            } else if (act.kind == LubanStackActionKind::UpToDate) {
                badge_txt = "= 已就绪";
                fg = sp.info;
                bg = sp.info_bg;
            }
            plan_rows.push_back(
                Column {
                    Flow {
                        StatusBadge(badge_txt, fg, bg),
                        Text(act.env_name, TextRole::Body)
                            .Style(TextStyle{
                                .font = Font::Monospace(13.0F).WithWeight(FontWeight::SemiBold),
                                .foreground = theme.colors.on_surface,
                            }),
                    }.With(Spacing(8.0F)),
                    Text(act.summary, TextRole::Label)
                        .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
                }.With(
                    Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                    Spacing(4.0F),
                    Background(theme.colors.surface_container_low),
                    CornerRadius(8.0F),
                    CrossAlign(CrossAxisAlignment::Stretch)
                ).Key("luban-plan-act-" + act.env_name + "-" + std::to_string(i))
            );
        }
    }

    return ScrollView(
        Column {
            Card(
                Column {
                    SectionHeader(
                        "蓝图模板与本地库",
                        "本地蓝图目录: ~/.config/hxlings/luban-stacks · 支持 TOML 与 JSON 双向切换",
                        ActionButton(
                            "重载本地库",
                            ButtonTone::Secondary,
                            [store, tasks] { store->RefreshSavedLubanStacks(tasks); },
                            !store->skip_initial_refresh,
                            icons::RefreshIcon(),
                            true
                        )
                    ),
                    Text("内置精选模板", TextRole::Label),
                    Flow(std::move(preset_buttons)).With(Spacing(8.0F)),
                    Divider(),
                    Text(
                        "已保存本地蓝图 (" + std::to_string(saved_stacks.size()) + ")",
                        TextRole::Label
                    ),
                    saved_rows.empty()
                        ? Text("暂无本地保存蓝图，可在下方编辑有效清单后点击「保存到本地」", TextRole::Label)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        : View(Column(std::move(saved_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader(
                        "声明式清单编辑器 (TOML / JSON)",
                        "支持注释 TOML 与标准 JSON 格式实时解析与无损互转 (`hxlings.luban-stack/v1`)"
                    ),
                    Flow {
                        ActionButton(
                            "TOML",
                            fmt == "toml" ? ButtonTone::Primary : ButtonTone::Secondary,
                            [format_state, draft_input, parsed, toast]() mutable {
                                format_state = std::string("toml");
                                if (parsed.ok) {
                                    draft_input = TextEditingValue::FromText(SerializeLubanStackToml(parsed.spec));
                                } else {
                                    toast.Show("已切换为 TOML 模式；当前清单存在语法错误，请修正后转换");
                                }
                            },
                            true,
                            std::nullopt,
                            true
                        ).Key("luban-stack-fmt-toml"),
                        ActionButton(
                            "JSON",
                            fmt == "json" ? ButtonTone::Primary : ButtonTone::Secondary,
                            [format_state, draft_input, parsed, toast]() mutable {
                                format_state = std::string("json");
                                if (parsed.ok) {
                                    draft_input = TextEditingValue::FromText(SerializeLubanStackJson(parsed.spec));
                                } else {
                                    toast.Show("已切换为 JSON 模式；当前清单存在语法错误，请修正后转换");
                                }
                            },
                            true,
                            std::nullopt,
                            true
                        ).Key("luban-stack-fmt-json"),
                        ActionButton(
                            "从当前环境导出",
                            ButtonTone::Secondary,
                            [store, format_state, draft_input, toast]() mutable {
                                const auto exported = ExportEnvironmentsToStack(
                                    "local-snapshot",
                                    store->luban_envs.Get(),
                                    store->luban_config_cache.Get()
                                );
                                if (exported.environments.empty()) {
                                    toast.Show("当前没有可导出的非默认环境");
                                    return;
                                }
                                const std::string serialized = (format_state.Get() == "json")
                                                                   ? SerializeLubanStackJson(exported)
                                                                   : SerializeLubanStackToml(exported);
                                draft_input = TextEditingValue::FromText(serialized);
                                toast.Show("已从 " + std::to_string(exported.environments.size()) + " 个当前环境导出蓝图");
                            },
                            true,
                            icons::CopyIcon(),
                            true
                        ).Key("luban-stack-export-envs-btn"),
                        ActionButton(
                            "格式化",
                            ButtonTone::Secondary,
                            [fmt, draft_input, parsed]() mutable {
                                if (!parsed.ok) return;
                                draft_input = TextEditingValue::FromText(
                                    fmt == "json" ? SerializeLubanStackJson(parsed.spec)
                                                  : SerializeLubanStackToml(parsed.spec)
                                );
                            },
                            parsed.ok,
                            icons::FileCodeIcon(),
                            true
                        ).Key("luban-stack-format-btn"),
                        ActionButton(
                            "保存到本地",
                            ButtonTone::Secondary,
                            [store, tasks, toast, parsed, raw_text, fmt] {
                                if (!parsed.ok) return;
                                (void)store->SaveLubanStackDraft(tasks, toast, parsed.spec.name, raw_text, fmt);
                            },
                            parsed.ok,
                            icons::CheckIcon(),
                            true
                        ).Key("luban-stack-save-btn"),
                        ActionButton(
                            "复制清单",
                            ButtonTone::Ghost,
                            [clipboard, toast, raw_text] {
                                const bool ok = clipboard && clipboard->SupportsSynchronousText() &&
                                                clipboard->WriteText(raw_text);
                                toast.Show(ok ? "已复制蓝图清单" : "复制到剪贴板失败");
                            },
                            !raw_text.empty(),
                            icons::CopyIcon(),
                            true
                        ),
                    }.With(Spacing(8.0F)),
                    TextField(draft_input)
                        .Label(std::string("声明式编排清单 (") + (fmt == "toml" ? "TOML" : "JSON") + ")")
                        .Placeholder("输入 hxlings.luban-stack/v1 清单...")
                        .Variant(TextFieldVariant::Outlined)
                        .LineLimits(TextFieldLineLimits::MultiLine(8, 14))
                        .OnChanged([draft_input](const TextEditingValue& v) mutable { draft_input = v; })
                        .Key("luban-stack-editor"),
                    std::move(validation_box),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader(
                        "实时差异计划 (Plan Diff) 与执行",
                        parsed.ok ? ("栈名称: " + parsed.spec.name + " · 目标环境 " +
                                         std::to_string(parsed.spec.environments.size()) + " 个")
                                  : std::string("修正清单语法后将自动对比本机现有环境并生成执行计划")
                    ),
                    parsed.ok
                        ? Flow {
                              StatusBadge("+ 新建 " + std::to_string(plan.create_count), sp.success, sp.success_bg),
                              StatusBadge("~ 更新配置 " + std::to_string(plan.configure_count), sp.warning, sp.warning_bg),
                              StatusBadge("= 已就绪 " + std::to_string(plan.unchanged_count), sp.info, sp.info_bg),
                          }.With(Spacing(8.0F))
                        : View{},
                    plan_rows.empty()
                        ? View{}
                        : View(Column(std::move(plan_rows)).With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                    Flow {
                        ActionButton(
                            "一键应用编排 (Apply)",
                            ButtonTone::Primary,
                            [dialog, store, tasks, toast, parsed, plan] {
                                if (!parsed.ok) return;
                                const std::string detail =
                                    "栈 `" + parsed.spec.name + "` 将执行: 新建 " +
                                    std::to_string(plan.create_count) + " 个 / 更新配置 " +
                                    std::to_string(plan.configure_count) + " 个 / 跳过已就绪 " +
                                    std::to_string(plan.unchanged_count) + " 个。";
                                dialog.Show(
                                    "应用多环境蓝图",
                                    detail,
                                    "开始执行",
                                    "取消",
                                    [store, tasks, toast, spec = parsed.spec] {
                                        store->ApplyLubanStackAsync(tasks, toast, spec, false);
                                    },
                                    {}
                                );
                            },
                            parsed.ok && luban_avail && !task_busy,
                            icons::PlayIcon(),
                            false
                        ).Key("luban-stack-apply-btn"),
                        ActionButton(
                            "仅预演计划 (Dry-Run)",
                            ButtonTone::Secondary,
                            [store, tasks, toast, parsed] {
                                if (!parsed.ok) return;
                                store->ApplyLubanStackAsync(tasks, toast, parsed.spec, true);
                            },
                            parsed.ok && luban_avail && !task_busy,
                            icons::CheckIcon(),
                            true
                        ),
                        ActionButton(
                            "同步实时配置",
                            ButtonTone::Secondary,
                            [store, tasks, toast, parsed, envs] {
                                if (!parsed.ok) return;
                                std::vector<std::string> targets;
                                for (const auto& item : parsed.spec.environments) {
                                    const bool exists = std::ranges::any_of(
                                        envs, [&](const LubanEnvironment& e) { return e.name == item.name; });
                                    if (exists) targets.push_back(item.name);
                                }
                                if (targets.empty()) {
                                    toast.Show("本机尚未存在本蓝图中的任何环境");
                                    return;
                                }
                                store->SyncLubanConfigsForStackAsync(tasks, toast, std::move(targets));
                            },
                            parsed.ok && luban_avail,
                            icons::RefreshIcon(),
                            true
                        ),
                        ActionButton(
                            "停止栈内环境",
                            ButtonTone::Danger,
                            [dialog, store, tasks, toast, parsed] {
                                if (!parsed.ok) return;
                                dialog.Show(
                                    "停止蓝图内所有运行环境",
                                    "将依次停止 `" + parsed.spec.name + "` 中正在运行的 Luban 环境。",
                                    "全部停止",
                                    "取消",
                                    [store, tasks, toast, spec = parsed.spec] {
                                        store->StopLubanStackAsync(tasks, toast, spec);
                                    },
                                    {}
                                );
                            },
                            parsed.ok && luban_avail && !task_busy,
                            icons::TrashIcon(),
                            true
                        ),
                    }.With(Spacing(8.0F)),
                    CodeBox(
                        "# 容器互操作：导出 Luban 环境为 OCI rootfs tar 并导入 Podman / Docker\n"
                        "luban export <env> ./<env>.tar --format tar && podman import ./<env>.tar luban-<env>:latest",
                        56.0F,
                        84.0F
                    ),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
        }.With(Padding(18.0F), Spacing(14.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Grow(1.0F), ScrollBar{})
        .Key("luban-stack-scroll");
}

} // namespace hxlings
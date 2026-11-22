export module hxlings.page.packages;

import std;
import huxerui;
import xlings.types;
import hxlings.theme;
import hxlings.icons;
import hxlings.store;
import hxlings.components;

using namespace huxerui;

namespace hxlings::detail {

std::string ToLowerCopy(std::string_view sv) {
    std::string out(sv);
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

std::string MakeTargetSpec(std::string_view ns, std::string_view name, std::string_view version) {
    std::string res;
    if (!ns.empty()) {
        res.append(ns);
        res.push_back(':');
    }
    res.append(name);
    if (!version.empty()) {
        res.push_back('@');
        res.append(version);
    }
    return res;
}

} // namespace hxlings::detail

export namespace hxlings {

[[huxerui::composable]]
View PackagesPage() {
    const auto store = UseService<XlingsStore>();
    const TaskScope tasks = UseApplicationTaskScope();
    const ToastHandle toast = UseToast();
    const DialogHandle dialog = UseDialog();
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    auto filter_input = UseState(TextEditingValue::FromText(""));
    auto filter_mode = UseState<std::size_t>(0); // 0=all, 1=multi-version, 2=degraded

    const auto& all_groups = store->installed_groups.Get();
    const std::string selected_key = store->selected_installed_key.Get();
    const auto& detail_opt = store->selected_installed_detail.Get();
    const auto& inst_versions = store->selected_installed_versions.Get();
    const auto& plan_items = store->installed_plan_preview.Get();
    const bool loading_detail = store->loading_installed_detail.Get();

    const std::string needle = detail::ToLowerCopy(filter_input.Get().text);
    const std::size_t mode = filter_mode.Get();

    std::vector<PkgGroup> filtered;
    filtered.reserve(all_groups.size());
    for (const auto& g : all_groups) {
        if (mode == 1 && g.versions.size() <= 1) continue;
        if (mode == 2 && !g.has_degraded) continue;
        if (!needle.empty()) {
            const std::string hay = detail::ToLowerCopy(g.key + " " + g.description);
            if (hay.find(needle) == std::string::npos) continue;
        }
        filtered.push_back(g);
    }

    const PkgGroup* selected_group = nullptr;
    for (const auto& g : all_groups) {
        if (g.key == selected_key) {
            selected_group = &g;
            break;
        }
    }

    View toolbar = Row {
        TextField(filter_input)
            .Placeholder("筛选已安装软件包 (如 gcc, mcpp, gtk4)...")
            .Variant(TextFieldVariant::Outlined)
            .LeadingIcon(icons::SearchIcon())
            .OnChanged([filter_input](const TextEditingValue& next) mutable { filter_input = next; })
            .With(Grow(1.0F)),
        Chip("全部 (" + std::to_string(all_groups.size()) + ")", mode == 0)
            .OnChanged([filter_mode](bool) mutable { filter_mode = 0; }),
        Chip("多版本", mode == 1)
            .OnChanged([filter_mode](bool) mutable { filter_mode = 1; }),
        Chip("含异常", mode == 2)
            .OnChanged([filter_mode](bool) mutable { filter_mode = 2; }),
        ActionButton(
            "更新索引",
            ButtonTone::Secondary,
            [store, tasks, toast] { store->UpdatePackagesAsync(tasks, toast); },
            true,
            icons::RefreshIcon(),
            true
        ),
    }.With(
        Padding(EdgeInsets::Symmetric(20.0F, 12.0F)),
        Spacing(10.0F),
        Background(theme.colors.surface),
        CrossAlign(CrossAxisAlignment::Center)
    );

    std::vector<View> list_rows;
    list_rows.reserve(filtered.size());
    for (const auto& g : filtered) {
        const bool is_sel = g.key == selected_key;
        const Color row_bg = is_sel ? theme.colors.primary_container : sp.card_bg;
        const Color row_border = is_sel ? theme.colors.primary : sp.border_soft;

        list_rows.push_back(
            Column {
                Row {
                    Text(g.name, TextRole::Body)
                        .Style(TextStyle{
                            .font = Font::System(14.0F).WithWeight(FontWeight::SemiBold),
                            .foreground = is_sel ? theme.colors.primary : theme.colors.on_surface,
                        })
                        .With(Grow(1.0F)),
                    g.versions.size() > 1
                        ? StatusBadge("x" + std::to_string(g.versions.size()), sp.info, sp.info_bg)
                        : View{},
                    g.has_degraded
                        ? StatusBadge("异常", sp.danger, sp.danger_bg)
                        : View{},
                }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                Row {
                    StatusBadge(
                        g.ns.empty() ? std::string("local") : g.ns,
                        theme.colors.on_surface_variant,
                        sp.muted_bg
                    ),
                    StatusBadge(
                        g.active_version.empty() ? std::string("未激活") : g.active_version,
                        g.active_version.empty() ? sp.warning : sp.success,
                        g.active_version.empty() ? sp.warning_bg : sp.success_bg
                    ),
                }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                g.description.empty()
                    ? View{}
                    : Text(g.description, TextRole::Label)
                          .Style(TextStyle{
                              .font = Font::System(12.0F),
                              .foreground = theme.colors.on_surface_variant,
                          }),
            }.With(
                Padding(12.0F),
                Spacing(6.0F),
                Background(row_bg),
                Border{row_border, is_sel ? 1.5F : 1.0F},
                CornerRadius(10.0F),
                Focusable(true),
                PointerCursor(PointerCursorKind::Hand),
                CrossAlign(CrossAxisAlignment::Stretch)
            )
            .OnClick([store, tasks, g] { store->LoadInstalledDetail(tasks, g); })
            .Key(g.key)
        );
    }

    View left_pane = filtered.empty()
                         ? EmptyState(icons::PackagesIcon(), "无匹配的已安装软件包", "尝试清除筛选词或切换过滤条件")
                         : View(ScrollView(
                               Column(std::move(list_rows))
                                   .With(Padding(14.0F), Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))
                           ).ScrollAxis(Axis::Vertical)
                                    .With(Grow(1.0F), ScrollBar{}));

    View right_pane = View{};
    if (selected_group == nullptr) {
        right_pane = EmptyState(
            icons::PackagesIcon(),
            "选择一个已安装软件包",
            "可在此切换活跃版本、卸载指定版本、清理冗余旧版本或检查依赖信息"
        );
    } else {
        const PkgGroup grp = *selected_group;
        const std::string plain_name = grp.name;
        const std::string active_ver = grp.active_version;

        // Build version rows by combining list_installed_versions (has paths) & group.versions (has degraded status)
        std::vector<View> ver_rows;
        for (std::size_t i = 0; i < grp.versions.size(); ++i) {
            const auto& v = grp.versions[i];
            std::string path_str;
            for (const auto& iv : inst_versions) {
                if (iv.version == v.version) {
                    path_str = iv.path;
                    break;
                }
            }
            const std::string full_target = detail::MakeTargetSpec(v.ns, v.name, v.version);

            ver_rows.push_back(
                Row {
                    Column {
                        Row {
                            Text(v.version.empty() ? std::string("(default)") : v.version, TextRole::Body)
                                .Style(TextStyle{
                                    .font = Font::Monospace(13.5F).WithWeight(FontWeight::Bold),
                                    .foreground = theme.colors.on_surface,
                                }),
                            v.is_active
                                ? StatusBadge("当前活跃", sp.success, sp.success_bg)
                                : (v.is_degraded
                                       ? StatusBadge(v.status, sp.danger, sp.danger_bg)
                                       : StatusBadge("未激活", theme.colors.on_surface_variant, sp.muted_bg)),
                        }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                        path_str.empty()
                            ? View{}
                            : Text(path_str, TextRole::Label)
                                  .Style(TextStyle{
                                      .font = Font::Monospace(11.5F),
                                      .foreground = theme.colors.on_surface_variant,
                                  }),
                    }.With(Spacing(4.0F), Grow(1.0F)),
                    (!v.is_active && !v.version.empty())
                        ? ActionButton(
                              "设为活跃",
                              ButtonTone::Secondary,
                              [store, tasks, toast, plain_name, ver_str = v.version] {
                                  store->ActivateVersionAsync(tasks, toast, plain_name, ver_str);
                              },
                              true,
                              icons::CheckIcon(),
                              true
                          )
                        : View{},
                    ActionButton(
                        "卸载",
                        ButtonTone::Danger,
                        [store, tasks, toast, dialog, full_target] {
                            dialog.Show(
                                "确认卸载软件包",
                                "确定要卸载 `" + full_target + "` 吗？若存在反向依赖，xlings 将拒绝删除。",
                                "卸载",
                                "取消",
                                [store, tasks, toast, full_target] {
                                    store->RemovePackageAsync(tasks, toast, full_target);
                                },
                                {}
                            );
                        },
                        true,
                        icons::TrashIcon(),
                        true
                    ),
                }.With(
                    Padding(12.0F),
                    Spacing(8.0F),
                    Background(v.is_active ? sp.success_bg : sp.card_elevated_bg),
                    Border{v.is_active ? WithAlpha(sp.success, 0.35F) : sp.border_soft, 1.0F},
                    CornerRadius(10.0F),
                    CrossAlign(CrossAxisAlignment::Center)
                ).Key("ver-" + full_target + "-" + std::to_string(i))
            );
        }

        std::vector<View> detail_sections;

        detail_sections.push_back(
            Card(
                Column {
                    Row {
                        Column {
                            Row {
                                Text(grp.name, TextRole::Title)
                                    .Style(TextStyle{
                                        .font = Font::System(20.0F).WithWeight(FontWeight::Bold),
                                        .foreground = theme.colors.on_surface,
                                    }),
                                StatusBadge(grp.ns.empty() ? std::string("local") : grp.ns, theme.colors.primary, theme.colors.primary_container),
                                grp.active_version.empty()
                                    ? View{}
                                    : StatusBadge("active: " + grp.active_version, sp.success, sp.success_bg),
                            }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                            Text(grp.description.empty() ? std::string("暂无描述信息") : grp.description, TextRole::Body)
                                .Style(TextStyle{
                                    .font = Font::System(13.0F),
                                    .foreground = theme.colors.on_surface_variant,
                                }),
                        }.With(Spacing(6.0F), Grow(1.0F)),
                        loading_detail ? View(ProgressCircle().With(Frame{.width = 18.0F, .height = 18.0F})) : View{},
                    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Start)),
                    Flow {
                        ActionButton(
                            "升级此包",
                            ButtonTone::Primary,
                            [store, tasks, toast, plain_name] {
                                store->UpdatePackagesAsync(tasks, toast, plain_name);
                            },
                            true,
                            icons::DownloadIcon(),
                            true
                        ),
                        ActionButton(
                            "预览更新计划",
                            ButtonTone::Secondary,
                            [store, tasks, key = grp.key] {
                                store->PreviewInstallPlan(tasks, {key}, false, false);
                            },
                            true,
                            std::nullopt,
                            true
                        ),
                        (grp.versions.size() > 1 && !active_ver.empty())
                            ? ActionButton(
                                  "清理早于 " + active_ver + " 的旧版本",
                                  ButtonTone::Secondary,
                                  [store, tasks, toast, dialog, plain_name, active_ver] {
                                      dialog.Show(
                                          "清理旧版本",
                                          "将移除所有早于 `" + active_ver + "` 且无其他包引用的 `" + plain_name + "` 旧版本。",
                                          "确认清理",
                                          "取消",
                                          [store, tasks, toast, plain_name, active_ver] {
                                              store->RemovePackageAsync(tasks, toast, plain_name, active_ver, false);
                                          },
                                          {}
                                      );
                                  },
                                  true,
                                  icons::TrashIcon(),
                                  true
                              )
                            : View{},
                    }.With(Spacing(8.0F)),
                }.With(Spacing(14.0F), CrossAlign(CrossAxisAlignment::Stretch))
            )
        );

        detail_sections.push_back(
            Card(
                Column {
                    SectionHeader(
                        "已安装版本 (" + std::to_string(grp.versions.size()) + ")",
                        "点击“设为活跃”可即时切换当前 SubOS 中的默认命令指向"
                    ),
                    Column(std::move(ver_rows)).With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
            )
        );

        if (!plan_items.empty()) {
            std::vector<View> plan_rows;
            for (std::size_t i = 0; i < plan_items.size(); ++i) {
                const auto& item = plan_items[i];
                plan_rows.push_back(
                    Row {
                        StatusBadge(
                            item.action_code > 0 ? std::string("需安装/变更") : std::string("已就绪"),
                            item.action_code > 0 ? sp.info : sp.success,
                            item.action_code > 0 ? sp.info_bg : sp.success_bg
                        ),
                        Text(item.spec, TextRole::Body)
                            .Style(TextStyle{.font = Font::Monospace(12.5F), .foreground = theme.colors.on_surface})
                            .With(Grow(1.0F)),
                        item.note.empty()
                            ? View{}
                            : Text(item.note, TextRole::Label)
                                  .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
                    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center))
                        .Key("inst-plan-" + item.spec + "-" + std::to_string(i))
                );
            }
            detail_sections.push_back(
                Card(
                    Column {
                        SectionHeader("依赖安装/更新计划预览", "来自 xlings plan_install"),
                        Column(std::move(plan_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
                )
            );
        }

        if (detail_opt.has_value() && !detail_opt->available_versions.empty()) {
            std::vector<View> avail_chips;
            const std::size_t max_chips = std::min<std::size_t>(detail_opt->available_versions.size(), 40);
            for (std::size_t i = 0; i < max_chips; ++i) {
                const std::string& av = detail_opt->available_versions[i];
                const bool already_installed = std::ranges::any_of(
                    grp.versions, [&](const InstalledPkg& iv) { return iv.version == av; });
                const std::string target_spec = detail::MakeTargetSpec(grp.ns, grp.name, av);
                avail_chips.push_back(
                    ActionButton(
                        already_installed ? (av + " ✓") : av,
                        already_installed ? ButtonTone::Secondary : ButtonTone::Ghost,
                        [store, tasks, toast, dialog, target_spec, already_installed] {
                            if (already_installed) return;
                            dialog.Show(
                                "安装指定版本",
                                "是否安装并激活 `" + target_spec + "`？",
                                "安装并激活",
                                "取消",
                                [store, tasks, toast, target_spec] {
                                    store->InstallPackageAsync(tasks, toast, target_spec, false, true, {}, false);
                                },
                                {}
                            );
                        },
                        true,
                        std::nullopt,
                        true
                    ).Key("avail-" + av)
                );
            }
            detail_sections.push_back(
                Card(
                    Column {
                        SectionHeader(
                            "索引可用版本 (" + std::to_string(detail_opt->available_versions.size()) + ")",
                            "点击未安装的版本可快速安装并激活"
                        ),
                        Flow(std::move(avail_chips)).With(Spacing(6.0F)),
                    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
                )
            );
        }

        if (detail_opt.has_value() && !detail_opt->fields.empty()) {
            std::vector<View> kv_rows;
            for (std::size_t i = 0; i < detail_opt->fields.size(); ++i) {
                const auto& f = detail_opt->fields[i];
                kv_rows.push_back(
                    KeyValueRow(f.label, f.value, f.highlight, f.label == "runtime deps" || f.label == "build deps")
                        .Key("kv-" + f.label + "-" + std::to_string(i))
                );
            }
            detail_sections.push_back(
                Card(
                    Column {
                        SectionHeader("软件包元信息", "来自 xlings package_info"),
                        Column(std::move(kv_rows)).With(Spacing(4.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
                )
            );
        }

        right_pane = ScrollView(
            Column(std::move(detail_sections))
                .With(Padding(18.0F), Spacing(14.0F), CrossAlign(CrossAxisAlignment::Stretch))
        ).ScrollAxis(Axis::Vertical)
            .With(Grow(1.0F), ScrollBar{});
    }

    return Column {
        std::move(toolbar),
        Divider(),
        ResizableSplit(std::move(left_pane), std::move(right_pane), 340.0F, 250.0F, 520.0F),
    }.With(Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch));
}

} // namespace hxlings

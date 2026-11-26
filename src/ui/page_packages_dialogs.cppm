export module hxlings.page.packages.dialogs;

import std;
import huxerui;
import xlings.types;
import xlings.parser;
import hxlings.theme;
import hxlings.icons;
import hxlings.store;
import hxlings.components;

using namespace huxerui;

export namespace hxlings::packages_detail {


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

bool ContainsKey(const std::vector<std::string>& items, std::string_view target) {
    return std::ranges::any_of(items, [&](const std::string& item) { return item == target; });
}

void SetKeyPresent(std::vector<std::string>& items, const std::string& target, bool present) {
    const auto it = std::ranges::find(items, target);
    if (present) {
        if (it == items.end()) items.push_back(target);
    } else if (it != items.end()) {
        items.erase(it);
    }
}

std::string SummarizeVersionsPreview(const std::vector<RemovePlanItem>& items) {
    if (items.empty()) return {};
    const std::size_t shown = std::min<std::size_t>(items.size(), 4);
    std::string out = "待移除: ";
    for (std::size_t i = 0; i < shown; ++i) {
        if (i > 0) out += ", ";
        out += items[i].version;
    }
    if (items.size() > shown) {
        out += " 等共 " + std::to_string(items.size()) + " 个版本";
    }
    return out;
}

} // namespace hxlings::packages_detail


export namespace hxlings {

[[huxerui::composable]]
View BatchPruneDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast
) {
    auto excluded_keys = UseState(std::vector<std::string>{});

    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    const auto& plans = store->expired_plans.Get();
    const bool scanning = store->scanning_expired.Get();
    const bool task_running = store->task_running.Get();
    const auto& excluded = excluded_keys.Get();

    std::vector<std::string> all_removable_keys;
    std::vector<ExpiredPackagePlan> selected_targets;
    std::size_t total_removable_pkgs = 0;
    std::size_t protected_only_pkgs = 0;
    std::uint64_t total_reclaimable_bytes = 0;
    std::size_t sel_pkgs = 0;
    std::size_t sel_vers = 0;
    std::uint64_t sel_bytes = 0;

    for (const auto& item : plans) {
        if (item.plan.remove.empty()) {
            if (!item.plan.keep.empty()) ++protected_only_pkgs;
            continue;
        }
        ++total_removable_pkgs;
        total_reclaimable_bytes += item.plan.total_bytes;
        all_removable_keys.push_back(item.key);
        if (!packages_detail::ContainsKey(excluded, item.key)) {
            ++sel_pkgs;
            sel_vers += item.plan.remove.size();
            sel_bytes += item.plan.total_bytes;
            selected_targets.push_back(item);
        }
    }

    std::string header_text;
    if (scanning) {
        header_text = "正在通过 xlings dry-run 扫描过期版本...";
    } else if (total_removable_pkgs == 0) {
        header_text = "未发现可清理的过期版本";
    } else if (protected_only_pkgs > 0) {
        header_text = "可清理 " + std::to_string(total_removable_pkgs) + " 包 · " +
                      FormatBytes(total_reclaimable_bytes) + " (已跳过 " +
                      std::to_string(protected_only_pkgs) + " 保护包)";
    } else {
        header_text = "共扫描到 " + std::to_string(total_removable_pkgs) + " 个可清理包 · 可释放 " +
                      FormatBytes(total_reclaimable_bytes);
    }

    View summary_bar = Row {
        Text(header_text, TextRole::Body)
            .Style(TextStyle{
                .font = Font::System(12.5F).WithWeight(FontWeight::SemiBold),
                .foreground = theme.colors.on_surface_variant,
            })
            .With(Grow(1.0F)),
        ActionButton(
            "全选",
            ButtonTone::Ghost,
            [excluded_keys]() mutable { excluded_keys = std::vector<std::string>{}; },
            !scanning && total_removable_pkgs > 0,
            std::nullopt,
            true
        ),
        ActionButton(
            "全不选",
            ButtonTone::Ghost,
            [excluded_keys, all_removable_keys]() mutable { excluded_keys = all_removable_keys; },
            !scanning && total_removable_pkgs > 0,
            std::nullopt,
            true
        ),
        RefreshButton(
            "重新扫描",
            [excluded_keys, store, tasks]() mutable {
                excluded_keys = std::vector<std::string>{};
                store->ScanExpiredVersions(tasks);
            },
            scanning,
            ButtonTone::Secondary,
            !store->skip_initial_refresh,
            true
        ),
    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center));

    View list_body = View{};
    if (scanning) {
        list_body = Row {
            Spacer{}.With(Grow(1.0F)),
            InlineSpinner(20.0F, theme.colors.primary),
            Text("正在通过 xlings dry-run 扫描所有多版本包...", TextRole::Body)
                .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
            Spacer{}.With(Grow(1.0F)),
        }.With(Padding(EdgeInsets::Symmetric(12.0F, 36.0F)), Spacing(10.0F), CrossAlign(CrossAxisAlignment::Center));
    } else if (plans.empty()) {
        list_body = EmptyState(
            icons::CheckIcon(),
            "暂无可清理的过期版本",
            "所有多版本软件包均无早于基准版本的冗余旧版本"
        );
    } else {
        std::vector<View> rows;
        rows.reserve(plans.size());
        for (std::size_t i = 0; i < plans.size(); ++i) {
            const auto& item = plans[i];
            const bool has_removable = !item.plan.remove.empty();
            const bool is_checked = has_removable && !packages_detail::ContainsKey(excluded, item.key);

            std::string kept_summary;
            if (!item.plan.keep.empty()) {
                for (std::size_t k = 0; k < item.plan.keep.size(); ++k) {
                    if (k > 0) kept_summary += "；";
                    kept_summary += item.plan.keep[k].version;
                    if (!item.plan.keep[k].reason.empty()) {
                        kept_summary += " (" + item.plan.keep[k].reason + ")";
                    }
                }
            }

            if (has_removable) {
                rows.push_back(
                    Column {
                        Row {
                            Checkbox(is_checked).OnChanged([excluded_keys, key = item.key](bool checked) mutable {
                                excluded_keys.Update([&](std::vector<std::string>& list) {
                                    packages_detail::SetKeyPresent(list, key, !checked);
                                });
                            }),
                            Text(item.key, TextRole::Body)
                                .Style(TextStyle{
                                    .font = Font::System(13.5F).WithWeight(FontWeight::SemiBold),
                                    .foreground = theme.colors.on_surface,
                                })
                                .With(Grow(1.0F)),
                            StatusBadge(
                                (item.cutoff_is_active ? "早于 " : "保留最新 ") + item.cutoff_version,
                                theme.colors.on_surface_variant,
                                sp.muted_bg
                            ),
                            StatusBadge(
                                std::to_string(item.plan.remove.size()) + " 个 · " +
                                    FormatBytes(item.plan.total_bytes),
                                sp.info,
                                sp.info_bg
                            ),
                        }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                        Text(packages_detail::SummarizeVersionsPreview(item.plan.remove), TextRole::Label)
                            .Style(TextStyle{
                                .font = Font::Monospace(11.5F),
                                .foreground = theme.colors.on_surface_variant,
                            }),
                        kept_summary.empty()
                            ? View{}
                            : Text("受保护保留: " + kept_summary, TextRole::Label)
                                  .Style(TextStyle{
                                      .font = Font::System(11.5F),
                                      .foreground = sp.warning,
                                  }),
                    }.With(
                        Padding(10.0F),
                        Spacing(6.0F),
                        Background(is_checked ? sp.card_elevated_bg : sp.card_bg),
                        Border{is_checked ? WithAlpha(theme.colors.primary, 0.35F) : sp.border_soft, 1.0F},
                        CornerRadius(8.0F),
                        CrossAlign(CrossAxisAlignment::Stretch)
                    ).Key("prune-item-" + item.key + "-" + std::to_string(i))
                );
            } else {
                rows.push_back(
                    Column {
                        Row {
                            StatusBadge("受依赖保护", sp.warning, sp.warning_bg),
                            Text(item.key, TextRole::Body)
                                .Style(TextStyle{
                                    .font = Font::System(13.5F).WithWeight(FontWeight::SemiBold),
                                    .foreground = theme.colors.on_surface,
                                })
                                .With(Grow(1.0F)),
                            StatusBadge("早于 " + item.cutoff_version, theme.colors.on_surface_variant, sp.muted_bg),
                        }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                        Text("已跳过删除: " + kept_summary, TextRole::Label)
                            .Style(TextStyle{
                                .font = Font::System(11.5F),
                                .foreground = sp.warning,
                            }),
                    }.With(
                        Padding(10.0F),
                        Spacing(6.0F),
                        Background(sp.warning_bg),
                        Border{WithAlpha(sp.warning, 0.35F), 1.0F},
                        CornerRadius(8.0F),
                        CrossAlign(CrossAxisAlignment::Stretch)
                    ).Key("prune-kept-" + item.key + "-" + std::to_string(i))
                );
            }
        }

        list_body = ScrollView(
            Column(std::move(rows))
                .With(Padding(4.0F), Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))
        ).ScrollAxis(Axis::Vertical)
            .With(Frame{.height = 210.0F}, ScrollBar{});
    }

    View body = Column {
        std::move(summary_bar),
        std::move(list_body),
    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch));

    const bool can_execute = !selected_targets.empty() && !scanning && !task_running;

    View actions = Row {
        Text(
            "已选 " + std::to_string(sel_pkgs) + " 包 · " + std::to_string(sel_vers) + " 版本 · " +
                FormatBytes(sel_bytes),
            TextRole::Label
        ).Style(TextStyle{
            .font = Font::System(12.5F).WithWeight(FontWeight::Medium),
            .foreground = theme.colors.on_surface_variant,
        }).With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }, true, std::nullopt, true),
        ActionButton(
            "执行批量清理",
            ButtonTone::Danger,
            [ctx, store, tasks, toast, selected_targets = std::move(selected_targets)]() mutable {
                ctx.Dismiss();
                store->BatchRemoveExpiredAsync(tasks, toast, std::move(selected_targets));
            },
            can_execute,
            icons::TrashIcon(),
            true
        ),
    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Center));

    return DialogCardShell("批量清理过期版本", std::move(body), std::move(actions), 560.0F);
}

[[huxerui::composable]]
View BatchUpdateDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast
) {
    auto excluded_keys = UseState(std::vector<std::string>{});
    auto activate_after_update = UseState(true);

    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    const auto& plans = store->updatable_plans.Get();
    const bool scanning = store->scanning_updates.Get();
    const bool task_running = store->task_running.Get();
    const auto& excluded = excluded_keys.Get();
    const bool activate_opt = activate_after_update.Get();

    std::vector<std::string> all_updatable_keys;
    std::vector<UpdatablePackagePlan> selected_targets;
    std::size_t total_updatable_pkgs = 0;
    std::size_t new_ver_count = 0;
    std::size_t rev_update_count = 0;
    std::size_t skipped_pkgs = 0;

    for (const auto& item : plans) {
        if (item.is_skipped) {
            ++skipped_pkgs;
            continue;
        }
        ++total_updatable_pkgs;
        if (item.is_revision_only) {
            ++rev_update_count;
        } else {
            ++new_ver_count;
        }
        all_updatable_keys.push_back(item.key);
        if (!packages_detail::ContainsKey(excluded, item.key)) {
            selected_targets.push_back(item);
        }
    }

    std::string header_text;
    if (scanning) {
        header_text = "正在扫描全部已安装软件包的可用更新...";
    } else if (total_updatable_pkgs == 0 && skipped_pkgs == 0) {
        header_text = "所有已安装软件包均为最新版本";
    } else if (total_updatable_pkgs == 0) {
        header_text = "暂无可更新包 (已跳过 " + std::to_string(skipped_pkgs) + " 个未在索引中的包)";
    } else {
        header_text = "可更新 " + std::to_string(total_updatable_pkgs) + " 包 (" +
                      std::to_string(new_ver_count) + " 新版 · " +
                      std::to_string(rev_update_count) + " 修订)";
        if (skipped_pkgs > 0) {
            header_text += " · 跳过 " + std::to_string(skipped_pkgs);
        }
    }

    View summary_bar = Row {
        Text(header_text, TextRole::Body)
            .Style(TextStyle{
                .font = Font::System(12.5F).WithWeight(FontWeight::SemiBold),
                .foreground = theme.colors.on_surface_variant,
            })
            .With(Grow(1.0F)),
        ActionButton(
            "全选",
            ButtonTone::Ghost,
            [excluded_keys]() mutable { excluded_keys = std::vector<std::string>{}; },
            !scanning && total_updatable_pkgs > 0,
            std::nullopt,
            true
        ),
        ActionButton(
            "全不选",
            ButtonTone::Ghost,
            [excluded_keys, all_updatable_keys]() mutable { excluded_keys = all_updatable_keys; },
            !scanning && total_updatable_pkgs > 0,
            std::nullopt,
            true
        ),
        RefreshButton(
            "重新扫描",
            [excluded_keys, store, tasks]() mutable {
                excluded_keys = std::vector<std::string>{};
                store->ScanUpdatablePackages(tasks);
            },
            scanning,
            ButtonTone::Secondary,
            !store->skip_initial_refresh,
            true
        ),
    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center));

    View options_bar = Row {
        Checkbox(activate_opt).OnChanged([activate_after_update](bool checked) mutable {
            activate_after_update = checked;
        }),
        Text("升级后自动切换为活跃版本", TextRole::Label)
            .Style(TextStyle{
                .font = Font::System(12.5F),
                .foreground = theme.colors.on_surface_variant,
            })
            .With(Grow(1.0F)),
        ActionButton(
            "同步索引并重扫",
            ButtonTone::Ghost,
            [excluded_keys, store, tasks, toast]() mutable {
                excluded_keys = std::vector<std::string>{};
                store->SyncIndexAndScanUpdatesAsync(tasks, toast);
            },
            !scanning && !task_running && !store->skip_initial_refresh,
            icons::DownloadIcon(),
            true
        ),
    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center));

    View list_body = View{};
    if (scanning) {
        list_body = Row {
            Spacer{}.With(Grow(1.0F)),
            InlineSpinner(20.0F, theme.colors.primary),
            Text("正在检查全部已安装软件包的可用更新...", TextRole::Body)
                .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
            Spacer{}.With(Grow(1.0F)),
        }.With(Padding(EdgeInsets::Symmetric(12.0F, 36.0F)), Spacing(10.0F), CrossAlign(CrossAxisAlignment::Center));
    } else if (plans.empty()) {
        list_body = EmptyState(
            icons::CheckIcon(),
            "所有软件包均已是最新版本",
            "可点击“同步索引并重扫”拉取最新索引后再检查"
        );
    } else {
        std::vector<View> rows;
        rows.reserve(plans.size());
        for (std::size_t i = 0; i < plans.size(); ++i) {
            const auto& item = plans[i];
            if (!item.is_skipped) {
                const bool is_checked = !packages_detail::ContainsKey(excluded, item.key);
                const std::string delta_badge = item.is_revision_only
                    ? (item.latest_version + " (r" + std::to_string(item.installed_revision) + " → r" +
                       std::to_string(item.latest_revision) + ")")
                    : ((item.current_version.empty() ? std::string("?") : item.current_version) + " → " +
                       item.latest_version);
                const std::string target_spec = item.key + "@" + item.latest_version;
                std::string subtitle = "安装目标: " + target_spec;
                if (item.active_version.empty()) {
                    subtitle += " · 原未激活，保持未激活安装";
                } else if (activate_opt && (!item.is_revision_only || item.active_version == item.latest_version)) {
                    subtitle += " · 完成后激活 " + item.latest_version;
                } else {
                    subtitle += " · 保持当前活跃版本 " + item.active_version;
                }

                rows.push_back(
                    Column {
                        Row {
                            Checkbox(is_checked).OnChanged([excluded_keys, key = item.key](bool checked) mutable {
                                excluded_keys.Update([&](std::vector<std::string>& list) {
                                    packages_detail::SetKeyPresent(list, key, !checked);
                                });
                            }),
                            Text(item.key, TextRole::Body)
                                .Style(TextStyle{
                                    .font = Font::System(13.5F).WithWeight(FontWeight::SemiBold),
                                    .foreground = theme.colors.on_surface,
                                })
                                .With(Grow(1.0F)),
                            StatusBadge(
                                item.is_revision_only ? std::string("配方修订") : std::string("新版本"),
                                item.is_revision_only ? sp.warning : sp.success,
                                item.is_revision_only ? sp.warning_bg : sp.success_bg
                            ),
                            StatusBadge(delta_badge, sp.info, sp.info_bg),
                        }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                        Text(subtitle, TextRole::Label)
                            .Style(TextStyle{
                                .font = Font::Monospace(11.5F),
                                .foreground = theme.colors.on_surface_variant,
                            }),
                    }.With(
                        Padding(10.0F),
                        Spacing(6.0F),
                        Background(is_checked ? sp.card_elevated_bg : sp.card_bg),
                        Border{is_checked ? WithAlpha(theme.colors.primary, 0.35F) : sp.border_soft, 1.0F},
                        CornerRadius(8.0F),
                        CrossAlign(CrossAxisAlignment::Stretch)
                    ).Key("upd-item-" + item.key + "-" + std::to_string(i))
                );
            } else {
                rows.push_back(
                    Column {
                        Row {
                            StatusBadge("未在索引中", sp.warning, sp.warning_bg),
                            Text(item.key, TextRole::Body)
                                .Style(TextStyle{
                                    .font = Font::System(13.5F).WithWeight(FontWeight::SemiBold),
                                    .foreground = theme.colors.on_surface,
                                })
                                .With(Grow(1.0F)),
                            item.current_version.empty()
                                ? View{}
                                : StatusBadge(item.current_version, theme.colors.on_surface_variant, sp.muted_bg),
                        }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                        Text("已跳过更新: " + item.skip_reason, TextRole::Label)
                            .Style(TextStyle{
                                .font = Font::System(11.5F),
                                .foreground = sp.warning,
                            }),
                    }.With(
                        Padding(10.0F),
                        Spacing(6.0F),
                        Background(sp.warning_bg),
                        Border{WithAlpha(sp.warning, 0.35F), 1.0F},
                        CornerRadius(8.0F),
                        CrossAlign(CrossAxisAlignment::Stretch)
                    ).Key("upd-skip-" + item.key + "-" + std::to_string(i))
                );
            }
        }

        list_body = ScrollView(
            Column(std::move(rows))
                .With(Padding(4.0F), Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))
        ).ScrollAxis(Axis::Vertical)
            .With(Frame{.height = 190.0F}, ScrollBar{});
    }

    View body = Column {
        std::move(summary_bar),
        std::move(options_bar),
        std::move(list_body),
    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch));

    const std::size_t sel_count = selected_targets.size();
    const bool can_execute = !selected_targets.empty() && !scanning && !task_running;

    View actions = Row {
        Text(
            "已选 " + std::to_string(sel_count) + " 个待更新包",
            TextRole::Label
        ).Style(TextStyle{
            .font = Font::System(12.5F).WithWeight(FontWeight::Medium),
            .foreground = theme.colors.on_surface_variant,
        }).With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }, true, std::nullopt, true),
        ActionButton(
            "执行批量更新",
            ButtonTone::Primary,
            [ctx, store, tasks, toast, selected_targets = std::move(selected_targets), activate_opt]() mutable {
                ctx.Dismiss();
                store->BatchUpdatePackagesAsync(tasks, toast, std::move(selected_targets), activate_opt);
            },
            can_execute,
            icons::DownloadIcon(),
            true
        ),
    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Center));

    return DialogCardShell("批量更新软件包", std::move(body), std::move(actions), 560.0F);
}

} // namespace hxlings

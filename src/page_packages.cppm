export module hxlings.page.packages;

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

} // namespace hxlings::detail

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
        if (!detail::ContainsKey(excluded, item.key)) {
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
        ActionButton(
            "重新扫描",
            ButtonTone::Secondary,
            [excluded_keys, store, tasks]() mutable {
                excluded_keys = std::vector<std::string>{};
                store->ScanExpiredVersions(tasks);
            },
            !scanning && !store->skip_initial_refresh,
            icons::RefreshIcon(),
            true
        ),
    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center));

    View list_body = View{};
    if (scanning) {
        list_body = Row {
            Spacer{}.With(Grow(1.0F)),
            ProgressCircle().With(Frame{.width = 20.0F, .height = 20.0F}),
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
            const bool is_checked = has_removable && !detail::ContainsKey(excluded, item.key);

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
                                    detail::SetKeyPresent(list, key, !checked);
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
                        Text(detail::SummarizeVersionsPreview(item.plan.remove), TextRole::Label)
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
        if (!detail::ContainsKey(excluded, item.key)) {
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
        ActionButton(
            "重新扫描",
            ButtonTone::Secondary,
            [excluded_keys, store, tasks]() mutable {
                excluded_keys = std::vector<std::string>{};
                store->ScanUpdatablePackages(tasks);
            },
            !scanning && !store->skip_initial_refresh,
            icons::RefreshIcon(),
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
            ProgressCircle().With(Frame{.width = 20.0F, .height = 20.0F}),
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
                const bool is_checked = !detail::ContainsKey(excluded, item.key);
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
                                    detail::SetKeyPresent(list, key, !checked);
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

[[huxerui::composable]]
View PackagesPage() {
    const auto store = UseService<XlingsStore>();
    const TaskScope tasks = UseApplicationTaskScope();
    const TaskScope ui_tasks = UseTaskScope();
    const ToastHandle toast = UseToast();
    const DialogHandle dialog = UseDialog();
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    auto filter_input = UseState(TextEditingValue::FromText(""));
    auto filter_mode = UseState<std::size_t>(0); // 0=all, 1=multi-version, 2=degraded
    auto checked_targets = UseState(std::vector<std::string>{});
    const ViewHandle filter_handle = UseViewHandle();

    Lifecycle([store, ui_tasks, filter_handle] {
        if (store->active_nav.Get() == 1 && store->ConsumeFocusSearchNonce()) {
            ui_tasks.Post([filter_handle] {
                (void)filter_handle.RequestFocus(true);
            });
        }
    }, store->focus_search_nonce.Get());

    Lifecycle([store, tasks] {
        if (!store->skip_initial_refresh && store->active_nav.Get() == 1) {
            store->EnsureInstalledDetailLoaded(tasks, false);
        }
    }, store->active_nav.Get(), store->installed_groups.Get().size());

    Lifecycle([store, tasks, ui_tasks, dialog, toast] {
        if (store->ConsumeBatchUpdateNonce()) {
            ui_tasks.Post([store, tasks, dialog, toast] {
                if (!store->has_scanned_updates.Get() && !store->scanning_updates.Get() && !store->skip_initial_refresh) {
                    store->ScanUpdatablePackages(tasks);
                }
                dialog.Show([store, tasks, toast](DialogContext ctx) {
                    return BatchUpdateDialogContent(ctx, store, tasks, toast);
                });
            });
        }
    }, store->open_batch_update_nonce.Get());

    Lifecycle([store, tasks, ui_tasks, dialog, toast] {
        if (store->ConsumeBatchCleanupNonce()) {
            ui_tasks.Post([store, tasks, dialog, toast] {
                if (!store->has_scanned_expired.Get() && !store->scanning_expired.Get() && !store->skip_initial_refresh) {
                    store->ScanExpiredVersions(tasks);
                }
                dialog.Show([store, tasks, toast](DialogContext ctx) {
                    return BatchPruneDialogContent(ctx, store, tasks, toast);
                });
            });
        }
    }, store->open_batch_cleanup_nonce.Get());

    const auto& all_groups = store->installed_groups.Get();
    const std::string selected_key = store->selected_installed_key.Get();
    const auto& detail_opt = store->selected_installed_detail.Get();
    const auto& inst_versions = store->selected_installed_versions.Get();
    const auto& plan_items = store->installed_plan_preview.Get();
    const bool loading_detail = store->loading_installed_detail.Get();
    const auto& remove_plan_opt = store->selected_remove_plan.Get();
    const bool loading_remove_plan = store->loading_remove_plan.Get();

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

    View toolbar = Column {
        Row {
            TextField(filter_input)
                .Placeholder("筛选已安装包...")
                .Variant(TextFieldVariant::Outlined)
                .LeadingIcon(icons::SearchIcon())
                .OnChanged([filter_input](const TextEditingValue& next) mutable { filter_input = next; })
                .With(Grow(1.0F), filter_handle)
                .Key("packages-filter-input"),
            ActionButton(
                "批量更新",
                ButtonTone::Primary,
                [dialog, store, tasks, toast] {
                    if (!store->has_scanned_updates.Get() && !store->scanning_updates.Get() && !store->skip_initial_refresh) {
                        store->ScanUpdatablePackages(tasks);
                    }
                    dialog.Show([store, tasks, toast](DialogContext ctx) {
                        return BatchUpdateDialogContent(ctx, store, tasks, toast);
                    });
                },
                true,
                icons::DownloadIcon(),
                true
            ),
            ActionButton(
                "批量清理",
                ButtonTone::Secondary,
                [dialog, store, tasks, toast] {
                    if (!store->has_scanned_expired.Get() && !store->scanning_expired.Get() && !store->skip_initial_refresh) {
                        store->ScanExpiredVersions(tasks);
                    }
                    dialog.Show([store, tasks, toast](DialogContext ctx) {
                        return BatchPruneDialogContent(ctx, store, tasks, toast);
                    });
                },
                true,
                icons::TrashIcon(),
                true
            ),
            ActionButton(
                "更新索引",
                ButtonTone::Secondary,
                [store, tasks, toast] { store->UpdatePackagesAsync(tasks, toast); },
                true,
                icons::RefreshIcon(),
                true
            ),
        }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
        Flow {
            Chip("全部 (" + std::to_string(all_groups.size()) + ")", mode == 0)
                .OnChanged([filter_mode](bool) mutable { filter_mode = 0; }),
            Chip("多版本", mode == 1)
                .OnChanged([filter_mode](bool) mutable { filter_mode = 1; }),
            Chip("含异常", mode == 2)
                .OnChanged([filter_mode](bool) mutable { filter_mode = 2; }),
        }.With(Spacing(8.0F)),
    }.With(
        Padding(EdgeInsets::Symmetric(16.0F, 12.0F)),
        Spacing(8.0F),
        Background(theme.colors.surface),
        CrossAlign(CrossAxisAlignment::Stretch)
    );

    std::vector<View> list_rows;
    list_rows.reserve(filtered.size());
    for (std::size_t i = 0; i < filtered.size(); ++i) {
        const auto& g = filtered[i];
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
            .Key("pkg-" + g.key + "-" + std::to_string(i))
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

        std::string cutoff_ver = active_ver;
        if (cutoff_ver.empty() && grp.versions.size() > 1) {
            for (const auto& v : grp.versions) {
                if (!v.version.empty() &&
                    (cutoff_ver.empty() || CompareVersionStrings(v.version, cutoff_ver) > 0)) {
                    cutoff_ver = v.version;
                }
            }
        }

        const auto& raw_checked = checked_targets.Get();
        std::vector<std::string> non_active_targets;
        std::vector<std::string> selected_for_pkg;
        for (const auto& v : grp.versions) {
            if (!v.is_active && grp.versions.size() > 1) {
                const std::string target_spec = detail::MakeTargetSpec(v.ns, v.name, v.version);
                non_active_targets.push_back(target_spec);
                if (detail::ContainsKey(raw_checked, target_spec)) {
                    selected_for_pkg.push_back(target_spec);
                }
            }
        }

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
            const bool selectable = !v.is_active && grp.versions.size() > 1;
            const bool is_checked = selectable && detail::ContainsKey(selected_for_pkg, full_target);

            ver_rows.push_back(
                Column {
                    Row {
                        selectable
                            ? View(
                                  Checkbox(is_checked).OnChanged([checked_targets, full_target](bool checked) mutable {
                                      checked_targets.Update([&](std::vector<std::string>& list) {
                                          detail::SetKeyPresent(list, full_target, checked);
                                      });
                                  })
                              )
                            : View{},
                        Text(v.version.empty() ? std::string("(default)") : v.version, TextRole::Body)
                            .Style(TextStyle{
                                .font = Font::Monospace(13.5F).WithWeight(FontWeight::Bold),
                                .foreground = theme.colors.on_surface,
                            })
                            .With(Grow(1.0F)),
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
                    Row {
                        Spacer{}.With(Grow(1.0F)),
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
                    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                }.With(
                    Padding(12.0F),
                    Spacing(6.0F),
                    Background(v.is_active ? sp.success_bg : sp.card_elevated_bg),
                    Border{v.is_active ? WithAlpha(sp.success, 0.35F) : sp.border_soft, 1.0F},
                    CornerRadius(10.0F),
                    CrossAlign(CrossAxisAlignment::Stretch)
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
                        (grp.versions.size() > 1 && !cutoff_ver.empty())
                            ? ActionButton(
                                  "预演清理旧版",
                                  ButtonTone::Secondary,
                                  [store, tasks, pkg_key = grp.key, cutoff_ver] {
                                      store->PreviewSingleRemoveOlderThan(tasks, pkg_key, cutoff_ver);
                                  },
                                  true,
                                  std::nullopt,
                                  true
                              )
                            : View{},
                        (grp.versions.size() > 1 && !cutoff_ver.empty())
                            ? ActionButton(
                                  "清理早于 " + cutoff_ver + " 的旧版本",
                                  ButtonTone::Secondary,
                                  [store, tasks, toast, dialog, pkg_key = grp.key, cutoff_ver] {
                                      dialog.Show(
                                          "清理旧版本",
                                          "将移除所有早于 `" + cutoff_ver + "` 且无其他包引用的 `" + pkg_key + "` 旧版本。",
                                          "确认清理",
                                          "取消",
                                          [store, tasks, toast, pkg_key, cutoff_ver] {
                                              store->RemovePackageAsync(tasks, toast, pkg_key, cutoff_ver, false);
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

        if (loading_remove_plan) {
            detail_sections.push_back(
                Card(
                    Row {
                        ProgressCircle().With(Frame{.width = 16.0F, .height = 16.0F}),
                        Text("正在预演清理计划...", TextRole::Body)
                            .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
                    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center))
                )
            );
        } else if (remove_plan_opt.has_value()) {
            const RemovePlan& rp = *remove_plan_opt;
            std::vector<View> rp_rows;

            if (rp.remove.empty() && rp.keep.empty()) {
                rp_rows.push_back(
                    Text("没有早于 " + rp.older_than + " 的可清理旧版本", TextRole::Label)
                        .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                );
            } else {
                for (std::size_t ri = 0; ri < rp.remove.size(); ++ri) {
                    const auto& item = rp.remove[ri];
                    rp_rows.push_back(
                        Row {
                            Text(item.version, TextRole::Body)
                                .Style(TextStyle{
                                    .font = Font::Monospace(12.5F),
                                    .foreground = theme.colors.on_surface,
                                })
                                .With(Grow(1.0F)),
                            StatusBadge(FormatBytes(item.bytes), sp.info, sp.info_bg),
                        }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center))
                            .Key("rp-rem-" + item.version + "-" + std::to_string(ri))
                    );
                }
                for (std::size_t ki = 0; ki < rp.keep.size(); ++ki) {
                    const auto& k = rp.keep[ki];
                    const std::string keep_txt = k.reason.empty() ? k.version : (k.version + " · " + k.reason);
                    rp_rows.push_back(
                        Row {
                            StatusBadge("保留", sp.warning, sp.warning_bg),
                            Text(keep_txt, TextRole::Label)
                                .Style(TextStyle{.foreground = sp.warning})
                                .With(Grow(1.0F)),
                        }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center))
                            .Key("rp-keep-" + k.version + "-" + std::to_string(ki))
                    );
                }
            }

            detail_sections.push_back(
                Card(
                    Column {
                        SectionHeader("过期版本清理预演 (早于 " + rp.older_than + ")"),
                        rp.remove.empty()
                            ? View{}
                            : Flow {
                                  StatusBadge(
                                      "可移除 " + std::to_string(rp.remove.size()) + " 个 · 释放 " +
                                          FormatBytes(rp.total_bytes),
                                      sp.info,
                                      sp.info_bg
                                  ),
                              }.With(Spacing(6.0F)),
                        Column(std::move(rp_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                        Flow {
                            !rp.remove.empty()
                                ? ActionButton(
                                      "确认清理这 " + std::to_string(rp.remove.size()) + " 个旧版本",
                                      ButtonTone::Danger,
                                      [dialog, store, tasks, toast, pkg_key = grp.key, older = rp.older_than,
                                       cnt = rp.remove.size(), total_bytes = rp.total_bytes] {
                                          dialog.Show(
                                              "确认清理过期版本",
                                              "将移除 `" + pkg_key + "` 早于 `" + older + "` 的 " +
                                                  std::to_string(cnt) + " 个旧版本，预计释放 " +
                                                  FormatBytes(total_bytes) + "。",
                                              "确认清理",
                                              "取消",
                                              [store, tasks, toast, pkg_key, older] {
                                                  store->RemovePackageAsync(tasks, toast, pkg_key, older, false);
                                              },
                                              {}
                                          );
                                      },
                                      !rp.older_than.empty(),
                                      icons::TrashIcon(),
                                      true
                                  )
                                : View{},
                            ActionButton(
                                "关闭预演",
                                ButtonTone::Ghost,
                                [store] { store->selected_remove_plan = std::optional<RemovePlan>{}; },
                                true,
                                std::nullopt,
                                true
                            ),
                        }.With(Spacing(8.0F)),
                    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
                )
            );
        }

        const bool all_non_active_selected =
            !non_active_targets.empty() && selected_for_pkg.size() == non_active_targets.size();

        detail_sections.push_back(
            Card(
                Column {
                    SectionHeader(
                        "已安装版本 (" + std::to_string(grp.versions.size()) + ")",
                        "点击“设为活跃”可即时切换当前 SubOS 中的默认命令指向"
                    ),
                    !non_active_targets.empty()
                        ? Flow {
                              ActionButton(
                                  all_non_active_selected
                                      ? "取消全选"
                                      : ("全选未激活 (" + std::to_string(non_active_targets.size()) + ")"),
                                  ButtonTone::Secondary,
                                  [checked_targets, non_active_targets, all_non_active_selected]() mutable {
                                      if (all_non_active_selected) {
                                          checked_targets = std::vector<std::string>{};
                                      } else {
                                          checked_targets = non_active_targets;
                                      }
                                  },
                                  true,
                                  icons::CheckIcon(),
                                  true
                              ),
                              !selected_for_pkg.empty()
                                  ? ActionButton(
                                        "批量卸载已选 (" + std::to_string(selected_for_pkg.size()) + ")",
                                        ButtonTone::Danger,
                                        [dialog, store, tasks, toast, selected_for_pkg, checked_targets]() mutable {
                                            dialog.Show(
                                                "确认批量卸载",
                                                "确定要卸载已选的 " + std::to_string(selected_for_pkg.size()) +
                                                    " 个版本吗？若存在反向依赖，xlings 将拒绝删除相应版本。",
                                                "批量卸载",
                                                "取消",
                                                [store, tasks, toast, selected_for_pkg, checked_targets]() mutable {
                                                    checked_targets = std::vector<std::string>{};
                                                    store->BatchRemoveTargetsAsync(tasks, toast, selected_for_pkg);
                                                },
                                                {}
                                            );
                                        },
                                        true,
                                        icons::TrashIcon(),
                                        true
                                    )
                                  : View{},
                          }.With(Spacing(8.0F))
                        : View{},
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
                    ).Key("avail-" + av + "-" + std::to_string(i))
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
                .With(Padding(16.0F), Spacing(14.0F), CrossAlign(CrossAxisAlignment::Stretch))
        ).ScrollAxis(Axis::Vertical)
            .With(Grow(1.0F), ScrollBar{});
    }

    return Column {
        std::move(toolbar),
        Divider(),
        ResizableSplit(std::move(left_pane), std::move(right_pane), 280.0F, 220.0F, 480.0F),
    }.With(Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch));
}

} // namespace hxlings

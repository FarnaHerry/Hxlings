export module hxlings.page.dashboard;

import std;
import huxerui;
import xlings.types;
import hxlings.theme;
import hxlings.icons;
import hxlings.store;
import hxlings.components;

using namespace huxerui;

export namespace hxlings {

[[huxerui::composable]]
View DashboardPage() {
    const auto store = UseService<XlingsStore>();
    const TaskScope tasks = UseApplicationTaskScope();
    const ToastHandle toast = UseToast();
    const DialogHandle dialog = UseDialog();
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    const auto& ver = store->version_info.Get();
    const auto& env = store->env.Get();
    const auto& status = store->system_status.Get();
    const auto& raw_pkgs = store->installed_raw.Get();
    const auto& groups = store->installed_groups.Get();
    const auto& suboses = store->subos_list.Get();
    const auto& shims = store->active_shims.Get();
    const auto& repos = store->repos.Get();
    const auto& idx_vers = store->index_versions.Get();
    const auto& logs = store->recent_logs.Get();
    const bool refreshing = store->refreshing.Get();
    const bool loaded = store->initial_loaded.Get();

    std::string active_snap = "—";
    if (!idx_vers.empty() && !idx_vers.front().current.empty()) {
        active_snap = idx_vers.front().current;
    }

    View hero_badges = Flow {
        StatusBadge(
            ver.version.empty() ? std::string("xlings") : ("xlings " + ver.version),
            theme.colors.primary,
            theme.colors.primary_container
        ),
        StatusBadge(
            "SubOS: " + (env.active_subos.empty() ? std::string("—") : env.active_subos),
            sp.info,
            sp.info_bg
        ),
        StatusBadge(
            "镜像: " + (env.mirror.empty() ? std::string("—") : env.mirror),
            sp.success,
            sp.success_bg
        ),
        ver.protocol_version.empty()
            ? View{}
            : StatusBadge("Protocol " + ver.protocol_version, theme.colors.on_surface_variant, sp.muted_bg),
    }.With(Spacing(8.0F));

    View hero_actions = Flow {
        ActionButton(
            refreshing ? "刷新中..." : "刷新全部状态",
            ButtonTone::Primary,
            [store, tasks] { store->RefreshAll(tasks); },
            !refreshing,
            icons::RefreshIcon(),
            true
        ),
        ActionButton(
            "更新索引",
            ButtonTone::Secondary,
            [store, tasks, toast] { store->UpdatePackagesAsync(tasks, toast); },
            true,
            icons::DownloadIcon(),
            true
        ),
        ActionButton(
            "更新 xlings",
            ButtonTone::Secondary,
            [store, tasks, toast, dialog] {
                dialog.Show(
                    "更新 xlings 自身",
                    "将运行 `xlings self update --yes --agent` 检查并更新 xlings。",
                    "立即更新",
                    "取消",
                    [store, tasks, toast] { store->RunSelfUpdateAsync(tasks, toast); },
                    {}
                );
            },
            true,
            icons::PlayIcon(),
            true
        ),
        ActionButton(
            "快速诊断",
            ButtonTone::Secondary,
            [store, tasks, toast] {
                store->RunDoctorAsync(tasks, toast, false, false, false, false);
                store->active_nav = 5;
            },
            true,
            icons::WrenchIcon(),
            true
        ),
        ActionButton(
            "缓存清理预演",
            ButtonTone::Secondary,
            [store, tasks, toast] {
                store->RunSelfCleanAsync(tasks, toast, true);
                store->active_nav = 5;
            },
            true,
            icons::TrashIcon(),
            true
        ),
    }.With(Spacing(8.0F));

    View hero_card = Card(
        Column {
            Row {
                Column {
                    Text("xlings 开发环境总览", TextRole::Title)
                        .Style(TextStyle{
                            .font = Font::System(20.0F).WithWeight(FontWeight::Bold),
                            .foreground = theme.colors.on_surface,
                        }),
                    Text("集中管理软件包版本、SubOS 隔离环境、包索引仓库与系统诊断", TextRole::Body)
                        .Style(TextStyle{
                            .font = Font::System(13.0F),
                            .foreground = theme.colors.on_surface_variant,
                        }),
                }.With(Spacing(4.0F), Grow(1.0F)),
            },
            std::move(hero_badges),
            std::move(hero_actions),
            Divider(),
            KeyValueRow("XLINGS_HOME", env.xlings_home, false, true),
            KeyValueRow("XLINGS_DATA", env.data_dir, false, true),
            KeyValueRow("XLINGS_SUBOS", env.subos_dir, false, true),
            KeyValueRow("可执行目录 (bin)", env.bin_dir, false, true),
            KeyValueRow("xlings 程序路径", env.binary_path, true, true),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    );

    View stats_row = Row {
        StatCard(
            "已安装软件",
            std::to_string(groups.size()),
            "共 " + std::to_string(raw_pkgs.size()) + " 个已安装版本",
            icons::PackagesIcon(),
            theme.colors.primary,
            [store] { store->active_nav = 1; }
        ).With(Grow(1.0F)),
        StatCard(
            "SubOS 环境",
            std::to_string(suboses.size()),
            "当前激活: " + (env.active_subos.empty() ? std::string("default") : env.active_subos),
            icons::SubosIcon(),
            sp.info,
            [store] { store->active_nav = 3; }
        ).With(Grow(1.0F)),
        StatCard(
            "活跃 Shims",
            std::to_string(shims.size()),
            "当前 SubOS 可执行命令",
            icons::TerminalIcon(),
            sp.success,
            [store] { store->active_nav = 3; }
        ).With(Grow(1.0F)),
        StatCard(
            "索引仓库",
            std::to_string(repos.size()),
            "当前快照: " + active_snap,
            icons::IndexIcon(),
            sp.warning,
            [store] { store->active_nav = 4; }
        ).With(Grow(1.0F)),
    }.With(Spacing(14.0F));

    View health_card = View{};
    if (!status.findings.empty()) {
        std::vector<View> finding_views;
        for (std::size_t idx = 0; idx < status.findings.size(); ++idx) {
            const auto& f = status.findings[idx];
            finding_views.push_back(
                Column {
                    Row {
                        StatusBadge(f.severity.empty() ? std::string("warning") : f.severity, sp.danger, sp.danger_bg),
                        Text(f.code, TextRole::Label)
                            .Style(TextStyle{.font = Font::Monospace(12.0F), .foreground = theme.colors.on_surface}),
                        Spacer{}.With(Grow(1.0F)),
                        Text("SubOS: " + f.subos, TextRole::Label)
                            .Style(TextStyle{.font = Font::System(12.0F), .foreground = theme.colors.on_surface_variant}),
                    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Text(f.summary, TextRole::Body)
                        .Style(TextStyle{.font = Font::System(13.0F), .foreground = theme.colors.on_surface}),
                    f.hint.empty()
                        ? View{}
                        : Text("建议: " + f.hint, TextRole::Label)
                              .Style(TextStyle{.font = Font::Monospace(12.0F), .foreground = sp.warning}),
                }.With(
                    Padding(12.0F),
                    Spacing(6.0F),
                    Background(sp.warning_bg),
                    Border{WithAlpha(sp.warning, 0.35F), 1.0F},
                    CornerRadius(10.0F),
                    CrossAlign(CrossAxisAlignment::Stretch)
                ).Key("finding-" + std::to_string(idx))
            );
        }

        health_card = Card(
            Column {
                SectionHeader(
                    "检测到环境绑定问题 (" + std::to_string(status.findings.size()) + ")",
                    "来自 xlings system_status 诊断",
                    Row {
                        ActionButton(
                            "预演修复",
                            ButtonTone::Secondary,
                            [store, tasks, toast] {
                                store->RunDoctorAsync(tasks, toast, true, true, true, false);
                                store->active_nav = 5;
                            },
                            true,
                            std::nullopt,
                            true
                        ),
                        ActionButton(
                            "立即修复 (--fix)",
                            ButtonTone::Danger,
                            [store, tasks, toast, dialog] {
                                dialog.Show(
                                    "执行 doctor --fix",
                                    "将运行 `xlings self doctor --fix --yes --agent` 修复所有 SubOS 的绑定问题。",
                                    "执行修复",
                                    "取消",
                                    [store, tasks, toast] { store->RunDoctorAsync(tasks, toast, true, true, false, false); },
                                    {}
                                );
                            },
                            true,
                            icons::WrenchIcon(),
                            true
                        ),
                    }.With(Spacing(8.0F))
                ),
                Column(std::move(finding_views)).With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch)),
            }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
        );
    } else if (loaded) {
        health_card = Row {
            Image(icons::CheckIcon())
                .Fit(ImageFit::Contain)
                .Tint(sp.success)
                .With(Frame{.width = 18.0F, .height = 18.0F}),
            Text("环境绑定状态正常 — 当前 SubOS 中所有链接与发布锚点一致", TextRole::Body)
                .Style(TextStyle{
                    .font = Font::System(13.5F).WithWeight(FontWeight::Medium),
                    .foreground = sp.success,
                }),
        }.With(
            Padding(EdgeInsets::Symmetric(16.0F, 12.0F)),
            Spacing(10.0F),
            Background(sp.success_bg),
            Border{WithAlpha(sp.success, 0.30F), 1.0F},
            CornerRadius(10.0F),
            CrossAlign(CrossAxisAlignment::Center)
        );
    }

    std::vector<View> log_rows;
    const std::size_t show_count = std::min<std::size_t>(logs.size(), 12);
    for (std::size_t i = 0; i < show_count; ++i) {
        const auto& entry = logs[i];
        const bool ok = entry.exit_code == 0;
        log_rows.push_back(
            Row {
                StatusBadge(
                    ok ? std::string("exit 0") : ("exit " + std::to_string(entry.exit_code)),
                    ok ? sp.success : sp.danger,
                    ok ? sp.success_bg : sp.danger_bg
                ),
                Text(entry.command, TextRole::Body)
                    .Style(TextStyle{.font = Font::Monospace(12.5F), .foreground = theme.colors.on_surface})
                    .With(Grow(1.0F)),
                Text(entry.started, TextRole::Label)
                    .Style(TextStyle{.font = Font::Monospace(11.5F), .foreground = theme.colors.on_surface_variant}),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 7.0F)),
                Spacing(10.0F),
                Background(sp.card_elevated_bg),
                CornerRadius(8.0F),
                CrossAlign(CrossAxisAlignment::Center)
            ).Key("selflog-" + entry.trace + "-" + std::to_string(i))
        );
    }

    View logs_card = Card(
        Column {
            SectionHeader(
                "最近操作记录",
                "来自 xlings self log --json（最近 12 条）"
            ),
            log_rows.empty()
                ? Text("暂无操作记录", TextRole::Body)
                      .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                : View(Column(std::move(log_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    );

    return ScrollView(
        Column {
            std::move(hero_card),
            std::move(stats_row),
            std::move(health_card),
            std::move(logs_card),
        }.With(
            Padding(22.0F),
            Spacing(16.0F),
            CrossAlign(CrossAxisAlignment::Stretch)
        )
    ).ScrollAxis(Axis::Vertical)
        .With(Grow(1.0F), ScrollBar{});
}

} // namespace hxlings

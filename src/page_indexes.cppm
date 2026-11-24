export module hxlings.page.indexes;

import std;
import huxerui;
import xlings.types;
import xlings.parser;
import hxlings.theme;
import hxlings.icons;
import hxlings.store;
import hxlings.components;

using namespace huxerui;

export namespace hxlings {

[[huxerui::composable]]
View AddRepoDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast
) {
    auto name_input = UseState(TextEditingValue::FromText(""));
    auto url_input = UseState(TextEditingValue::FromText(""));
    auto err_msg = UseState(std::string{});

    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    auto submit = [ctx, store, tasks, toast, name_input, url_input, err_msg]() mutable {
        const std::string name = name_input.Get().text;
        const std::string url = url_input.Get().text;
        if (name.empty() || url.empty()) {
            err_msg = std::string("仓库名称与 Git URL 均不能为空");
            return;
        }
        ctx.Dismiss();
        store->AddRepoAsync(tasks, toast, name, url);
    };

    View body = Column {
        TextField(name_input)
            .Label("仓库名称 / 命名空间 *")
            .Placeholder("例如: xim 或 custom")
            .Variant(TextFieldVariant::Outlined)
            .OnChanged([name_input, err_msg](const TextEditingValue& v) mutable {
                name_input = v;
                err_msg = std::string{};
            }),
        TextField(url_input)
            .Label("Git 仓库 URL *")
            .Placeholder("https://gitee.com/sunrisepeak/xim-pkgindex.git")
            .Variant(TextFieldVariant::Outlined)
            .OnChanged([url_input, err_msg](const TextEditingValue& v) mutable {
                url_input = v;
                err_msg = std::string{};
            })
            .OnSubmitted(submit),
        err_msg.Get().empty()
            ? View{}
            : Text(err_msg.Get(), TextRole::Label).Style(TextStyle{.foreground = sp.danger}),
    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch));

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton("保存仓库", ButtonTone::Primary, submit, true, icons::PlusIcon()),
    }.With(Spacing(10.0F));

    return DialogCardShell("添加或更新索引仓库", std::move(body), std::move(actions), 480.0F);
}

[[huxerui::composable]]
View AddXpkgDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast
) {
    auto path_input = UseState(TextEditingValue::FromText(""));
    auto err_msg = UseState(std::string{});

    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    auto submit = [ctx, store, tasks, toast, path_input, err_msg]() mutable {
        const std::string path = path_input.Get().text;
        if (path.empty()) {
            err_msg = std::string("请输入 .lua 配方文件的绝对路径");
            return;
        }
        ctx.Dismiss();
        store->AddLocalXpkgAsync(tasks, toast, path);
    };

    View body = Column {
        TextField(path_input)
            .Label("xpkg 文件路径 (.lua) *")
            .Placeholder("/path/to/package.lua")
            .Variant(TextFieldVariant::Outlined)
            .OnChanged([path_input, err_msg](const TextEditingValue& v) mutable {
                path_input = v;
                err_msg = std::string{};
            })
            .OnSubmitted(submit),
        err_msg.Get().empty()
            ? View{}
            : Text(err_msg.Get(), TextRole::Label).Style(TextStyle{.foreground = sp.danger}),
    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch));

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton("添加配方", ButtonTone::Primary, submit, true, icons::PlusIcon()),
    }.With(Spacing(10.0F));

    return DialogCardShell("注册本地 xpkg 配方", std::move(body), std::move(actions), 480.0F);
}

[[huxerui::composable]]
View IndexesPage() {
    const auto store = UseService<XlingsStore>();
    const TaskScope tasks = UseApplicationTaskScope();
    const ToastHandle toast = UseToast();
    const DialogHandle dialog = UseDialog();
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    Lifecycle([store, tasks] {
        if (!store->skip_initial_refresh && store->active_nav.Get() == 4) {
            store->RefreshIndexes(tasks, false);
        }
    }, store->active_nav.Get());

    const auto& repos = store->repos.Get();
    const auto& idx_versions = store->index_versions.Get();
    const auto& local_xpkgs = store->local_xpkgs.Get();
    const bool loading_indexes = store->loading_indexes.Get();
    const bool loading_idx_versions = store->loading_index_versions.Get();
    const bool idx_versions_loaded = store->index_versions_loaded.Get();

    std::vector<View> repo_rows;
    for (std::size_t i = 0; i < repos.size(); ++i) {
        const auto& r = repos[i];
        repo_rows.push_back(
            Row {
                Column {
                    Row {
                        Text(r.name, TextRole::Body)
                            .Style(TextStyle{.font = Font::System(15.0F).WithWeight(FontWeight::Bold),
                                             .foreground = theme.colors.on_surface}),
                        StatusBadge(r.scope.empty() ? std::string("global") : r.scope, sp.info, sp.info_bg),
                    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                    ExternalLinkRow(r.url, "repo-link-" + r.name),
                }.With(Spacing(4.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                ActionButton(
                    "同步索引",
                    ButtonTone::Secondary,
                    [store, tasks, toast, name = r.name] { store->SyncIndexAsync(tasks, toast, name); },
                    true,
                    icons::RefreshIcon(),
                    true
                ),
                ActionButton(
                    "移除",
                    ButtonTone::Danger,
                    [store, tasks, toast, dialog, name = r.name] {
                        dialog.Show(
                            "移除索引仓库",
                            "确定要移除全局索引仓库 `" + name + "` 吗？",
                            "移除",
                            "取消",
                            [store, tasks, toast, name] { store->RemoveRepoAsync(tasks, toast, name); },
                            {}
                        );
                    },
                    true,
                    icons::TrashIcon(),
                    true
                ),
            }.With(
                Padding(12.0F),
                Spacing(10.0F),
                Background(sp.card_elevated_bg),
                Border{sp.border_soft, 1.0F},
                CornerRadius(10.0F),
                CrossAlign(CrossAxisAlignment::Center)
            ).Key("repo-" + r.name + "-" + std::to_string(i))
        );
    }

    View repos_card = Card(
        Column {
            SectionHeader(
                "索引仓库 (Index Repositories)",
                "管理全局 xim 包索引仓库及远程地址",
                Row {
                    RefreshButton(
                        "刷新索引与快照",
                        [store, tasks] { store->RefreshIndexes(tasks, true); },
                        loading_indexes || loading_idx_versions
                    ).Key("indexes-refresh-btn"),
                    ActionButton(
                        "添加 / 更新仓库",
                        ButtonTone::Primary,
                        [dialog, store, tasks, toast] {
                            dialog.Show([store, tasks, toast](DialogContext ctx) {
                                return AddRepoDialogContent(ctx, store, tasks, toast);
                            });
                        },
                        true,
                        icons::PlusIcon(),
                        true
                    ),
                }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center))
            ),
            repo_rows.empty()
                ? Text("暂无已配置的索引仓库", TextRole::Body)
                      .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                : View(Column(std::move(repo_rows)).With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    );

    std::vector<View> idx_cards;
    for (std::size_t ri = 0; ri < idx_versions.size(); ++ri) {
        const auto& repo_idx = idx_versions[ri];
        std::vector<View> snap_rows;
        const std::size_t max_snaps = std::min<std::size_t>(repo_idx.snapshots.size(), 15);
        for (std::size_t i = 0; i < max_snaps; ++i) {
            const auto& s = repo_idx.snapshots[i];
            const bool is_pinned = !repo_idx.pinned.empty() && repo_idx.pinned == s.index_version;
            snap_rows.push_back(
                Row {
                    Text(s.index_version, TextRole::Body)
                        .Style(TextStyle{.font = Font::Monospace(13.0F).WithWeight(FontWeight::Bold),
                                         .foreground = theme.colors.on_surface})
                        .With(Frame{.width = 140.0F}),
                    s.current ? StatusBadge("当前使用", sp.success, sp.success_bg) : View{},
                    s.installed ? StatusBadge("已本地缓存", sp.info, sp.info_bg) : View{},
                    is_pinned ? StatusBadge("已固定", sp.warning, sp.warning_bg) : View{},
                    Text(FormatLocalTimestamp(s.generated_at), TextRole::Label)
                        .Style(TextStyle{.font = Font::Monospace(11.5F),
                                         .foreground = theme.colors.on_surface_variant})
                        .With(Grow(1.0F)),
                    s.size_bytes > 0
                        ? Text(FormatBytes(s.size_bytes), TextRole::Label)
                              .Style(TextStyle{.font = Font::Monospace(11.5F),
                                               .foreground = theme.colors.on_surface_variant})
                        : View{},
                    !s.current
                        ? ActionButton(
                              "切换到此快照",
                              ButtonTone::Secondary,
                              [store, tasks, toast, dialog, repo_name = repo_idx.name, ver = s.index_version] {
                                  dialog.Show(
                                      "切换索引快照",
                                      "确定要将索引 `" + repo_name + "` 切换到快照 `" + ver + "` 吗？",
                                      "切换",
                                      "取消",
                                      [store, tasks, toast, repo_name, ver] {
                                          store->PinIndexVersionAsync(tasks, toast, repo_name, ver);
                                      },
                                      {}
                                  );
                              },
                              true,
                              std::nullopt,
                              true
                          )
                        : View{},
                }.With(
                    Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                    Spacing(8.0F),
                    Background(s.current ? sp.success_bg : sp.card_elevated_bg),
                    Border{s.current ? WithAlpha(sp.success, 0.30F) : sp.border_soft, 1.0F},
                    CornerRadius(8.0F),
                    CrossAlign(CrossAxisAlignment::Center)
                ).Key("snap-" + repo_idx.name + "-" + s.index_version + "-" + std::to_string(i))
            );
        }

        idx_cards.push_back(
            Card(
                Column {
                    SectionHeader(
                        "索引快照: " + repo_idx.name,
                        "当前: " + (repo_idx.current.empty() ? std::string("—") : repo_idx.current) +
                            "  ·  固定: " + (repo_idx.pinned.empty() ? std::string("未固定 (跟随最新)") : repo_idx.pinned),
                        ActionButton(
                            "同步最新",
                            ButtonTone::Secondary,
                            [store, tasks, toast, name = repo_idx.name] { store->SyncIndexAsync(tasks, toast, name); },
                            true,
                            icons::RefreshIcon(),
                            true
                        )
                    ),
                    snap_rows.empty()
                        ? Text("暂无已发布的索引快照", TextRole::Body)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        : View(Column(std::move(snap_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ).Key("idx-card-" + repo_idx.name + "-" + std::to_string(ri))
        );
    }

    std::vector<View> xpkg_rows;
    for (std::size_t i = 0; i < local_xpkgs.size(); ++i) {
        const auto& x = local_xpkgs[i];
        xpkg_rows.push_back(
            Row {
                Column {
                    Row {
                        Text(x.name, TextRole::Body)
                            .Style(TextStyle{.font = Font::System(14.0F).WithWeight(FontWeight::Bold),
                                             .foreground = theme.colors.on_surface}),
                        StatusBadge(x.version, sp.info, sp.info_bg),
                        StatusBadge(x.status, theme.colors.on_surface_variant, sp.muted_bg),
                    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                    SelectionArea(
                        Text(x.source, TextRole::Label)
                            .Style(TextStyle{.font = Font::Monospace(11.5F),
                                             .foreground = theme.colors.on_surface_variant})
                    ),
                }.With(Spacing(4.0F), Grow(1.0F)),
                ActionButton(
                    "移除",
                    ButtonTone::Danger,
                    [store, tasks, toast, dialog, name = x.name] {
                        dialog.Show(
                            "移除本地 xpkg",
                            "确定要从本地配置中移除 `" + name + "` 吗？",
                            "移除",
                            "取消",
                            [store, tasks, toast, name] { store->RemoveLocalXpkgAsync(tasks, toast, name); },
                            {}
                        );
                    },
                    true,
                    icons::TrashIcon(),
                    true
                ),
            }.With(
                Padding(10.0F),
                Spacing(8.0F),
                Background(sp.card_elevated_bg),
                Border{sp.border_soft, 1.0F},
                CornerRadius(8.0F),
                CrossAlign(CrossAxisAlignment::Center)
            ).Key("lxpkg-" + x.name + "-" + std::to_string(i))
        );
    }

    View xpkg_card = Card(
        Column {
            SectionHeader(
                "本地 xpkg 配方 (" + std::to_string(local_xpkgs.size()) + ")",
                "通过 `xlings config --add-xpkg` 注册的本地 Lua 包配方"
            ),
            Flow {
                ActionButton(
                    "添加本地 xpkg",
                    ButtonTone::Primary,
                    [dialog, store, tasks, toast] {
                        dialog.Show([store, tasks, toast](DialogContext ctx) {
                            return AddXpkgDialogContent(ctx, store, tasks, toast);
                        });
                    },
                    true,
                    icons::PlusIcon(),
                    true
                ),
                ActionButton(
                    "清理过期配方",
                    ButtonTone::Secondary,
                    [store, tasks, toast] { store->ClearLocalXpkgsAsync(tasks, toast, "stale"); },
                    !local_xpkgs.empty(),
                    icons::WrenchIcon(),
                    true
                ),
                ActionButton(
                    "清空全部配方",
                    ButtonTone::Danger,
                    [dialog, store, tasks, toast] {
                        dialog.Show(
                            "清空全部本地 xpkg 配方",
                            "确定执行 `xlings config --clear-xpkg all` 移除所有已注册的本地配方吗？",
                            "全部清空",
                            "取消",
                            [store, tasks, toast] { store->ClearLocalXpkgsAsync(tasks, toast, "all"); },
                            {}
                        );
                    },
                    !local_xpkgs.empty(),
                    icons::TrashIcon(),
                    true
                ),
            }.With(Spacing(8.0F)),
            xpkg_rows.empty()
                ? Text("尚未注册本地 xpkg 配方", TextRole::Body)
                      .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                : View(Column(std::move(xpkg_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    );

    std::vector<View> sections;
    sections.push_back(std::move(repos_card));
    if (idx_cards.empty()) {
        if (loading_idx_versions) {
            sections.push_back(
                Card(
                    Row {
                        InlineSpinner(16.0F, theme.colors.primary),
                        Column {
                            Text("正在后台查询远程索引快照版本...", TextRole::Body)
                                .Style(TextStyle{.font = Font::System(13.5F).WithWeight(FontWeight::SemiBold),
                                                 .foreground = theme.colors.on_surface}),
                            Text("此操作不会阻塞仓库列表与本地配方操作", TextRole::Label)
                                .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
                        }.With(Spacing(2.0F), Grow(1.0F)),
                    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center))
                )
            );
        } else if (!idx_versions_loaded) {
            sections.push_back(
                Card(
                    Row {
                        Column {
                            Text("尚未查询远程索引快照", TextRole::Body)
                                .Style(TextStyle{.font = Font::System(13.5F).WithWeight(FontWeight::SemiBold),
                                                 .foreground = theme.colors.on_surface}),
                            Text("点击右侧按钮按需拉取历史索引快照与固定状态", TextRole::Label)
                                .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
                        }.With(Spacing(2.0F), Grow(1.0F)),
                        RefreshButton(
                            "立即查询快照",
                            [store, tasks] { store->RefreshIndexes(tasks, true); },
                            loading_idx_versions
                        ),
                    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center))
                )
            );
        }
    } else {
        for (auto& c : idx_cards) sections.push_back(std::move(c));
    }
    sections.push_back(std::move(xpkg_card));

    return ScrollView(
        Column(std::move(sections))
            .With(Padding(22.0F), Spacing(16.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Grow(1.0F), ScrollBar{});
}

} // namespace hxlings

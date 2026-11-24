export module hxlings.page.store;

import std;
import huxerui;
import xlings.types;
import hxlings.theme;
import hxlings.icons;
import hxlings.store;
import hxlings.components;

using namespace huxerui;

namespace hxlings::detail {

struct InstalledBadgeLookup {
    std::unordered_map<std::string, std::string> by_key;
    std::unordered_map<std::string, std::string> by_ns_name;
    std::unordered_map<std::string, std::string> by_name;

    [[nodiscard]] std::optional<std::string> Find(const SearchPkg& pkg) const {
        if (const auto it = by_key.find(pkg.package); it != by_key.end()) {
            return it->second;
        }
        if (!pkg.ns.empty()) {
            if (const auto it = by_ns_name.find(pkg.ns + '\0' + pkg.name); it != by_ns_name.end()) {
                return it->second;
            }
        } else if (const auto it = by_name.find(pkg.name); it != by_name.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

std::shared_ptr<InstalledBadgeLookup> BuildInstalledBadgeLookup(const std::vector<PkgGroup>& installed_groups) {
    auto lookup = std::make_shared<InstalledBadgeLookup>();
    for (const auto& g : installed_groups) {
        const std::string label = g.active_version.empty()
                                      ? std::string("已安装")
                                      : ("已装 " + g.active_version);
        lookup->by_key.try_emplace(g.key, label);
        if (!g.ns.empty()) {
            lookup->by_ns_name.try_emplace(g.ns + '\0' + g.name, label);
        }
        lookup->by_name.try_emplace(g.name, label);
    }
    return lookup;
}

} // namespace hxlings::detail

export namespace hxlings {

[[huxerui::composable]]
View StorePage() {
    const auto store = UseService<XlingsStore>();
    const TaskScope tasks = UseApplicationTaskScope();
    const TaskScope ui_tasks = UseTaskScope();
    const ToastHandle toast = UseToast();
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    auto query_input = UseState(TextEditingValue::FromText(""));
    auto selected_category = UseState(std::string{});
    auto chosen_version = UseState(std::string{});
    auto chosen_version_pkg = UseState(std::string{});
    auto chosen_subos = UseState(std::string{});
    auto opt_activate = UseState(true);
    auto opt_global = UseState(false);
    auto opt_reconfig = UseState(false);
    const ScrollController list_scroll = UseScrollController();
    const ViewHandle search_handle = UseViewHandle();

    Lifecycle([store, tasks] {
        if (store->active_nav.Get() == 2 &&
            !store->skip_initial_refresh &&
            !store->has_searched.Get() &&
            !store->searching.Get()) {
            store->RunSearch(tasks, "", "");
        }
    }, store->active_nav.Get());

    Lifecycle([store, ui_tasks, search_handle] {
        if (store->active_nav.Get() == 2 && store->ConsumeFocusSearchNonce()) {
            ui_tasks.Post([search_handle] {
                (void)search_handle.RequestFocus(true);
            });
        }
    }, store->focus_search_nonce.Get());

    const auto& results = store->search_results.Get();
    const bool searching = store->searching.Get();
    const bool has_searched = store->has_searched.Get();
    const std::string& search_error = store->search_error.Get();
    const std::string selected_pkg = store->selected_store_pkg.Get();
    const auto& detail_opt = store->selected_store_detail.Get();
    const bool loading_detail = store->loading_store_detail.Get();
    const auto& plan_items = store->store_plan_preview.Get();
    const auto& installed_groups = store->installed_groups.Get();
    const auto& suboses = store->subos_list.Get();

    const std::string cur_cat = selected_category.Get();

    auto trigger_search = [store, tasks, query_input, selected_category, list_scroll](std::string override_kw = {}) mutable {
        const std::string kw = override_kw.empty() ? query_input.Get().text : override_kw;
        (void)list_scroll.ScrollTo(0.0F);
        store->RunSearch(tasks, kw, selected_category.Get());
    };

    const std::array<std::pair<std::string, std::string>, 7> categories{{
        {"", "全部类别"},
        {"compiler", "编译器"},
        {"runtime", "运行时"},
        {"tools", "工具"},
        {"library", "开发库"},
        {"language-server", "LSP"},
        {"cpp", "C++"},
    }};

    std::vector<View> cat_chips;
    for (const auto& [cat_id, cat_label] : categories) {
        const bool active = cur_cat == cat_id;
        cat_chips.push_back(
            Chip(cat_label, active).OnChanged([store, tasks, query_input, selected_category, list_scroll, cat_id](bool) mutable {
                selected_category = cat_id;
                (void)list_scroll.ScrollTo(0.0F);
                store->RunSearch(tasks, query_input.Get().text, cat_id);
            }).Key("cat-" + (cat_id.empty() ? std::string("all") : cat_id))
        );
    }

    const std::array<std::string, 8> quick_keywords{
        "gcc", "llvm", "node", "rust", "python", "cmake", "mcpp", "huxerui"
    };
    std::vector<View> quick_btns;
    for (const auto& kw : quick_keywords) {
        quick_btns.push_back(
            ActionButton(
                kw,
                ButtonTone::Ghost,
                [store, tasks, query_input, selected_category, list_scroll, kw]() mutable {
                    query_input = TextEditingValue::FromText(kw);
                    (void)list_scroll.ScrollTo(0.0F);
                    store->RunSearch(tasks, kw, selected_category.Get());
                },
                true,
                std::nullopt,
                true
            ).Key("qk-" + kw)
        );
    }

    View top_bar = Column {
        Row {
            SearchBox(query_input, {
                .placeholder = "搜索软件包关键词，例如 gcc、node、llvm、cmake、rust...",
                .focus_handle = search_handle,
                .input_key = "store-search-input",
                .on_submitted = [trigger_search]() mutable { trigger_search(); },
                .on_clear = [store, tasks, selected_category, list_scroll]() mutable {
                    (void)list_scroll.ScrollTo(0.0F);
                    if (!store->skip_initial_refresh) {
                        store->RunSearch(tasks, "", selected_category.Get());
                    }
                },
            }).With(Grow(1.0F)),
            ActionButton(
                "搜索",
                ButtonTone::Primary,
                [trigger_search]() mutable { trigger_search(); },
                true,
                icons::SearchIcon(),
                false,
                searching
            ),
        }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Center)),
        Flow(std::move(cat_chips)).With(Spacing(8.0F)),
        Row {
            Text("热门检索:", TextRole::Label)
                .Style(TextStyle{.font = Font::System(12.0F), .foreground = theme.colors.on_surface_variant}),
            Flow(std::move(quick_btns)).With(Spacing(4.0F)).With(Grow(1.0F)),
        }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
    }.With(
        Padding(EdgeInsets::Symmetric(20.0F, 12.0F)),
        Spacing(10.0F),
        Background(theme.colors.surface),
        CrossAlign(CrossAxisAlignment::Stretch)
    );

    View left_pane = View{};
    if (searching) {
        left_pane = Column {
            InlineSpinner(28.0F, theme.colors.primary),
            Text("正在检索软件包...", TextRole::Body)
                .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
        }.With(
            Grow(1.0F),
            Spacing(12.0F),
            MainAlign(MainAxisAlignment::Center),
            CrossAlign(CrossAxisAlignment::Center)
        );
    } else if (!search_error.empty()) {
        left_pane = Column {
            EmptyState(icons::WarningIcon(), "搜索软件包失败", search_error),
            Row {
                ActionButton(
                    "重试搜索",
                    ButtonTone::Primary,
                    [trigger_search]() mutable { trigger_search(); },
                    true,
                    icons::RefreshIcon(),
                    true
                ),
                ActionButton(
                    "同步索引",
                    ButtonTone::Secondary,
                    [store, tasks, toast = UseToast()] { store->UpdatePackagesAsync(tasks, toast); },
                    true,
                    std::nullopt,
                    true
                ),
            }.With(Spacing(8.0F), MainAlign(MainAxisAlignment::Center)),
        }.With(
            Grow(1.0F),
            Padding(16.0F),
            Spacing(10.0F),
            MainAlign(MainAxisAlignment::Center),
            CrossAlign(CrossAxisAlignment::Center)
        );
    } else if (results.empty()) {
        left_pane = Column {
            EmptyState(
                icons::StoreIcon(),
                has_searched ? "未找到匹配的软件包" : "探索 xlings 软件库",
                has_searched ? "请尝试更短的关键词或更换类别过滤" : "输入关键词或点击上方热门标签检索可安装的工具链与库"
            ),
            ActionButton(
                "浏览全部软件包",
                ButtonTone::Secondary,
                [store, tasks, query_input, selected_category, list_scroll]() mutable {
                    query_input = TextEditingValue::FromText("");
                    selected_category = std::string{};
                    (void)list_scroll.ScrollTo(0.0F);
                    store->RunSearch(tasks, "", "");
                },
                true,
                icons::StoreIcon(),
                true
            ).Key("store-browse-all"),
        }.With(
            Grow(1.0F),
            Padding(16.0F),
            Spacing(6.0F),
            MainAlign(MainAxisAlignment::Center),
            CrossAlign(CrossAxisAlignment::Center)
        );
    } else {
        auto results_snapshot = std::make_shared<std::vector<SearchPkg>>(results);
        auto installed_lookup = detail::BuildInstalledBadgeLookup(installed_groups);
        const Color col_primary = theme.colors.primary;
        const Color col_primary_container = theme.colors.primary_container;
        const Color col_on_surface = theme.colors.on_surface;
        const Color col_on_surface_variant = theme.colors.on_surface_variant;

        View results_header = Row {
            Text("共 " + std::to_string(results.size()) + " 个软件包", TextRole::Label)
                .Style(TextStyle{.font = Font::System(12.0F), .foreground = col_on_surface_variant})
                .With(Grow(1.0F)),
            !cur_cat.empty()
                ? StatusBadge(cur_cat, sp.info, sp.info_bg)
                : View{},
        }.With(
            Padding(EdgeInsets{.left = 14.0F, .top = 12.0F, .right = 14.0F, .bottom = 6.0F}),
            Spacing(6.0F),
            CrossAlign(CrossAxisAlignment::Center)
        ).Key("store-results-header");

        View results_list = VirtualList(
            results_snapshot->size(),
            [store, tasks, chosen_version, chosen_version_pkg, results_snapshot, installed_lookup,
             selected_pkg, col_primary, col_primary_container, col_on_surface, col_on_surface_variant,
             sp](std::size_t i) -> View {
                if (i >= results_snapshot->size()) {
                    return View{};
                }
                const auto& pkg = (*results_snapshot)[i];
                const bool is_sel = pkg.package == selected_pkg;
                const std::optional<std::string> installed_badge = installed_lookup->Find(pkg);

                return Column {
                    Row {
                        Text(pkg.name, TextRole::Body)
                            .Style(TextStyle{
                                .font = Font::System(14.0F).WithWeight(FontWeight::SemiBold),
                                .foreground = is_sel ? col_primary : col_on_surface,
                            })
                            .With(Grow(1.0F)),
                        installed_badge.has_value()
                            ? StatusBadge(*installed_badge, sp.success, sp.success_bg)
                            : View{},
                    }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Row {
                        StatusBadge(pkg.package, col_on_surface_variant, sp.muted_bg),
                        pkg.version.empty()
                            ? View{}
                            : StatusBadge("v" + pkg.version, sp.info, sp.info_bg),
                        pkg.status.empty()
                            ? View{}
                            : StatusBadge(
                                  pkg.status,
                                  pkg.status == "stable" ? sp.success : sp.warning,
                                  pkg.status == "stable" ? sp.success_bg : sp.warning_bg
                              ),
                    }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                    pkg.description.empty()
                        ? View{}
                        : Text(pkg.description, TextRole::Label)
                              .Style(TextStyle{.font = Font::System(12.0F), .foreground = col_on_surface_variant}),
                }.With(
                    Padding(12.0F),
                    Spacing(6.0F),
                    Background(is_sel ? col_primary_container : sp.card_bg),
                    Border{is_sel ? col_primary : sp.border_soft, is_sel ? 1.5F : 1.0F},
                    CornerRadius(10.0F),
                    Focusable(true),
                    PointerCursor(PointerCursorKind::Hand),
                    CrossAlign(CrossAxisAlignment::Stretch)
                )
                .OnClick([store, tasks, chosen_version, chosen_version_pkg, pkg_id = pkg.package]() mutable {
                    chosen_version = std::string{};
                    chosen_version_pkg = pkg_id;
                    store->LoadStoreDetail(tasks, pkg_id);
                })
                .Key("search-" + pkg.package + "-" + std::to_string(i));
            }
        )
            .ScrollAxis(Axis::Vertical)
            .EstimatedItemExtent(96.0F)
            .CacheExtent(280.0F)
            .Controller(list_scroll)
            .With(
                Grow(1.0F),
                Padding(EdgeInsets{.left = 14.0F, .top = 4.0F, .right = 14.0F, .bottom = 14.0F}),
                Spacing(8.0F),
                CrossAlign(CrossAxisAlignment::Stretch),
                ScrollBar{}
            )
            .Key("store-results-list");

        left_pane = Column {
            std::move(results_header),
            std::move(results_list),
        }.With(Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch));
    }

    View right_pane = View{};
    if (!selected_pkg.empty() && loading_detail && !detail_opt.has_value()) {
        right_pane = Column {
            InlineSpinner(28.0F, theme.colors.primary),
            Text("正在加载 " + selected_pkg + " 详情...", TextRole::Body)
                .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
        }.With(
            Grow(1.0F),
            Spacing(12.0F),
            MainAlign(MainAxisAlignment::Center),
            CrossAlign(CrossAxisAlignment::Center)
        );
    } else if (selected_pkg.empty() || !detail_opt.has_value()) {
        right_pane = EmptyState(
            icons::StoreIcon(),
            "选择一个搜索结果查看详情",
            "可选择目标版本、目标 SubOS、预览依赖安装计划并一键安装"
        );
    } else {
        const PackageDetail& d = *detail_opt;
        std::vector<std::string> ver_options;
        ver_options.reserve(d.available_versions.size() + 1);
        for (const auto& av : d.available_versions) {
            if (!av.empty() && std::ranges::find(ver_options, av) == ver_options.end()) {
                ver_options.push_back(av);
            }
        }
        if (ver_options.empty()) {
            ver_options.push_back(d.selected_version.empty() ? std::string("latest") : d.selected_version);
        }

        const std::string cur_ver_choice =
            (chosen_version_pkg.Get() == selected_pkg) ? chosen_version.Get() : std::string{};
        std::size_t ver_idx = 0;
        const std::string effective_ver = cur_ver_choice.empty() ? d.selected_version : cur_ver_choice;
        for (std::size_t i = 0; i < ver_options.size(); ++i) {
            if (ver_options[i] == effective_ver) {
                ver_idx = i;
                break;
            }
        }

        std::vector<std::string> subos_options{"(当前活跃 SubOS)"};
        std::vector<std::string> subos_values{""};
        for (const auto& s : suboses) {
            if (s.name.empty() || std::ranges::find(subos_values, s.name) != subos_values.end()) {
                continue;
            }
            subos_options.push_back(s.name + (s.active ? " (当前)" : ""));
            subos_values.push_back(s.name);
        }
        std::size_t subos_idx = 0;
        const std::string cur_subos_val = chosen_subos.Get();
        for (std::size_t i = 0; i < subos_values.size(); ++i) {
            if (subos_values[i] == cur_subos_val) {
                subos_idx = i;
                break;
            }
        }

        const std::string pkg_base = d.package.empty() ? selected_pkg : d.package;
        const std::string selected_ver_str = ver_options[ver_idx];
        const std::string install_target =
            (selected_ver_str.empty() || selected_ver_str == "latest")
                ? pkg_base
                : (pkg_base + "@" + selected_ver_str);

        std::vector<View> kv_rows;
        for (std::size_t i = 0; i < d.fields.size(); ++i) {
            const auto& f = d.fields[i];
            kv_rows.push_back(
                KeyValueRow(f.label, f.value, f.highlight, f.label == "runtime deps" || f.label == "build deps")
                    .Key("store-kv-" + std::to_string(i))
            );
        }

        std::vector<View> detail_cards;

        detail_cards.push_back(
            Card(
                Column {
                    Row {
                        Column {
                            Row {
                                Text(d.name.empty() ? pkg_base : d.name, TextRole::Title)
                                    .Style(TextStyle{
                                        .font = Font::System(20.0F).WithWeight(FontWeight::Bold),
                                        .foreground = theme.colors.on_surface,
                                    }),
                                StatusBadge(pkg_base, theme.colors.primary, theme.colors.primary_container),
                                d.status.empty()
                                    ? View{}
                                    : StatusBadge(
                                          d.status,
                                          d.status == "stable" ? sp.success : sp.warning,
                                          d.status == "stable" ? sp.success_bg : sp.warning_bg
                                      ),
                            }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                            Text(d.description.empty() ? std::string("暂无描述") : d.description, TextRole::Body)
                                .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
                        }.With(Spacing(6.0F), Grow(1.0F)),
                        LoadingIndicatorSlot(loading_detail, 16.0F),
                    }.With(Spacing(10.0F)),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            )
        );

        detail_cards.push_back(
            Card(
                Column {
                    SectionHeader("安装选项", "目标坐标: " + install_target),
                    Row {
                        Select(ver_options, ver_idx, [](const std::string& v) { return Text(v); })
                            .Label("选择版本")
                            .OnChanged([chosen_version, chosen_version_pkg, ver_options, selected_pkg](std::size_t idx) mutable {
                                if (idx < ver_options.size()) {
                                    chosen_version = ver_options[idx];
                                    chosen_version_pkg = selected_pkg;
                                }
                            })
                            .With(Grow(1.0F)),
                        Select(subos_options, subos_idx, [](const std::string& s) { return Text(s); })
                            .Label("目标 SubOS")
                            .OnChanged([chosen_subos, subos_values](std::size_t idx) mutable {
                                if (idx < subos_values.size()) chosen_subos = subos_values[idx];
                            })
                            .With(Grow(1.0F)),
                    }.With(Spacing(12.0F)),
                    Flow {
                        Checkbox("安装后立即激活 (--use)", opt_activate)
                            .OnChanged([opt_activate](bool checked) mutable { opt_activate = checked; }),
                        Checkbox("全局作用域 (--global)", opt_global)
                            .OnChanged([opt_global](bool checked) mutable { opt_global = checked; }),
                        Checkbox("重新配置 (--reconfig)", opt_reconfig)
                            .OnChanged([opt_reconfig](bool checked) mutable { opt_reconfig = checked; }),
                    }.With(Spacing(16.0F)),
                    Row {
                        ActionButton(
                            "预览依赖计划",
                            ButtonTone::Secondary,
                            [store, tasks, install_target, opt_global] {
                                store->PreviewInstallPlan(tasks, {install_target}, opt_global.Get(), true);
                            },
                            true,
                            std::nullopt,
                            false
                        ),
                        ActionButton(
                            "安装 " + install_target,
                            ButtonTone::Primary,
                            [store, tasks, toast, install_target, opt_global, opt_activate, cur_subos_val, opt_reconfig] {
                                store->InstallPackageAsync(
                                    tasks,
                                    toast,
                                    install_target,
                                    opt_global.Get(),
                                    opt_activate.Get(),
                                    cur_subos_val,
                                    opt_reconfig.Get()
                                );
                            },
                            true,
                            icons::DownloadIcon(),
                            false
                        ),
                    }.With(Spacing(10.0F)),
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
                            item.action_code > 0 ? std::string("将安装") : std::string("已安装"),
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
                        .Key("store-plan-" + item.spec + "-" + std::to_string(i))
                );
            }
            detail_cards.push_back(
                Card(
                    Column {
                        SectionHeader("依赖解析计划 (" + std::to_string(plan_items.size()) + ")", "来自 xlings plan_install"),
                        Column(std::move(plan_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
                )
            );
        }

        if (!kv_rows.empty()) {
            detail_cards.push_back(
                Card(
                    Column {
                        SectionHeader("软件包信息", "来自 xlings package_info"),
                        Column(std::move(kv_rows)).With(Spacing(4.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
                )
            );
        }

        right_pane = ScrollView(
            Column(std::move(detail_cards))
                .With(Padding(18.0F), Spacing(14.0F), CrossAlign(CrossAxisAlignment::Stretch))
        ).ScrollAxis(Axis::Vertical)
            .With(Grow(1.0F), ScrollBar{});
    }

    return Column {
        std::move(top_bar),
        Divider(),
        ResizableSplit(std::move(left_pane), std::move(right_pane), 340.0F, 250.0F, 520.0F),
    }.With(Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch));
}

} // namespace hxlings

export module app;

import std;
import mcpplibs.os;
import huxerui;
import hxlings.theme;
import hxlings.icons;
import hxlings.shortcuts;
import hxlings.store;
import hxlings.components;
import hxlings.console;
import hxlings.page.dashboard;
import hxlings.page.packages;
import hxlings.page.store;
import hxlings.page.subos;
import hxlings.page.indexes;
import hxlings.page.settings;

using namespace huxerui;
using namespace hxlings;

namespace {

void InstallApplication(ApplicationContext& context) {
    auto store = std::make_shared<XlingsStore>();
    context.Provide(std::move(store));
}

[[huxerui::composable]]
View NavItemButton(
    std::string label,
    VectorAsset icon,
    bool active,
    std::function<void()> on_click,
    View trailing = {}
) {
    const ThemeSpec& theme = UseTheme();
    const bool dark = theme.brightness == ThemeBrightness::Dark;
    const Color fg = active ? theme.colors.primary : theme.colors.on_surface_variant;
    const Color bg = active ? theme.colors.primary_container : Color::Transparent();
    const Color hover = dark ? Color::Rgb(255, 255, 255, 0.06F) : Color::Rgb(15, 23, 42, 0.05F);
    const Color press = dark ? Color::Rgb(255, 255, 255, 0.12F) : Color::Rgb(15, 23, 42, 0.10F);

    return Row {
        Image(std::move(icon))
            .Fit(ImageFit::Contain)
            .Align(HorizontalAlignment::Center, VerticalAlignment::Center)
            .Tint(fg)
            .With(Frame{.width = 18.0F, .height = 18.0F}),
        Text(label, TextRole::Body)
            .Style(TextStyle{
                .font = Font::System(13.5F).WithWeight(active ? FontWeight::SemiBold : FontWeight::Medium),
                .foreground = active ? theme.colors.on_surface : fg,
            })
            .With(Grow(1.0F)),
        std::move(trailing),
    }.With(
        Padding(EdgeInsets::Symmetric(12.0F, 10.0F)),
        Spacing(10.0F),
        Background(bg),
        CornerRadius(9.0F),
        Indication{
            .hover = IndicationLayer{.fill = hover},
            .press = IndicationLayer{.fill = press},
        },
        Focusable(true),
        PointerCursor(PointerCursorKind::Hand),
        CrossAlign(CrossAxisAlignment::Center),
        Semantics{.role = SemanticRole::Button, .label = label, .selected = active}
    ).OnClick(std::move(on_click));
}

[[huxerui::composable]]
View TitleIconAction(
    VectorAsset icon,
    std::string tooltip,
    std::function<void()> on_click,
    bool enabled = true,
    std::string semantic_label = "Action"
) {
    const ThemeSpec& theme = UseTheme();
    const Color tint = enabled ? theme.colors.on_surface_variant : WithAlpha(theme.colors.on_surface_variant, 0.4F);
    return Row {
        Image(std::move(icon))
            .Fit(ImageFit::Contain)
            .Align(HorizontalAlignment::Center, VerticalAlignment::Center)
            .Tint(tint)
            .With(Frame{.width = 16.0F, .height = 16.0F}),
    }.With(
        Padding(6.0F),
        CornerRadius(8.0F),
        Tooltip(std::move(tooltip)),
        Focusable(enabled),
        Enabled(enabled),
        PointerCursor(enabled ? PointerCursorKind::Hand : PointerCursorKind::Default),
        Semantics{.role = SemanticRole::Button, .label = std::move(semantic_label)}
    ).OnClick(std::move(on_click));
}

[[huxerui::composable]]
View ShortcutsDialogContent(DialogContext ctx) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const auto catalog = AppShortcutCatalog();

    std::vector<View> rows;
    rows.reserve(catalog.size() + 4);
    std::string last_group;
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        const auto& item = catalog[i];
        if (item.group != last_group) {
            last_group = item.group;
            rows.push_back(
                Text(item.group, TextRole::Label)
                    .Style(TextStyle{
                        .font = Font::System(12.0F).WithWeight(FontWeight::SemiBold),
                        .foreground = theme.colors.primary,
                    })
                    .With(Padding(EdgeInsets{.top = rows.empty() ? 0.0F : 6.0F}))
                    .Key("sc-grp-" + std::to_string(i))
            );
        }
        rows.push_back(
            Row {
                Text(item.description, TextRole::Body)
                    .Style(TextStyle{
                        .font = Font::System(13.0F),
                        .foreground = theme.colors.on_surface,
                    })
                    .With(Grow(1.0F)),
                Row {
                    Text(item.keys, TextRole::Label)
                        .Style(TextStyle{
                            .font = Font::Monospace(12.0F).WithWeight(FontWeight::SemiBold),
                            .foreground = theme.colors.on_surface,
                        }),
                }.With(
                    Padding(EdgeInsets::Symmetric(8.0F, 3.0F)),
                    Background(sp.muted_bg),
                    Border{sp.border_soft, 1.0F},
                    CornerRadius(6.0F)
                ),
            }.With(
                Padding(EdgeInsets::Symmetric(8.0F, 5.0F)),
                Spacing(10.0F),
                Background(sp.card_elevated_bg),
                CornerRadius(8.0F),
                CrossAlign(CrossAxisAlignment::Center)
            ).Key("sc-row-" + std::to_string(i))
        );
    }

    View body = Column {
        Text(
            kIsMacOs ? "当前运行于 macOS（主修饰键为 ⌘ Command）"
                     : "当前主修饰键为 Ctrl（macOS 自动映射为 ⌘ Command）",
            TextRole::Label
        ).Style(TextStyle{
            .font = Font::System(12.0F),
            .foreground = theme.colors.on_surface_variant,
        }),
        ScrollView(
            Column(std::move(rows))
                .With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))
        ).ScrollAxis(Axis::Vertical)
            .With(Frame{.max_height = 320.0F}, ScrollBar{}),
    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch));

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("关闭", ButtonTone::Primary, [ctx] { ctx.Dismiss(); }),
    }.With(CrossAlign(CrossAxisAlignment::Center));

    return DialogCardShell("快捷键一览", std::move(body), std::move(actions), 540.0F).Key("shortcuts-dialog");
}

[[huxerui::composable]]
View MainShell() {
    const auto store = UseService<XlingsStore>();
    const TaskScope tasks = UseApplicationTaskScope();
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const WindowHandle window = UseWindow();
    const DialogHandle dialog = UseDialog();

    Lifecycle([store, tasks] {
        if (!store->skip_initial_refresh) {
            store->RefreshAll(tasks);
        }
    });

    constexpr KeyModifiers primary = PrimaryModifier();
    constexpr KeyModifiers primary_shift = PrimaryShiftModifier();
    window.OnShortcut(Key::Digit1, primary, [store] { store->active_nav = 0; return true; });
    window.OnShortcut(Key::Digit2, primary, [store] { store->active_nav = 1; return true; });
    window.OnShortcut(Key::Digit3, primary, [store] { store->active_nav = 2; return true; });
    window.OnShortcut(Key::Digit4, primary, [store] { store->active_nav = 3; return true; });
    window.OnShortcut(Key::Digit5, primary, [store] { store->active_nav = 4; return true; });
    window.OnShortcut(Key::Digit6, primary, [store] { store->active_nav = 5; return true; });
    window.OnShortcut(Key::Comma, primary, [store] { store->active_nav = 5; return true; });
    window.OnShortcut(Key::K, primary, [store] {
        store->active_nav = 2;
        store->RequestFocusSearch();
        return true;
    });
    window.OnShortcut(Key::F, primary, [store] {
        const std::size_t nav = store->active_nav.Get();
        if (nav != 1 && nav != 2 && nav != 3) {
            store->active_nav = 2;
        }
        store->RequestFocusSearch();
        return true;
    });
    window.OnShortcut(Key::R, primary, [store, tasks] {
        if (!store->refreshing.Get()) store->RefreshAll(tasks);
        return true;
    });
    window.OnShortcut(Key::F5, KeyModifiers{}, [store, tasks] {
        if (!store->refreshing.Get()) store->RefreshAll(tasks);
        return true;
    });
    window.OnShortcut(Key::J, primary, [store] {
        store->console_expanded = !store->console_expanded.Get();
        return true;
    });
    window.OnShortcut(Key::D, primary_shift, [store] {
        store->dark_mode = !store->dark_mode.Get();
        return true;
    });
    window.OnShortcut(Key::U, primary_shift, [store] {
        store->RequestOpenBatchUpdate();
        return true;
    });
    window.OnShortcut(Key::P, primary_shift, [store] {
        store->RequestOpenBatchCleanup();
        return true;
    });
    window.OnShortcut(Key::Slash, primary, [dialog] {
        dialog.Show([](DialogContext ctx) { return ShortcutsDialogContent(ctx); });
        return true;
    });
    window.OnShortcut(Key::F1, KeyModifiers{}, [dialog] {
        dialog.Show([](DialogContext ctx) { return ShortcutsDialogContent(ctx); });
        return true;
    });
    if constexpr (kIsMacOs) {
        window.OnShortcut(Key::W, primary, [window] { window.Close(); return true; });
        window.OnShortcut(Key::Q, primary, [window] { window.Close(); return true; });
        window.OnShortcut(Key::M, primary, [window] { window.Minimize(); return true; });
        window.OnShortcut(Key::F, KeyModifiers{.control = true, .meta = true}, [window] {
            window.SetFullscreen(!window.IsFullscreen());
            return true;
        });
    }

    const std::size_t nav_idx = store->active_nav.Get();
    const bool refreshing = store->refreshing.Get();
    const bool task_running = store->task_running.Get();
    const bool dark = store->dark_mode.Get();
    const auto& env = store->env.Get();
    const auto& ver = store->version_info.Get();
    const auto& groups = store->installed_groups.Get();
    const auto& suboses = store->subos_list.Get();
    const auto& findings = store->system_status.Get().findings;

    const std::string active_subos = env.active_subos.empty() ? std::string("default") : env.active_subos;

    View title_bar = WindowTitleBar {
        Row {
            Image(icons::LogoIcon())
                .Fit(ImageFit::Contain)
                .Tint(theme.colors.primary)
                .With(Frame{.width = 18.0F, .height = 18.0F}),
        }.With(
            Padding(4.0F),
            Background(theme.colors.primary_container),
            CornerRadius(6.0F)
        ),
        Text("Hxlings", TextRole::Title)
            .Style(TextStyle{
                .font = Font::System(15.0F).WithWeight(FontWeight::Bold),
                .foreground = theme.colors.on_surface,
            }),
        Text("xlings 管理器", TextRole::Label)
            .Style(TextStyle{
                .font = Font::System(12.0F),
                .foreground = theme.colors.on_surface_variant,
            }),
        Row {
            StatusBadge("SubOS: " + active_subos, sp.info, sp.info_bg),
        }.With(PointerCursor(PointerCursorKind::Hand))
            .OnClick([store] { store->active_nav = 3; }),
        env.mirror.empty()
            ? View{}
            : StatusBadge(env.mirror, theme.colors.on_surface_variant, sp.muted_bg),
        Spacer{}.With(Grow(1.0F)),
        (refreshing || task_running)
            ? View(ProgressCircle().With(Frame{.width = 15.0F, .height = 15.0F}))
            : View{},
        TitleIconAction(
            icons::KeyboardIcon(),
            "快捷键一览 (" + FormatShortcut("/") + ")",
            [dialog] {
                dialog.Show([](DialogContext ctx) { return ShortcutsDialogContent(ctx); });
            },
            true,
            "快捷键"
        ),
        TitleIconAction(
            icons::RefreshIcon(),
            "刷新全部 xlings 状态 (" + FormatShortcut("R") + ")",
            [store, tasks] { store->RefreshAll(tasks); },
            !refreshing,
            "刷新"
        ),
        TitleIconAction(
            dark ? icons::SunIcon() : icons::MoonIcon(),
            (dark ? "切换到浅色主题 (" : "切换到深色主题 (") + FormatShortcut("D", true) + ")",
            [store, dark] { store->dark_mode = !dark; },
            true,
            "切换主题"
        ),
    }.With(
        Padding(kIsMacOs ? EdgeInsets{.left = 8.0F, .right = 14.0F} : EdgeInsets{.left = 14.0F, .right = 8.0F}),
        Spacing(10.0F),
        Background(theme.colors.surface)
    );

    View sidebar = Column {
        Text("工作区", TextRole::Label)
            .Style(TextStyle{
                .font = Font::System(11.5F).WithWeight(FontWeight::SemiBold),
                .foreground = theme.colors.on_surface_variant,
            })
            .With(Padding(EdgeInsets{.left = 10.0F, .bottom = 2.0F})),
        NavItemButton(
            "概览总览",
            icons::DashboardIcon(),
            nav_idx == 0,
            [store] { store->active_nav = 0; },
            findings.empty()
                ? View{}
                : StatusBadge(std::to_string(findings.size()), sp.danger, sp.danger_bg)
        ).With(Tooltip("概览总览 (" + FormatShortcut("1") + ")")).Key("nav-dashboard"),
        NavItemButton(
            "已安装管理",
            icons::PackagesIcon(),
            nav_idx == 1,
            [store] { store->active_nav = 1; },
            groups.empty()
                ? View{}
                : StatusBadge(std::to_string(groups.size()), theme.colors.on_surface_variant, sp.muted_bg)
        ).With(Tooltip("已安装管理 (" + FormatShortcut("2") + ")")).Key("nav-packages"),
        NavItemButton(
            "探索与安装",
            icons::StoreIcon(),
            nav_idx == 2,
            [store] { store->active_nav = 2; }
        ).With(Tooltip("探索与安装 (" + FormatShortcut("3") + ")")).Key("nav-store"),
        NavItemButton(
            "SubOS 环境",
            icons::SubosIcon(),
            nav_idx == 3,
            [store] { store->active_nav = 3; },
            suboses.empty()
                ? View{}
                : StatusBadge(std::to_string(suboses.size()), theme.colors.on_surface_variant, sp.muted_bg)
        ).With(Tooltip("SubOS 环境 (" + FormatShortcut("4") + ")")).Key("nav-subos"),
        NavItemButton(
            "索引与配方",
            icons::IndexIcon(),
            nav_idx == 4,
            [store] { store->active_nav = 4; }
        ).With(Tooltip("索引与配方 (" + FormatShortcut("5") + ")")).Key("nav-indexes"),
        NavItemButton(
            "设置与诊断",
            icons::SettingsIcon(),
            nav_idx == 5,
            [store] { store->active_nav = 5; }
        ).With(Tooltip("设置与诊断 (" + FormatShortcut("6") + ")")).Key("nav-settings"),
        Spacer{}.With(Grow(1.0F)),
        Divider(),
        Column {
            Text(ver.version.empty() ? std::string("xlings CLI") : ("xlings " + ver.version), TextRole::Label)
                .Style(TextStyle{.font = Font::Monospace(11.5F).WithWeight(FontWeight::SemiBold),
                                 .foreground = theme.colors.on_surface}),
            Text("Hxlings v0.1.0 · HuxerUI", TextRole::Label)
                .Style(TextStyle{.font = Font::System(11.0F),
                                 .foreground = theme.colors.on_surface_variant}),
        }.With(Padding(EdgeInsets::Symmetric(8.0F, 4.0F)), Spacing(3.0F)),
    }.With(
        Frame{.width = 212.0F},
        Padding(12.0F),
        Spacing(6.0F),
        Background(sp.sidebar_bg),
        CrossAlign(CrossAxisAlignment::Stretch)
    );

    View pages = IndexedPages({
        DashboardPage().Key("page-dashboard"),
        PackagesPage().Key("page-packages"),
        StorePage().Key("page-store"),
        SubosPage().Key("page-subos"),
        IndexesPage().Key("page-indexes"),
        SettingsPage().Key("page-settings"),
    }, nav_idx).With(Grow(1.0F));

    View workspace_area = Column {
        std::move(pages),
        Divider(),
        TaskConsole(),
    }.With(Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch));

    View main_row = Row {
        std::move(sidebar),
        Divider(Axis::Vertical),
        std::move(workspace_area),
    }.With(Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch));

    return Column {
        std::move(title_bar),
        Divider(),
        std::move(main_row),
    }.With(Background(theme.colors.background), CrossAlign(CrossAxisAlignment::Stretch));
}

[[huxerui::composable]]
View AppRoot() {
    const auto store = UseService<XlingsStore>();
    const bool dark = store->dark_mode.Get();
    const Color brand = store->brand_color.Get();

    ThemeSpec spec = AppThemeSpec(dark, brand);
    ThemeDefinition definition = FlatThemeDefinition(std::move(spec));

    return Theme(std::move(definition), MainShell());
}

} // namespace

export View App() {
    return AppRoot();
}

const Application application{
    App,
    {
        .window = {
            .title = "Hxlings — xlings Manager",
            .initial_size = {1180.0F, 760.0F},
            .minimum_size = Size{880.0F, 560.0F},
            .chrome_mode = WindowChromeMode::Custom,
            .title_bar_height = 44.0F,
        },
        .show_debug_overlay = false,
        .application_hooks = {InstallApplication},
    },
};

export module app;

import std;
import huxerui;
import hxlings.theme;
import hxlings.icons;
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
View TitleIconAction(VectorAsset icon, std::string tooltip, std::function<void()> on_click, bool enabled = true) {
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
        Semantics{.role = SemanticRole::Button, .label = "Action"}
    ).OnClick(std::move(on_click));
}

[[huxerui::composable]]
View MainShell() {
    const auto store = UseService<XlingsStore>();
    const TaskScope tasks = UseApplicationTaskScope();
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    Lifecycle([store, tasks] {
        store->RefreshAll(tasks);
    });

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
            icons::RefreshIcon(),
            "刷新全部 xlings 状态",
            [store, tasks] { store->RefreshAll(tasks); },
            !refreshing
        ),
        TitleIconAction(
            dark ? icons::SunIcon() : icons::MoonIcon(),
            dark ? "切换到浅色主题" : "切换到深色主题",
            [store, dark] { store->dark_mode = !dark; }
        ),
    }.With(
        Padding(EdgeInsets{.left = 14.0F}),
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
        ).Key("nav-dashboard"),
        NavItemButton(
            "已安装管理",
            icons::PackagesIcon(),
            nav_idx == 1,
            [store] { store->active_nav = 1; },
            groups.empty()
                ? View{}
                : StatusBadge(std::to_string(groups.size()), theme.colors.on_surface_variant, sp.muted_bg)
        ).Key("nav-packages"),
        NavItemButton(
            "探索与安装",
            icons::StoreIcon(),
            nav_idx == 2,
            [store] { store->active_nav = 2; }
        ).Key("nav-store"),
        NavItemButton(
            "SubOS 环境",
            icons::SubosIcon(),
            nav_idx == 3,
            [store] { store->active_nav = 3; },
            suboses.empty()
                ? View{}
                : StatusBadge(std::to_string(suboses.size()), theme.colors.on_surface_variant, sp.muted_bg)
        ).Key("nav-subos"),
        NavItemButton(
            "索引与配方",
            icons::IndexIcon(),
            nav_idx == 4,
            [store] { store->active_nav = 4; }
        ).Key("nav-indexes"),
        NavItemButton(
            "设置与诊断",
            icons::SettingsIcon(),
            nav_idx == 5,
            [store] { store->active_nav = 5; }
        ).Key("nav-settings"),
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

View App() {
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

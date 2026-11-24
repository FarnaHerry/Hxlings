export module app;

import std;
import mcpplibs.os;
import huxerui;
import app.resources;
import hxlings.theme;
import hxlings.icons;
import hxlings.shortcuts;
import hxlings.settings;
import hxlings.platform_icon;
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

export View App();

namespace {

WindowOptions MainWindowOptions() {
    return {
        .title = "Hxlings — xlings Manager",
        .initial_size = {1180.0F, 760.0F},
        .minimum_size = Size{880.0F, 560.0F},
        .chrome_mode = WindowChromeMode::Custom,
        .title_bar_height = 44.0F,
    };
}

void InstallApplication(ApplicationContext& context) {
    auto store = std::make_shared<XlingsStore>();
    auto sync_native_theme = [](ThemeMode mode, bool eff_dark) {
        const int kind = (mode == ThemeMode::Auto) ? 0 : (mode == ThemeMode::Light ? 1 : 2);
        ApplyPlatformThemeMode(kind, eff_dark);
    };
    store->on_theme_changed = sync_native_theme;
    store->InitAppSettings(LoadAppSettings());
    ApplyPlatformApplicationIcon();

    const ApplicationHandle application = UseApplication();
    const SystemTrayHandle tray = application.SystemTray();
    const TaskScope app_tasks = UseApplicationTaskScope();
    std::weak_ptr<XlingsStore> weak_store = store;

    const auto ensure_main_window = [weak_store, application] {
        const auto s = weak_store.lock();
        if (!s) return;
        if (s->main_window.has_value() && s->main_window->IsOpen()) {
            s->main_window->Show();
            s->main_window->Restore();
            s->main_window->Activate();
            return;
        }
        if (application.SupportsMultipleWindows()) {
            s->main_window = application.OpenWindow(App, MainWindowOptions());
            ApplyPlatformWindowIcon();
        }
    };

    if (!store->tray_activate_registered) {
        store->tray_activate_registered = true;
        tray.OnActivate(ensure_main_window);
    }

    store->on_tray_state_changed = [weak_store, application, tray, app_tasks, ensure_main_window] {
        const auto s = weak_store.lock();
        if (!s || s->skip_initial_refresh) return;
        const auto cfg = s->app_settings.Get();
        if (!cfg.tray_enabled) {
            tray.Hide();
            return;
        }
        const std::string subos_env = s->env.Get().active_subos;
        const std::string active_subos = subos_env.empty() ? std::string("default") : subos_env;
        const bool busy = s->task_running.Get();
        const std::string tooltip = "Hxlings — SubOS: " + active_subos +
                                    (busy ? " · 任务执行中" : "");
        std::vector<MenuEntry> menu{
            MenuItem("显示主窗口", ensure_main_window),
            MenuSection{},
            MenuItem("概览总览", [weak_store, ensure_main_window] {
                if (auto st = weak_store.lock()) st->active_nav = 0;
                ensure_main_window();
            }),
            MenuItem("已安装管理", [weak_store, ensure_main_window] {
                if (auto st = weak_store.lock()) st->active_nav = 1;
                ensure_main_window();
            }),
            MenuItem("探索与安装", [weak_store, ensure_main_window] {
                if (auto st = weak_store.lock()) st->active_nav = 2;
                ensure_main_window();
            }),
            MenuItem("SubOS 环境 (" + active_subos + ")", [weak_store, ensure_main_window] {
                if (auto st = weak_store.lock()) st->active_nav = 3;
                ensure_main_window();
            }),
            MenuItem("索引与配方", [weak_store, ensure_main_window] {
                if (auto st = weak_store.lock()) st->active_nav = 4;
                ensure_main_window();
            }),
            MenuItem("设置与诊断", [weak_store, ensure_main_window] {
                if (auto st = weak_store.lock()) st->active_nav = 5;
                ensure_main_window();
            }),
            MenuSection{},
            MenuItem("刷新状态", [weak_store, app_tasks] {
                if (auto st = weak_store.lock()) st->RefreshCore(app_tasks, true);
            }),
            MenuItem("批量检查更新", [weak_store, ensure_main_window] {
                if (auto st = weak_store.lock()) st->RequestOpenBatchUpdate();
                ensure_main_window();
            }),
            MenuSection{},
            MenuItem("退出 Hxlings", [weak_store, application] {
                if (auto st = weak_store.lock()) st->force_quit_requested = true;
                application.Quit();
            }),
        };
        tray.Show(app::images::tray_icon, {.tooltip = tooltip, .menu = std::move(menu)});
    };
    store->NotifyTrayStateChanged();

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
    const StatusPalette sp = ResolveStatusPalette(theme);
    const Color fg = active ? theme.colors.primary : theme.colors.on_surface_variant;
    const Color bg = active ? theme.colors.primary_container : Color::Transparent();

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
            .hover = IndicationLayer{.fill = sp.hover_subtle},
            .press = IndicationLayer{.fill = sp.press_subtle},
        },
        Focusable(true),
        PointerCursor(PointerCursorKind::Hand),
        CrossAlign(CrossAxisAlignment::Center),
        Semantics{.role = SemanticRole::Button, .label = label, .selected = active}
    ).OnClick(std::move(on_click));
}

[[huxerui::composable]]
View TitleIconAction(
    ImageVariant icon,
    std::string tooltip,
    std::function<void()> on_click,
    bool enabled = true,
    std::string semantic_label = "Action"
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const Color tint = enabled ? theme.colors.on_surface_variant : sp.ghost_disabled_fg;
    return Row {
        Image(std::move(icon))
            .Fit(ImageFit::Contain)
            .Align(HorizontalAlignment::Center, VerticalAlignment::Center)
            .Tint(tint)
            .With(Frame{.width = 16.0F, .height = 16.0F}),
    }.With(
        Padding(6.0F),
        CornerRadius(8.0F),
        Indication{
            .hover = IndicationLayer{.fill = sp.hover_subtle},
            .press = IndicationLayer{.fill = sp.press_subtle},
        },
        Tooltip(std::move(tooltip)),
        Focusable(enabled),
        Enabled(enabled),
        PointerCursor(enabled ? PointerCursorKind::Hand : PointerCursorKind::Default),
        Semantics{.role = SemanticRole::Button, .label = std::move(semantic_label)}
    ).OnClick(std::move(on_click));
}

[[huxerui::composable]]
View MainShell(bool effective_dark) {
    const auto store = UseService<XlingsStore>();
    const TaskScope tasks = UseApplicationTaskScope();
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const ApplicationHandle application = UseApplication();
    const SystemTrayHandle tray = application.SystemTray();
    const WindowHandle window = UseWindow();
    const DialogHandle dialog = UseDialog();
    const ToastHandle toast = UseToast();
    const SceneTransitionHandle transition = UseSceneTransition();

    Lifecycle([store, window, toast] {
        store->main_window = window;
        store->active_toast = toast;
        return [store] {
            store->active_toast.reset();
            store->main_window.reset();
        };
    });

    Lifecycle([store, tasks] {
        if (!store->skip_initial_refresh) {
            ApplyPlatformWindowIcon();
            if (!store->initial_refresh_started) {
                store->initial_refresh_started = true;
                store->RefreshCore(tasks);
            }
        }
    });

    const ThemeMode cur_theme_mode = store->theme_mode.Get();
    const int cur_theme_kind = (cur_theme_mode == ThemeMode::Auto)
                                   ? 0
                                   : (cur_theme_mode == ThemeMode::Light ? 1 : 2);
    Lifecycle([store, cur_theme_kind, effective_dark] {
        if (!store->skip_initial_refresh) {
            ApplyPlatformThemeMode(cur_theme_kind, effective_dark);
        }
    }, cur_theme_kind, effective_dark);

    window.OnCloseRequest([store, application, window, tray] {
        if (store->force_quit_requested) return false;
        const auto cfg = store->app_settings.Get();
        const bool stay_in_tray = cfg.tray_enabled && tray.IsAvailable() &&
                                  (cfg.close_to_tray || cfg.lightweight_mode);
        if (stay_in_tray) {
            if (cfg.lightweight_mode && application.SupportsMultipleWindows()) {
                return false;
            }
            window.Hide();
            return true;
        }
        store->force_quit_requested = true;
        application.Quit();
        return true;
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
    window.OnShortcut(Key::D, primary_shift,
        [store, effective_dark, transition, duration = theme.motion.normal, reduced = theme.motion.reduced_motion] {
            auto mutation = [store, effective_dark] {
                store->ToggleEffectiveDark(effective_dark);
            };
            if (reduced) {
                mutation();
            } else {
                transition.Run(
                    TransitionSpec{FadeTransition{}, TweenSpec{.duration = duration, .easing = Easing::EaseInOut}},
                    std::move(mutation)
                );
            }
            return true;
        },
        effective_dark, theme.motion.normal, theme.motion.reduced_motion);
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
        window.OnShortcut(Key::Q, primary, [store, application] {
            store->force_quit_requested = true;
            application.Quit();
            return true;
        });
        window.OnShortcut(Key::M, primary, [window] { window.Minimize(); return true; });
        window.OnShortcut(Key::F, KeyModifiers{.control = true, .meta = true}, [window] {
            window.SetFullscreen(!window.IsFullscreen());
            return true;
        });
    }

    const std::size_t nav_idx = store->active_nav.Get();
    const bool refreshing = store->refreshing.Get();
    const bool task_running = store->task_running.Get();
    const auto& env = store->env.Get();
    const auto& groups = store->installed_groups.Get();
    const auto& suboses = store->subos_list.Get();
    const auto& findings = store->system_status.Get().findings;

    const std::string active_subos = env.active_subos.empty() ? std::string("default") : env.active_subos;

    View title_bar = WindowTitleBar {
        Image(app::images::hxlings)
            .Fit(ImageFit::Contain)
            .With(Frame{.width = 24.0F, .height = 24.0F}, CornerRadius(6.0F)),
        Text("Hxlings", TextRole::Title)
            .Style(TextStyle{
                .font = Font::System(15.0F).WithWeight(FontWeight::Bold),
                .foreground = theme.colors.on_surface,
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
            app::images::refresh,
            "刷新全部 xlings 状态 (" + FormatShortcut("R") + ")",
            [store, tasks] { store->RefreshAll(tasks); },
            !refreshing,
            "刷新"
        ),
    }.With(
        Padding(kIsMacOs ? EdgeInsets{.left = 8.0F, .right = 14.0F} : EdgeInsets{.left = 14.0F, .right = 8.0F}),
        Spacing(10.0F),
        Background(theme.colors.surface)
    );

    View sidebar = Column {
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
    const ThemeMode mode = store->theme_mode.Get();
    const Color brand = store->brand_color.Get();
    const bool dark = (mode == ThemeMode::Auto)
        ? (UseSystemThemeBrightness() == ThemeBrightness::Dark)
        : (mode == ThemeMode::Dark);

    ThemeSpec spec = AppThemeSpec(dark, brand);
    ThemeDefinition definition = FlatThemeDefinition(std::move(spec));

    return Theme(std::move(definition), MainShell(dark));
}

} // namespace

export View App() {
    return AppRoot();
}

const Application application{
    App,
    {
        .window = MainWindowOptions(),
        .show_debug_overlay = false,
        .quit_on_last_window_closed = false,
        .application_hooks = {InstallApplication},
    },
};

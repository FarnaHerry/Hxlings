export module hxlings.page.settings;

import std;
import mcpplibs.os;
import huxerui;
import app.resources;
import xlings.types;
import hxlings.theme;
import hxlings.icons;
import hxlings.shortcuts;
import hxlings.settings;
import hxlings.store;
import hxlings.components;

using namespace huxerui;

namespace hxlings::detail {

std::size_t MatchOptionIndex(std::string_view val, std::span<const std::string_view> opts, std::size_t fallback = 0) {
    for (std::size_t i = 0; i < opts.size(); ++i) {
        if (val.find(opts[i]) != std::string_view::npos) return i;
    }
    return fallback;
}

} // namespace hxlings::detail

export namespace hxlings {

[[huxerui::composable]]
View SettingsPage() {
    const auto store = UseService<XlingsStore>();
    const TaskScope tasks = UseApplicationTaskScope();
    const TaskScope ui_tasks = UseTaskScope();
    const ToastHandle toast = UseToast();
    const DialogHandle dialog = UseDialog();
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const SceneTransitionHandle transition = UseSceneTransition();
    const ThemeBrightness system_brightness = UseSystemThemeBrightness();
    const ApplicationHandle app = UseApplication();
    const auto clipboard = app.Clipboard();
    const SystemTrayHandle tray = app.SystemTray();
    const bool tray_available = tray.IsAvailable();

    struct ThemeAnimationFlag {
        bool animating = false;
    };
    auto theme_animating = UseState(std::make_shared<ThemeAnimationFlag>());
    auto commit_reason = UseState(TextEditingValue::FromText(""));

    const std::size_t tab = store->settings_tab.Get();
    const AppSettings app_cfg = store->app_settings.Get();
    const auto& cfg = store->config.Get();
    const auto& env = store->env.Get();
    const auto& ver = store->version_info.Get();
    const auto& profiles = store->profiles.Get();
    const bool loading_cfg = store->loading_xlings_config.Get();
    const bool loading_prof = store->loading_profiles.Get();
    const std::string& doctor_out = store->doctor_output.Get();
    const std::string& clean_out = store->clean_output.Get();

    Lifecycle([store, tasks, tab] {
        if (store->skip_initial_refresh || store->active_nav.Get() != 5) return;
        if (tab == 1) {
            store->RefreshXlingsConfig(tasks, false);
        } else if (tab == 2) {
            store->RefreshProfiles(tasks, false);
        }
    }, store->active_nav.Get(), tab);

    constexpr std::array<std::string_view, 2> kMirrors{"CN", "GLOBAL"};
    constexpr std::array<std::string_view, 3> kLangs{"auto", "zh", "en"};
    constexpr std::array<std::string_view, 3> kUiModes{"cli", "tui", "auto"};

    const std::string mirror_val = cfg.mirror.empty() ? env.mirror : cfg.mirror;
    const std::size_t mirror_idx = detail::MatchOptionIndex(mirror_val, kMirrors, 0);
    const std::size_t lang_idx = detail::MatchOptionIndex(cfg.lang, kLangs, 0);
    const std::size_t ui_idx = detail::MatchOptionIndex(cfg.ui_mode, kUiModes, 0);
    const bool interactive_on = cfg.interactive.find("true") != std::string::npos &&
                                cfg.interactive.find("false") == std::string::npos;

    View config_card = Card(
        Column {
            SectionHeader(
                "xlings 核心配置",
                "修改将直接写入 xlings 全局配置（仅在进入本标签页时按需读取）",
                Row {
                    loading_cfg
                        ? View(ProgressCircle().With(Frame{.width = 16.0F, .height = 16.0F}))
                        : View{},
                    ActionButton(
                        "重新读取配置",
                        ButtonTone::Secondary,
                        [store, tasks] { store->RefreshXlingsConfig(tasks, true); },
                        !loading_cfg,
                        icons::RefreshIcon(),
                        true
                    ),
                }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center))
            ),
            Row {
                Column {
                    Text("下载镜像源 (mirror)", TextRole::Label)
                        .Style(TextStyle{.font = Font::System(13.0F).WithWeight(FontWeight::Medium),
                                         .foreground = theme.colors.on_surface}),
                    SegmentedButton({"CN (国内镜像)", "GLOBAL (官方源)"}, mirror_idx)
                        .OnChanged([store, tasks, toast](std::size_t idx) {
                            store->SetMirrorAsync(tasks, toast, idx == 0 ? "CN" : "GLOBAL");
                        }),
                }.With(Spacing(6.0F), Grow(1.0F)),
                Column {
                    Text("输出语言 (lang)", TextRole::Label)
                        .Style(TextStyle{.font = Font::System(13.0F).WithWeight(FontWeight::Medium),
                                         .foreground = theme.colors.on_surface}),
                    SegmentedButton({"auto", "zh", "en"}, lang_idx)
                        .OnChanged([store, tasks, toast](std::size_t idx) {
                            const char* langs[] = {"auto", "zh", "en"};
                            if (idx < 3) store->SetLangAsync(tasks, toast, langs[idx]);
                        }),
                }.With(Spacing(6.0F), Grow(1.0F)),
            }.With(Spacing(16.0F)),
            Row {
                Column {
                    Text("CLI 界面模式 (ui-mode)", TextRole::Label)
                        .Style(TextStyle{.font = Font::System(13.0F).WithWeight(FontWeight::Medium),
                                         .foreground = theme.colors.on_surface}),
                    SegmentedButton({"cli", "tui", "auto"}, ui_idx)
                        .OnChanged([store, tasks, toast](std::size_t idx) {
                            const char* modes[] = {"cli", "tui", "auto"};
                            if (idx < 3) store->SetUiModeAsync(tasks, toast, modes[idx]);
                        }),
                }.With(Spacing(6.0F), Grow(1.0F)),
                Column {
                    Text("交互式确认提示 (interactive)", TextRole::Label)
                        .Style(TextStyle{.font = Font::System(13.0F).WithWeight(FontWeight::Medium),
                                         .foreground = theme.colors.on_surface}),
                    Switch("启用交互提示", interactive_on)
                        .OnChanged([store, tasks, toast](bool checked) {
                            store->SetInteractiveAsync(tasks, toast, checked);
                        }),
                }.With(Spacing(6.0F), Grow(1.0F)),
            }.With(Spacing(16.0F)),
            Divider(),
            KeyValueRow("XLINGS_HOME", cfg.xlings_home.empty() ? env.xlings_home : cfg.xlings_home, false, true),
            KeyValueRow("XLINGS_DATA", cfg.xlings_data.empty() ? env.data_dir : cfg.xlings_data, false, true),
            KeyValueRow("XLINGS_SUBOS", cfg.xlings_subos.empty() ? env.subos_dir : cfg.xlings_subos, false, true),
            KeyValueRow("xlings 二进制", env.binary_path, true, true),
        }.With(Spacing(14.0F), CrossAlign(CrossAxisAlignment::Stretch))
    );

    View doctor_card = Card(
        Column {
            SectionHeader(
                "环境诊断与修复 (xlings self doctor)",
                "校验包载荷、发布锚点、SubOS 沙箱隔离状态并自动修复",
                doctor_out.empty()
                    ? View{}
                    : ActionButton(
                          "复制诊断输出",
                          ButtonTone::Ghost,
                          [doctor_out, clipboard, toast] {
                              if (clipboard && clipboard->SupportsSynchronousText() &&
                                  clipboard->WriteText(doctor_out)) {
                                  toast.Show("已复制诊断结果");
                              } else {
                                  toast.Show("复制到剪贴板失败");
                              }
                          },
                          true,
                          icons::CopyIcon(),
                          true
                      )
            ),
            Flow {
                ActionButton(
                    "快速诊断",
                    ButtonTone::Primary,
                    [store, tasks, toast] { store->RunDoctorAsync(tasks, toast, false, false, false, false); },
                    true,
                    icons::WrenchIcon(),
                    true
                ),
                ActionButton(
                    "深度审查 (--deep)",
                    ButtonTone::Secondary,
                    [store, tasks, toast] { store->RunDoctorAsync(tasks, toast, true, false, false, false); },
                    true,
                    std::nullopt,
                    true
                ),
                ActionButton(
                    "沙箱隔离检查 (--isolation)",
                    ButtonTone::Secondary,
                    [store, tasks, toast] { store->RunDoctorAsync(tasks, toast, false, false, false, true); },
                    true,
                    std::nullopt,
                    true
                ),
                ActionButton(
                    "预演修复 (--fix --dry-run)",
                    ButtonTone::Secondary,
                    [store, tasks, toast] { store->RunDoctorAsync(tasks, toast, true, true, true, false); },
                    true,
                    std::nullopt,
                    true
                ),
                ActionButton(
                    "执行修复 (--fix)",
                    ButtonTone::Danger,
                    [store, tasks, toast, dialog] {
                        dialog.Show(
                            "确认执行 doctor --fix",
                            "将深度检查并修复各 SubOS 的发布锚点与绑定异常。",
                            "立即修复",
                            "取消",
                            [store, tasks, toast] { store->RunDoctorAsync(tasks, toast, true, true, false, false); },
                            {}
                        );
                    },
                    true,
                    icons::WrenchIcon(),
                    true
                ),
            }.With(Spacing(8.0F)),
            CodeBox(doctor_out, 130.0F, 260.0F),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    );

    std::vector<View> profile_rows;
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        const auto& p = profiles[i];
        profile_rows.push_back(
            Row {
                StatusBadge("Gen " + p.id, sp.info, sp.info_bg),
                Text(p.summary.empty() ? p.raw_line : p.summary, TextRole::Body)
                    .Style(TextStyle{.font = Font::Monospace(12.5F), .foreground = theme.colors.on_surface})
                    .With(Grow(1.0F)),
                ActionButton(
                    "回滚到此代际",
                    ButtonTone::Secondary,
                    [store, tasks, toast, dialog, id = p.id] {
                        dialog.Show(
                            "回滚 Profile 配置",
                            "确定要回滚到代际 `" + id + "` 吗？",
                            "确认回滚",
                            "取消",
                            [store, tasks, toast, id] { store->RollbackProfileAsync(tasks, toast, id); },
                            {}
                        );
                    },
                    true,
                    std::nullopt,
                    true
                ),
            }.With(
                Padding(10.0F),
                Spacing(10.0F),
                Background(sp.card_elevated_bg),
                Border{sp.border_soft, 1.0F},
                CornerRadius(8.0F),
                CrossAlign(CrossAxisAlignment::Center)
            ).Key("prof-" + p.id + "-" + std::to_string(i))
        );
    }

    View maintenance_card = Card(
        Column {
            SectionHeader("缓存维护、自更新与 Profile 代际快照"),
            Flow {
                ActionButton(
                    "清理缓存预演 (--dry-run)",
                    ButtonTone::Secondary,
                    [store, tasks, toast] { store->RunSelfCleanAsync(tasks, toast, true); },
                    true,
                    icons::TrashIcon(),
                    true
                ),
                ActionButton(
                    "立即清理未引用缓存",
                    ButtonTone::Danger,
                    [store, tasks, toast, dialog] {
                        dialog.Show(
                            "清理 xlings 缓存",
                            "将删除未被任何 SubOS 引用的孤立包载荷与临时下载缓存，是否继续？",
                            "确认清理",
                            "取消",
                            [store, tasks, toast] { store->RunSelfCleanAsync(tasks, toast, false); },
                            {}
                        );
                    },
                    true,
                    icons::TrashIcon(),
                    true
                ),
                ActionButton(
                    "更新 xlings 自身 (self update)",
                    ButtonTone::Primary,
                    [store, tasks, toast, dialog] {
                        dialog.Show(
                            "更新 xlings",
                            "将执行 `xlings self update --yes --agent` 检查并安装新版 xlings。",
                            "更新",
                            "取消",
                            [store, tasks, toast] { store->RunSelfUpdateAsync(tasks, toast); },
                            {}
                        );
                    },
                    true,
                    icons::DownloadIcon(),
                    true
                ),
            }.With(Spacing(8.0F)),
            clean_out.empty() ? View{} : CodeBox(clean_out, 90.0F, 180.0F),
            Divider(),
            Row {
                Text("Profile 配置代际 (xlings profile)", TextRole::Title)
                    .Style(TextStyle{.font = Font::System(14.5F).WithWeight(FontWeight::SemiBold),
                                     .foreground = theme.colors.on_surface})
                    .With(Grow(1.0F)),
                loading_prof
                    ? View(ProgressCircle().With(Frame{.width = 15.0F, .height = 15.0F}))
                    : View{},
                ActionButton(
                    "刷新代际",
                    ButtonTone::Secondary,
                    [store, tasks] { store->RefreshProfiles(tasks, true); },
                    !loading_prof,
                    icons::RefreshIcon(),
                    true
                ),
            }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
            Row {
                TextField(commit_reason)
                    .Placeholder("快照备注 (可选，例如: before upgrading llvm)")
                    .Variant(TextFieldVariant::Outlined)
                    .OnChanged([commit_reason](const TextEditingValue& v) mutable { commit_reason = v; })
                    .With(Grow(1.0F)),
                ActionButton(
                    "提交快照 (commit)",
                    ButtonTone::Secondary,
                    [store, tasks, toast, commit_reason]() mutable {
                        const std::string reason = commit_reason.Get().text;
                        commit_reason = TextEditingValue::FromText("");
                        store->CommitProfileAsync(tasks, toast, reason);
                    },
                    true,
                    icons::PlusIcon(),
                    false
                ),
            }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Center)),
            profile_rows.empty()
                ? Text("暂无已提交的 Profile 代际快照", TextRole::Label)
                      .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                : View(Column(std::move(profile_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    );

    std::vector<View> swatches;
    const auto presets = BrandPresets();
    for (std::size_t i = 0; i < presets.size(); ++i) {
        const Color preset = presets[i];
        const bool active = static_cast<int>(i) == app_cfg.brand_preset_index || preset == store->brand_color.Get();
        swatches.push_back(
            Row {}
                .With(
                    Frame{.width = 30.0F, .height = 30.0F},
                    Background(preset),
                    CornerRadius(15.0F),
                    Border{
                        active ? theme.colors.on_surface : WithAlpha(theme.colors.outline, 0.35F),
                        active ? 2.5F : 1.0F
                    },
                    Focusable(true),
                    PointerCursor(PointerCursorKind::Hand),
                    Semantics{.role = SemanticRole::Button, .label = "主题强调色", .selected = active}
                )
                .OnClick([store, idx = static_cast<int>(i)] { store->SetBrandPresetIndex(idx); })
                .Key("swatch-" + std::to_string(i))
        );
    }

    const auto resolve_brightness = [system_brightness](ThemeMode mode) noexcept {
        if (mode == ThemeMode::Auto) return system_brightness;
        return mode == ThemeMode::Dark ? ThemeBrightness::Dark : ThemeBrightness::Light;
    };
    const auto apply_theme = [store, transition, ui_tasks, theme_animating,
                              resolve_brightness, current_brightness = theme.brightness,
                              motion = theme.motion](ThemeMode target_mode) {
        if (theme_animating.Get()->animating || store->theme_mode.Get() == target_mode) {
            return;
        }
        const ThemeBrightness target_brightness = resolve_brightness(target_mode);
        auto mutation = [store, target_mode] {
            store->SetThemeMode(target_mode);
        };
        if (current_brightness == target_brightness || motion.reduced_motion) {
            mutation();
            return;
        }
        theme_animating.Get()->animating = true;
        ui_tasks.Launch([theme_animating, duration = motion.slow + motion.fast]() -> Task<void> {
            co_await Delay(std::chrono::duration<double>{duration});
            theme_animating.Get()->animating = false;
        });
        const TransitionSpec reveal{
            CircularRevealTransition{},
            TweenSpec{.duration = motion.slow, .easing = Easing::EaseInOut},
        };
        transition.RunFromCurrentInteraction(reveal, std::move(mutation));
    };

    const auto make_theme_icon = [&](ThemeMode target_mode, ImageVariant icon, std::string tooltip, std::string key) {
        const bool active = app_cfg.theme_mode == target_mode;
        const bool dark = theme.brightness == ThemeBrightness::Dark;
        const Color hover = dark ? Color::Rgb(255, 255, 255, 0.08F) : Color::Rgb(15, 23, 42, 0.06F);
        const Color press = dark ? Color::Rgb(255, 255, 255, 0.14F) : Color::Rgb(15, 23, 42, 0.11F);
        const Color fg = active ? theme.colors.primary : theme.colors.on_surface_variant;
        return Row {
            Image(std::move(icon))
                .Fit(ImageFit::Contain)
                .Align(HorizontalAlignment::Center, VerticalAlignment::Center)
                .Tint(fg)
                .With(Frame{.width = 16.0F, .height = 16.0F}),
        }.With(
            Padding(EdgeInsets::Symmetric(10.0F, 7.0F)),
            Background(active ? theme.colors.primary_container : Color::Transparent()),
            Border{active ? WithAlpha(theme.colors.primary, 0.35F) : Color::Transparent(), 1.0F},
            CornerRadius(8.0F),
            Indication{
                .hover = IndicationLayer{.fill = hover},
                .press = IndicationLayer{.fill = press},
            },
            Tooltip(tooltip),
            Focusable(true),
            PointerCursor(PointerCursorKind::Hand),
            Semantics{.role = SemanticRole::Button, .label = tooltip, .selected = active}
        ).OnClick([apply_theme, target_mode] { apply_theme(target_mode); })
         .Key(std::move(key));
    };

    View appearance_card = Card(
        Column {
            SectionHeader("Hxlings 界面外观", "调整客户端主题、强调色与快捷键设置，实时生效并保存到本地"),
            Row {
                Column {
                    Text("主题", TextRole::Body)
                        .Style(TextStyle{.font = Font::System(14.0F), .foreground = theme.colors.on_surface}),
                    Text("自动跟随系统，或固定浅色 / 深色外观（不影响 xlings CLI）", TextRole::Label)
                        .Style(TextStyle{.font = Font::System(12.0F), .foreground = theme.colors.on_surface_variant}),
                }.With(Spacing(2.0F), Grow(1.0F)),
                Row {
                    make_theme_icon(ThemeMode::Auto, app::images::theme_auto, "自动", "theme-mode-auto"),
                    make_theme_icon(ThemeMode::Light, app::images::sun, "浅色", "theme-mode-light"),
                    make_theme_icon(ThemeMode::Dark, app::images::moon, "深色", "theme-mode-dark"),
                }.With(
                    Padding(3.0F),
                    Spacing(4.0F),
                    Background(sp.muted_bg),
                    Border{sp.border_soft, 1.0F},
                    CornerRadius(10.0F),
                    CrossAlign(CrossAxisAlignment::Center)
                ),
            }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
            Row {
                Text("主题强调色", TextRole::Body)
                    .Style(TextStyle{.font = Font::System(14.0F), .foreground = theme.colors.on_surface})
                    .With(Grow(1.0F)),
                Row(std::move(swatches)).With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Center)),
            }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
            Divider(),
            Row {
                Column {
                    Text("快捷键", TextRole::Body)
                        .Style(TextStyle{.font = Font::System(14.0F), .foreground = theme.colors.on_surface}),
                    Text("查看全局页面导航、搜索聚焦与批量操作快捷键 (" + FormatShortcut("/") + ")", TextRole::Label)
                        .Style(TextStyle{.font = Font::System(12.0F), .foreground = theme.colors.on_surface_variant}),
                }.With(Spacing(2.0F), Grow(1.0F)),
                ActionButton(
                    "查看快捷键",
                    ButtonTone::Secondary,
                    [dialog] {
                        dialog.Show([](DialogContext ctx) { return ShortcutsDialogContent(ctx); });
                    },
                    true,
                    icons::KeyboardIcon(),
                    true
                ).Key("settings-open-shortcuts"),
            }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
        }.With(Spacing(14.0F), CrossAlign(CrossAxisAlignment::Stretch))
    );

    View tray_card = Card(
        Column {
            SectionHeader(
                "系统托盘与窗口行为",
                "管理状态栏/系统托盘驻留及关闭窗口时的默认动作",
                StatusBadge(
                    tray_available ? "托盘宿主可用" : "未检测到系统托盘",
                    tray_available ? sp.success : sp.warning,
                    tray_available ? sp.success_bg : sp.warning_bg
                )
            ),
            Row {
                Column {
                    Text("启用系统托盘图标", TextRole::Body)
                        .Style(TextStyle{.font = Font::System(14.0F), .foreground = theme.colors.on_surface}),
                    Text("在系统托盘展示快速切换页面、刷新与退出菜单", TextRole::Label)
                        .Style(TextStyle{.font = Font::System(12.0F), .foreground = theme.colors.on_surface_variant}),
                }.With(Spacing(2.0F), Grow(1.0F)),
                Switch("启用", app_cfg.tray_enabled)
                    .OnChanged([store](bool enabled) { store->SetTrayEnabled(enabled); }),
            }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
            Row {
                Column {
                    Text("关闭窗口时最小化到托盘", TextRole::Body)
                        .Style(TextStyle{.font = Font::System(14.0F), .foreground = theme.colors.on_surface}),
                    Text("仅当托盘可用时停留后台；使用托盘退出或 macOS ⌘Q 可彻底结束进程", TextRole::Label)
                        .Style(TextStyle{.font = Font::System(12.0F), .foreground = theme.colors.on_surface_variant}),
                }.With(Spacing(2.0F), Grow(1.0F)),
                Switch("驻留", app_cfg.close_to_tray || (app_cfg.lightweight_mode && app_cfg.tray_enabled))
                    .With(Enabled(app_cfg.tray_enabled))
                    .OnChanged([store](bool enabled) { store->SetCloseToTray(enabled); }),
            }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
            Row {
                Column {
                    Text("轻量级模式", TextRole::Body)
                        .Style(TextStyle{.font = Font::System(14.0F), .foreground = theme.colors.on_surface}),
                    Text("最小化到托盘时销毁 UI 窗口与渲染资源，仅保留后台托盘服务（依赖托盘开启）", TextRole::Label)
                        .Style(TextStyle{.font = Font::System(12.0F), .foreground = theme.colors.on_surface_variant}),
                }.With(Spacing(2.0F), Grow(1.0F)),
                Switch("轻量", app_cfg.lightweight_mode && app_cfg.tray_enabled)
                    .With(Enabled(app_cfg.tray_enabled))
                    .OnChanged([store](bool enabled) { store->SetLightweightMode(enabled); })
                    .Key("settings-lightweight-switch"),
            }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
        }.With(Spacing(14.0F), CrossAlign(CrossAxisAlignment::Stretch))
    );

    const std::string settings_path = mcpplibs::os::fs::utf8(DefaultAppSettingsPath());
    View storage_card = Card(
        Column {
            SectionHeader("应用本地配置", "与 xlings 全局 CLI 配置完全解耦，启动即零阻塞加载"),
            KeyValueRow("本地配置文件", settings_path, true, true),
        }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
    );

    const std::string cli_version_text =
        ver.version.empty()
            ? std::string("xlings CLI")
            : ("xlings " + ver.version +
               (ver.protocol_version.empty() ? std::string{} : (" (Protocol " + ver.protocol_version + ")")));
    View about_card = Card(
        Column {
            SectionHeader("关于", "应用版本、运行时引擎与开源协议信息"),
            Row {
                Image(app::images::hxlings)
                    .Fit(ImageFit::Contain)
                    .With(Frame{.width = 36.0F, .height = 36.0F}, CornerRadius(8.0F)),
                Column {
                    Row {
                        Text("Hxlings", TextRole::Title)
                            .Style(TextStyle{.font = Font::System(15.5F).WithWeight(FontWeight::Bold),
                                             .foreground = theme.colors.on_surface}),
                        StatusBadge("v0.1.2", theme.colors.primary, theme.colors.primary_container),
                    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Text("基于 C++23 Modules、mcpp 与 HuxerUI 构建的跨平台 xlings 桌面管理器", TextRole::Label)
                        .Style(TextStyle{.font = Font::System(12.0F),
                                         .foreground = theme.colors.on_surface_variant}),
                }.With(Spacing(3.0F), Grow(1.0F)),
            }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
            Divider(),
            KeyValueRow("应用版本", "Hxlings v0.1.2 · HuxerUI", false, true),
            KeyValueRow("xlings CLI", cli_version_text, false, true),
            KeyValueRow("开源协议", "MIT License", false, false),
            KeyValueRow("Hxlings 仓库", "https://github.com/FarnaHerry/Hxlings", true, true),
            KeyValueRow("HuxerUI 官方", "https://github.com/HuxerUI/HuxerUI", true, true),
            KeyValueRow("xlings 官方", "https://github.com/openxlings/xlings", true, true),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).Key("settings-about-card");

    View tab_bar = Row {
        SegmentedButton({"Hxlings 应用设置", "xlings 全局配置", "环境诊断与维护"}, tab)
            .OnChanged([store](std::size_t idx) { store->settings_tab = idx; })
            .Key("settings-tabs"),
        Spacer{}.With(Grow(1.0F)),
    }.With(
        Padding(EdgeInsets{.left = 22.0F, .top = 16.0F, .right = 22.0F, .bottom = 12.0F}),
        Background(theme.colors.surface),
        CrossAlign(CrossAxisAlignment::Center)
    );

    View body_content = View{};
    if (tab == 0) {
        body_content = Column {
            std::move(appearance_card),
            std::move(tray_card),
            std::move(storage_card),
            std::move(about_card),
        }.With(Padding(22.0F), Spacing(16.0F), CrossAlign(CrossAxisAlignment::Stretch)).Key("settings-tab-0");
    } else if (tab == 1) {
        body_content = Column {
            std::move(config_card),
        }.With(Padding(22.0F), Spacing(16.0F), CrossAlign(CrossAxisAlignment::Stretch)).Key("settings-tab-1");
    } else {
        body_content = Column {
            std::move(doctor_card),
            std::move(maintenance_card),
        }.With(Padding(22.0F), Spacing(16.0F), CrossAlign(CrossAxisAlignment::Stretch)).Key("settings-tab-2");
    }

    return Column {
        std::move(tab_bar),
        Divider(),
        ScrollView(std::move(body_content))
            .ScrollAxis(Axis::Vertical)
            .With(Grow(1.0F), ScrollBar{}),
    }.With(Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch));
}

} // namespace hxlings

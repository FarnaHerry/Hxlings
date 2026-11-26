#include <huxerui/huxerui.h>
#include <huxerui/testing/ui_test.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

import app;
import hxlings.theme;
import hxlings.shortcuts;
import hxlings.settings;
import hxlings.store;
import xlings.types;
import xlings.parser;
import hxlings.updater;
import hxlings.luban.types;

using namespace huxerui;
using namespace huxerui::testing;
using namespace hxlings;
using namespace std::chrono_literals;

#include "ui_layout_audit.hpp"

using namespace hxlings_test;

namespace {

void AuditAllPagesAtViewport(
    AuditContext& ctx,
    UiTest& ui,
    const std::shared_ptr<XlingsStore>& store,
    float w,
    float h
) {
    const std::string vp_tag = std::to_string(static_cast<int>(w)) + "x" + std::to_string(static_cast<int>(h));
    ui.SetWindowMetrics({.viewport = {w, h}});
    ui.Pump();

    // Dashboard
    SafeTap(ctx, vp_tag + " / Dashboard", ui.Find(UiSelector::Key("nav-dashboard")), "nav-dashboard");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "Dashboard", "page-dashboard");

    // Dashboard + Expanded TaskConsole + Active Task Progress
    if (SafeTap(ctx, vp_tag + " / Dashboard", ui.Find(UiSelector::Text("展开日志")), "展开日志")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "Dashboard + ExpandedConsole", "page-dashboard");

        store->task_running = true;
        store->task_title = "清理旧版本 xlings (13 个版本)";
        store->task_progress = 0.45F;
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "Dashboard + RunningTaskConsole", "page-dashboard");

        store->task_running = false;
        store->task_progress = std::optional<float>{};
        ui.Pump();
        SafeTap(ctx, vp_tag + " / Dashboard", ui.Find(UiSelector::Text("收起日志")), "收起日志");
        ui.Pump();
    }

    // Packages (with split view)
    SafeTap(ctx, vp_tag + " / Packages", ui.Find(UiSelector::Key("nav-packages")), "nav-packages");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "Packages", "page-packages", true);

    if (SafeTap(ctx, vp_tag + " / BatchPruneDialog", ui.Find(UiSelector::Text("批量清理")), "批量清理")) {
        ui.Pump(500ms);
        ui.Pump();
        const auto dlg_nodes = ui.Find(UiSelector::Text("批量清理过期版本")).All();
        const bool dlg_visible = std::ranges::any_of(
            dlg_nodes,
            [](const UiNodeInfo& n) { return n.participates_in_layout && n.in_viewport; }
        );
        if (!dlg_visible) {
            ctx.Flag(vp_tag + " / BatchPruneDialog", "BatchPruneDialog content not visible in viewport");
        }
        AuditCurrentScreen(ctx, ui, w, h, "BatchPruneDialog", "page-packages", false, true);

        const auto exec_btn_sel = UiSemanticSelector::AllOf(
            UiSemanticSelector::Role(SemanticRole::Button),
            UiSemanticSelector::Label("执行批量清理")
        );
        const auto exec_sem = ui.FindSemantics(exec_btn_sel);
        if (!exec_sem.Exists() || !exec_sem.One().enabled) {
            ctx.Flag(vp_tag + " / BatchPruneDialog",
                     "执行批量清理 should be enabled by default even when a protected package is present");
        }
        if (SafeTap(ctx, vp_tag + " / BatchPruneDialog", ui.Find(UiSelector::Text("全不选")), "全不选")) {
            ui.Pump();
            if (!ui.FindSemantics(exec_btn_sel).Exists() || ui.FindSemantics(exec_btn_sel).One().enabled) {
                ctx.Flag(vp_tag + " / BatchPruneDialog", "执行批量清理 should be disabled after 全不选");
            }
        }
        if (SafeTap(ctx, vp_tag + " / BatchPruneDialog", ui.Find(UiSelector::Text("全选")), "全选")) {
            ui.Pump();
            if (!ui.FindSemantics(exec_btn_sel).Exists() || !ui.FindSemantics(exec_btn_sel).One().enabled) {
                ctx.Flag(vp_tag + " / BatchPruneDialog", "执行批量清理 should be re-enabled after 全选");
            }
        }

        if (SafeTap(ctx, vp_tag + " / BatchPruneDialog", ui.Find(UiSelector::Text("取消")), "取消(BatchPruneDialog)")) {
            try {
                ui.PumpUntil(
                    [&] {
                        return !ui.Find(UiSelector::Text("执行批量清理")).Exists() &&
                               std::ranges::none_of(
                                   ui.Find(UiSelector::Text("批量清理过期版本")).All(),
                                   [](const UiNodeInfo& n) { return n.participates_in_layout; }
                               );
                    },
                    {.timeout = 2s, .step = 50ms}
                );
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / BatchPruneDialog", std::string("BatchPruneDialog still mounted after cancel: ") + ex.what());
            }
        }
    }

    if (SafeTap(ctx, vp_tag + " / BatchUpdateDialog", ui.Find(UiSelector::Text("批量更新")), "批量更新")) {
        ui.Pump(500ms);
        ui.Pump();
        const auto upd_nodes = ui.Find(UiSelector::Text("批量更新软件包")).All();
        const bool upd_visible = std::ranges::any_of(
            upd_nodes,
            [](const UiNodeInfo& n) { return n.participates_in_layout && n.in_viewport; }
        );
        if (!upd_visible) {
            ctx.Flag(vp_tag + " / BatchUpdateDialog", "BatchUpdateDialog content not visible in viewport");
        }
        AuditCurrentScreen(ctx, ui, w, h, "BatchUpdateDialog", "page-packages", false, true);

        const auto upd_exec_sel = UiSemanticSelector::AllOf(
            UiSemanticSelector::Role(SemanticRole::Button),
            UiSemanticSelector::Label("执行批量更新")
        );
        const auto upd_exec_sem = ui.FindSemantics(upd_exec_sel);
        if (!upd_exec_sem.Exists() || !upd_exec_sem.One().enabled) {
            ctx.Flag(vp_tag + " / BatchUpdateDialog",
                     "执行批量更新 should be enabled by default even when a skipped package is present");
        }
        if (SafeTap(ctx, vp_tag + " / BatchUpdateDialog", ui.Find(UiSelector::Text("全不选")), "全不选(BatchUpdateDialog)")) {
            ui.Pump();
            if (!ui.FindSemantics(upd_exec_sel).Exists() || ui.FindSemantics(upd_exec_sel).One().enabled) {
                ctx.Flag(vp_tag + " / BatchUpdateDialog", "执行批量更新 should be disabled after 全不选");
            }
        }
        if (SafeTap(ctx, vp_tag + " / BatchUpdateDialog", ui.Find(UiSelector::Text("全选")), "全选(BatchUpdateDialog)")) {
            ui.Pump();
            if (!ui.FindSemantics(upd_exec_sel).Exists() || !ui.FindSemantics(upd_exec_sel).One().enabled) {
                ctx.Flag(vp_tag + " / BatchUpdateDialog", "执行批量更新 should be re-enabled after 全选");
            }
        }

        if (SafeTap(ctx, vp_tag + " / BatchUpdateDialog", ui.Find(UiSelector::Text("取消")), "取消(BatchUpdateDialog)")) {
            try {
                ui.PumpUntil(
                    [&] {
                        return !ui.Find(UiSelector::Text("执行批量更新")).Exists() &&
                               std::ranges::none_of(
                                   ui.Find(UiSelector::Text("批量更新软件包")).All(),
                                   [](const UiNodeInfo& n) { return n.participates_in_layout; }
                               );
                    },
                    {.timeout = 2s, .step = 50ms}
                );
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / BatchUpdateDialog", std::string("BatchUpdateDialog still mounted after cancel: ") + ex.what());
            }
        }
    }

    // Store (with split view)
    SafeTap(ctx, vp_tag + " / Store", ui.Find(UiSelector::Key("nav-store")), "nav-store");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "Store", "page-store", true);

    // SubOS (all 6 tabs, with split view + Diff / Export modals)
    SafeTap(ctx, vp_tag + " / SubOS", ui.Find(UiSelector::Key("nav-subos")), "nav-subos");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "SubOS[Shims]", "page-subos", true);

    if (SafeTap(ctx, vp_tag + " / SubOS[EnvVars]", ui.Find(UiSelector::Text("环境")), "环境")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "SubOS[EnvVars]", "page-subos", true);
    }

    if (SafeTap(ctx, vp_tag + " / SubOS[Exec]", ui.Find(UiSelector::Text("执行")), "执行")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "SubOS[Exec]", "page-subos", true);
    }

    if (SafeTap(ctx, vp_tag + " / SubOS[Policy]", ui.Find(UiSelector::Text("策略")), "策略")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "SubOS[Policy]", "page-subos", true);
    }

    if (SafeTap(ctx, vp_tag + " / SubOS[Sessions]", ui.Find(UiSelector::Text("会话")), "会话")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "SubOS[Sessions]", "page-subos", true);
    }

    if (SafeTap(ctx, vp_tag + " / SubOS[Events]", ui.Find(UiSelector::Text("审计")), "审计")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "SubOS[Events]", "page-subos", true);
    }

    if (SafeTap(ctx, vp_tag + " / SubOS[Shims]", ui.Find(UiSelector::Text("Shims")), "Shims")) {
        ui.Pump();
    }

    // New SubOS modal dialog
    if (SafeTap(ctx, vp_tag + " / NewSubosDialog", ui.Find(UiSelector::Text("新建 SubOS")), "新建 SubOS")) {
        ui.Pump(500ms);
        ui.Pump();
        auto dlg_title = ui.Find(UiSelector::Text("新建 SubOS 子系统"));
        if (!dlg_title.Exists() || !dlg_title.One().in_viewport) {
            ctx.Flag(vp_tag + " / NewSubosDialog", "NewSubosDialog content not visible in viewport");
        }
        AuditCurrentScreen(ctx, ui, w, h, "NewSubosDialog", "page-subos", false, true);
        if (SafeTap(ctx, vp_tag + " / NewSubosDialog", ui.Find(UiSelector::Text("取消")), "取消(NewSubosDialog)")) {
            try {
                ui.PumpUntil([&] { return !ui.Find(UiSelector::Text("新建 SubOS 子系统")).Exists(); },
                             {.timeout = 2s, .step = 50ms});
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / NewSubosDialog", std::string("NewSubosDialog still mounted after cancel: ") + ex.what());
            }
        }
    }

    // SubOS Diff modal dialog
    if (SafeTap(ctx, vp_tag + " / SubosDiffDialog", ui.Find(UiSelector::Text("对比 Diff")), "对比 Diff")) {
        ui.Pump(500ms);
        ui.Pump();
        if (!ui.Find(UiSelector::Key("subos-diff-dialog")).Exists()) {
            ctx.Flag(vp_tag + " / SubosDiffDialog", "SubosDiffDialog did not mount");
        }
        AuditCurrentScreen(ctx, ui, w, h, "SubosDiffDialog", "page-subos", false, true);
        if (SafeTap(ctx, vp_tag + " / SubosDiffDialog", ui.Find(UiSelector::Text("关闭")), "关闭(SubosDiffDialog)")) {
            try {
                ui.PumpUntil([&] { return !ui.Find(UiSelector::Key("subos-diff-dialog")).Exists(); },
                             {.timeout = 2s, .step = 50ms});
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / SubosDiffDialog", std::string("SubosDiffDialog still mounted after close: ") + ex.what());
            }
        }
    }

    // SubOS Export / Pack modal dialog
    if (SafeTap(ctx, vp_tag + " / SubosExportDialog", ui.Find(UiSelector::Text("导出 / 打包")), "导出 / 打包")) {
        ui.Pump(500ms);
        ui.Pump();
        if (!ui.Find(UiSelector::Key("subos-export-dialog")).Exists()) {
            ctx.Flag(vp_tag + " / SubosExportDialog", "SubosExportDialog did not mount");
        }
        AuditCurrentScreen(ctx, ui, w, h, "SubosExportDialog", "page-subos", false, true);
        if (SafeTap(ctx, vp_tag + " / SubosExportDialog", ui.Find(UiSelector::Text("取消")), "取消(SubosExportDialog)")) {
            try {
                ui.PumpUntil([&] { return !ui.Find(UiSelector::Key("subos-export-dialog")).Exists(); },
                             {.timeout = 2s, .step = 50ms});
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / SubosExportDialog", std::string("SubosExportDialog still mounted after cancel: ") + ex.what());
            }
        }
    }

    // LubanOS (all 3 outer tabs, 3 env detail tabs, and NewLubanEnv modal)
    store->luban_tab = 0;
    SafeTap(ctx, vp_tag + " / LubanOS", ui.Find(UiSelector::Key("nav-luban")), "nav-luban");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "LubanOS[Environments]", "page-luban", true);

    if (SafeTap(ctx, vp_tag + " / LubanOS[RunCmd]", ui.Find(UiSelector::Text("执行命令")), "执行命令")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "LubanOS[RunCmd]", "page-luban", true);
    }

    if (SafeTap(ctx, vp_tag + " / LubanOS[HistoryLogs]", ui.Find(UiSelector::Text("历史与日志")), "历史与日志")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "LubanOS[HistoryLogs]", "page-luban", true);
    }

    if (SafeTap(ctx, vp_tag + " / LubanOS[Overview]", ui.Find(UiSelector::Text("概览与配置")), "概览与配置")) {
        ui.Pump();
    }

    if (SafeTap(ctx, vp_tag + " / NewLubanEnvDialog", ui.Find(UiSelector::Text("新建环境")), "新建环境")) {
        ui.Pump(500ms);
        ui.Pump();
        if (!ui.Find(UiSelector::Key("new-luban-env-dialog")).Exists()) {
            ctx.Flag(vp_tag + " / NewLubanEnvDialog", "NewLubanEnvDialog did not mount");
        }
        AuditCurrentScreen(ctx, ui, w, h, "NewLubanEnvDialog", "page-luban", false, true);
        if (SafeTap(ctx, vp_tag + " / NewLubanEnvDialog", ui.Find(UiSelector::Text("取消")), "取消(NewLubanEnvDialog)")) {
            try {
                ui.PumpUntil([&] { return !ui.Find(UiSelector::Key("new-luban-env-dialog")).Exists(); },
                             {.timeout = 2s, .step = 50ms});
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / NewLubanEnvDialog",
                         std::string("NewLubanEnvDialog still mounted after cancel: ") + ex.what());
            }
        }
    }

    if (SafeTap(ctx, vp_tag + " / LubanOS[Catalog]", ui.Find(UiSelector::Text("发行版与桌面目录")), "发行版与桌面目录")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "LubanOS[Catalog]", "page-luban");
    }

    if (SafeTap(ctx, vp_tag + " / LubanOS[Stacks]", ui.Find(UiSelector::Text("蓝图编排")), "蓝图编排")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "LubanOS[Stacks]", "page-luban");
    }

    if (SafeTap(ctx, vp_tag + " / LubanOS[BackToEnvs]", ui.Find(UiSelector::Text("环境与机器")), "环境与机器")) {
        ui.Pump();
    }

    // Indexes + AddRepo dialog
    SafeTap(ctx, vp_tag + " / Indexes", ui.Find(UiSelector::Key("nav-indexes")), "nav-indexes");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "Indexes", "page-indexes");

    if (SafeTap(ctx, vp_tag + " / AddRepoDialog", ui.Find(UiSelector::Text("添加 / 更新仓库")), "添加 / 更新仓库")) {
        ui.Pump(500ms);
        ui.Pump();
        auto dlg_title = ui.Find(UiSelector::Text("添加或更新索引仓库"));
        if (!dlg_title.Exists() || !dlg_title.One().in_viewport) {
            ctx.Flag(vp_tag + " / AddRepoDialog", "AddRepoDialog content not visible in viewport");
        }
        AuditCurrentScreen(ctx, ui, w, h, "AddRepoDialog", "page-indexes", false, true);
        if (SafeTap(ctx, vp_tag + " / AddRepoDialog", ui.Find(UiSelector::Text("取消")), "取消(AddRepoDialog)")) {
            try {
                ui.PumpUntil([&] { return !ui.Find(UiSelector::Text("添加或更新索引仓库")).Exists(); },
                             {.timeout = 2s, .step = 50ms});
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / AddRepoDialog", std::string("AddRepoDialog still mounted after cancel: ") + ex.what());
            }
        }
    }

    // Settings (3 tabs: App Settings, xlings Config, Diagnostics & Maintenance)
    SafeTap(ctx, vp_tag + " / Settings", ui.Find(UiSelector::Key("nav-settings")), "nav-settings");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "Settings[App]", "page-settings");

    if (!ui.Find(UiSelector::Key("settings-lightweight-switch")).Exists()) {
        ctx.Flag(vp_tag + " / Settings[App]", "Missing settings-lightweight-switch");
    }
    if (!ui.Find(UiSelector::Key("settings-about-card")).Exists()) {
        ctx.Flag(vp_tag + " / Settings[App]", "Missing settings-about-card");
    }
    store->last_opened_url = std::string{};
    try {
        auto scroll_q = ui.Find(UiSelector::Key("settings-scroll"));
        auto link_q = scroll_q.ScrollUntil(UiSelector::Key("about-link-hxlings"));
        ui.Pump();
        if (SafeTap(ctx, vp_tag + " / SettingsAboutLink", link_q, "about-link-hxlings")) {
            ui.Pump();
            if (store->last_opened_url.Get() != "https://github.com/FarnaHerry/Hxlings") {
                ctx.Flag(vp_tag + " / SettingsAboutLink",
                         "Clicking about-link-hxlings did not record GitHub URL (got '" +
                             store->last_opened_url.Get() + "')");
            }
        }
        auto mcpp_link_q = scroll_q.ScrollUntil(UiSelector::Key("about-link-mcpp"));
        ui.Pump();
        if (SafeTap(ctx, vp_tag + " / SettingsAboutMcppLink", mcpp_link_q, "about-link-mcpp")) {
            ui.Pump();
            if (store->last_opened_url.Get() != "https://github.com/mcpp-community/mcpp") {
                ctx.Flag(vp_tag + " / SettingsAboutMcppLink",
                         "Clicking about-link-mcpp did not record mcpp GitHub URL (got '" +
                             store->last_opened_url.Get() + "')");
            }
        }
        auto upd_btn_q = scroll_q.ScrollUntil(UiSelector::Key("about-check-update-btn"));
        ui.Pump();
        if (!ui.Find(UiSelector::Key("settings-app-update-box")).Exists()) {
            ctx.Flag(vp_tag + " / SettingsAboutUpdate", "Missing settings-app-update-box");
        }
        AuditCurrentScreen(ctx, ui, w, h, "Settings[AppAboutUpdate]", "page-settings");
        if (SafeTap(ctx, vp_tag + " / SettingsCheckUpdate", upd_btn_q, "about-check-update-btn")) {
            ui.Pump();
            if (store->app_update_state.Get().status_message.empty()) {
                ctx.Flag(vp_tag + " / SettingsCheckUpdate",
                         "Tapping about-check-update-btn in test mode did not update status_message");
            }
        }
        scroll_q.ScrollBy({0.0F, -2000.0F});
        ui.Pump();
    } catch (const UiTestFailure& ex) {
        ctx.Flag(vp_tag + " / SettingsAboutLink", std::string("ScrollUntil about-link-hxlings failed: ") + ex.what());
    }
    if (SafeTap(ctx, vp_tag + " / ThemeDark", ui.Find(UiSelector::Key("theme-mode-dark")), "theme-mode-dark")) {
        ui.Pump(550ms);
        ui.Pump();
        if (store->theme_mode.Get() != ThemeMode::Dark) {
            ctx.Flag(vp_tag + " / ThemeDark", "Clicking theme-mode-dark did not activate ThemeMode::Dark");
        }
    }
    if (SafeTap(ctx, vp_tag + " / ThemeLight", ui.Find(UiSelector::Key("theme-mode-light")), "theme-mode-light")) {
        ui.Pump(550ms);
        ui.Pump();
        if (store->theme_mode.Get() != ThemeMode::Light) {
            ctx.Flag(vp_tag + " / ThemeLight", "Clicking theme-mode-light did not activate ThemeMode::Light");
        }
    }
    if (SafeTap(ctx, vp_tag + " / ThemeAuto", ui.Find(UiSelector::Key("theme-mode-auto")), "theme-mode-auto")) {
        ui.Pump(550ms);
        ui.Pump();
        if (store->theme_mode.Get() != ThemeMode::Auto) {
            ctx.Flag(vp_tag + " / ThemeAuto", "Clicking theme-mode-auto did not activate ThemeMode::Auto");
        }
    }
    if (SafeTap(ctx, vp_tag + " / SettingsShortcuts", ui.Find(UiSelector::Key("settings-open-shortcuts")), "settings-open-shortcuts")) {
        ui.Pump(500ms);
        ui.Pump();
        if (!ui.Find(UiSelector::Key("shortcuts-dialog")).Exists()) {
            ctx.Flag(vp_tag + " / SettingsShortcuts", "Clicking settings-open-shortcuts did not open shortcuts-dialog");
        } else {
            AuditCurrentScreen(ctx, ui, w, h, "Settings[ShortcutsDialog]", "page-settings", false, true);
            ui.PressKey(Key::Escape);
            try {
                ui.PumpUntil([&] { return !ui.Find(UiSelector::Key("shortcuts-dialog")).Exists(); },
                             {.timeout = 2s, .step = 50ms});
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / SettingsShortcuts", std::string("Escape did not close shortcuts-dialog: ") + ex.what());
            }
        }
    }

    if (SafeTap(ctx, vp_tag + " / Settings[XlingsConfig]", ui.Find(UiSelector::Text("xlings 全局配置")), "xlings 全局配置")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "Settings[XlingsConfig]", "page-settings");
    }

    if (SafeTap(ctx, vp_tag + " / Settings[Maintenance]", ui.Find(UiSelector::Text("环境诊断与维护")), "环境诊断与维护")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "Settings[Maintenance]", "page-settings");
    }

    SafeTap(ctx, vp_tag + " / Settings[AppReset]", ui.Find(UiSelector::Text("Hxlings 应用设置")), "Hxlings 应用设置");
    ui.Pump();

    // Dark mode check on Dashboard & Settings
    store->SetDarkMode(true);
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "Settings(Dark)", "page-settings");
    SafeTap(ctx, vp_tag + " / Dashboard(Dark)", ui.Find(UiSelector::Key("nav-dashboard")), "nav-dashboard");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "Dashboard(Dark)", "page-dashboard");

    store->SetDarkMode(false);
    ui.Pump();
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path res_root = ResolveResourcesRoot(argc > 0 ? argv[0] : nullptr);
    auto resources = std::make_shared<DeployedResources>(res_root);
    std::shared_ptr<XlingsStore> store;

    Application test_app(
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
            .application_hooks = {
                [&store](ApplicationContext& context) {
                    store = MakeSeededStore();
                    context.Provide(store);
                },
            },
        }
    );

    UiTest ui(test_app, {
        .viewport = {1180.0F, 760.0F},
        .resource_provider = resources,
    });
    ui.Pump();

    AuditContext ctx;
    AuditAllPagesAtViewport(ctx, ui, store, 1180.0F, 760.0F);
    AuditAllPagesAtViewport(ctx, ui, store, 880.0F, 560.0F);

    // Spot check 1440x900
    ui.SetWindowMetrics({.viewport = {1440.0F, 900.0F}});
    ui.Pump();
    SafeTap(ctx, "1440x900 / Dashboard", ui.Find(UiSelector::Key("nav-dashboard")), "nav-dashboard");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, 1440.0F, 900.0F, "Dashboard", "page-dashboard");
    SafeTap(ctx, "1440x900 / Packages", ui.Find(UiSelector::Key("nav-packages")), "nav-packages");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, 1440.0F, 900.0F, "Packages", "page-packages", true);

    if (ctx.anomalies.empty()) {
        std::cout << "UI layout test passed with 0 anomalies.\n";
        return 0;
    }

    std::cerr << "Detected " << ctx.anomalies.size() << " UI layout anomaly(ies):\n";
    for (std::size_t i = 0; i < ctx.anomalies.size(); ++i) {
        std::cerr << "  " << (i + 1) << ". " << ctx.anomalies[i] << '\n';
    }
    return 1;
}

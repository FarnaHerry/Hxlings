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

void TestDuplicateDataNoDuplicateKeys(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    try {
        PkgGroup dup_group{
            .key = "xim:dup-pkg",
            .ns = "xim",
            .name = "dup-pkg",
            .description = "duplicate group regression entry",
            .active_version = "1.0.0",
            .versions = {
                InstalledPkg{.raw_spec = "xim:dup-pkg@1.0.0", .ns = "xim", .name = "dup-pkg", .version = "1.0.0", .is_active = true},
            },
        };
        store->installed_groups = {dup_group, dup_group};
        store->selected_installed_key = "xim:dup-pkg";
        store->selected_installed_detail = PackageDetail{
            .title = "xim:dup-pkg",
            .package = "xim:dup-pkg",
            .ns = "xim",
            .name = "dup-pkg",
            .description = "duplicate detail versions",
            .status = "stable",
            .selected_version = "1.0.0",
            .available_versions = {"1.0.0", "1.0.0", "0.9.0"},
        };

        SearchPkg dup_search{
            .package = "xim:dup-search",
            .ns = "xim",
            .name = "dup-search",
            .version = "2.0.0",
            .description = "duplicate search result",
            .status = "stable",
            .index = "xim",
        };
        store->search_results = {dup_search, dup_search};
        store->selected_store_pkg = "xim:dup-search";
        store->selected_store_detail = PackageDetail{
            .title = "xim:dup-search",
            .package = "xim:dup-search",
            .ns = "xim",
            .name = "dup-search",
            .description = "duplicate store versions",
            .status = "stable",
            .selected_version = "2.0.0",
            .available_versions = {"2.0.0", "2.0.0", "1.0.0"},
        };

        SubosEntry dup_subos{
            .name = "default",
            .dir = "/home/farna/.xlings/subos/default",
            .abi = "native",
            .kind = "view",
            .view = "overlay",
            .active = true,
            .packages = 10,
            .commands = 5,
        };
        store->subos_list = {dup_subos, dup_subos};
        store->selected_subos = "default";
        store->active_shims = {"gcc", "gcc", "clang"};

        IndexSnapshot dup_snap{
            .index_version = "bafdb27",
            .generated_at = "2026-11-22T07:36:45Z",
            .artifact_name = "xim-bafdb27.tar.gz",
            .size_bytes = 1024ULL,
            .current = true,
            .installed = true,
        };
        IndexRepoVersions dup_repo{
            .name = "xim",
            .current = "bafdb27",
            .installed = "bafdb27",
            .snapshots = {dup_snap, dup_snap},
        };
        store->index_versions = {dup_repo, dup_repo};

        const auto saved_luban_envs = store->luban_envs.Get();
        const auto saved_luban_catalog = store->luban_catalog.Get();
        LubanEnvironment dup_luban{
            .name = "dev-core",
            .kind = "rootfs",
            .edition = "core",
            .carrier = "bwrap",
            .shell = "bash",
            .packages = 18,
            .running = true,
        };
        store->luban_envs = {dup_luban, dup_luban};
        store->selected_luban_env = "dev-core";

        LubanCatalogItem dup_dist{
            .group = "distributions",
            .name = "core",
            .ref = "luban:core",
            .version = "2026.11",
            .status = "stable",
            .title_zh = "Luban Core",
            .summary_zh = "duplicate dist item",
            .tier = 1,
            .available = true,
        };
        LubanCatalogItem dup_desk{
            .group = "desktops",
            .name = "xfce",
            .ref = "luban:desktop-xfce",
            .version = "4.18",
            .status = "stable",
            .title_zh = "XFCE",
            .summary_zh = "duplicate desk item",
            .tier = 1,
            .available = true,
        };
        store->luban_catalog = LubanCatalog{
            .distributions = {dup_dist, dup_dist},
            .desktops = {dup_desk, dup_desk},
        };

        for (const char* nav_key : {"nav-packages", "nav-store", "nav-subos", "nav-luban", "nav-indexes"}) {
            if (SafeTap(ctx, "DuplicateKeysRegression", ui.Find(UiSelector::Key(nav_key)), nav_key)) {
                ui.Pump();
            }
        }
        store->luban_tab = 1;
        store->active_nav = 4;
        ui.Pump();
        store->luban_envs = saved_luban_envs;
        store->luban_catalog = saved_luban_catalog;
        store->luban_tab = 0;
        ui.Pump();
    } catch (const std::exception& ex) {
        ctx.Flag("DuplicateKeysRegression", std::string("Exception during duplicate data recomposition: ") + ex.what());
    }
}

void TestCrossPlatformShortcuts(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    ui.SetWindowMetrics({.viewport = {1180.0F, 760.0F}});
    store->active_nav = 0;
    store->SetDarkMode(false);
    store->console_expanded = false;
    ui.Pump();

    ui.PressKey(Key::Digit2, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 1) {
        ctx.Flag("Shortcuts", "Primary+2 did not switch to Packages page");
    }

    ui.PressKey(Key::Digit3, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 2) {
        ctx.Flag("Shortcuts", "Primary+3 did not switch to Store page");
    }

    ui.PressKey(Key::Digit5, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 4) {
        ctx.Flag("Shortcuts", "Primary+5 did not switch to LubanOS page");
    }

    ui.PressKey(Key::Digit6, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 5) {
        ctx.Flag("Shortcuts", "Primary+6 did not switch to Indexes page");
    }

    ui.PressKey(Key::Digit7, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 6) {
        ctx.Flag("Shortcuts", "Primary+7 did not switch to Settings page");
    }

    store->active_nav = 0;
    ui.Pump();
    ui.PressKey(Key::Comma, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 6) {
        ctx.Flag("Shortcuts", "Primary+Comma did not switch to Settings page");
    }

    ui.PressKey(Key::K, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 2) {
        ctx.Flag("Shortcuts", "Primary+K did not switch to Store page");
    }
    const auto search_box = ui.Find(UiSelector::Key("store-search-input"));
    if (!search_box.Exists() || !search_box.One().focused) {
        ctx.Flag("Shortcuts", "Primary+K did not focus store-search-input");
    }

    ui.PressKey(Key::J, PrimaryModifier());
    ui.Pump();
    if (!store->console_expanded.Get()) {
        ctx.Flag("Shortcuts", "Primary+J did not expand task console");
    }
    ui.PressKey(Key::J, PrimaryModifier());
    ui.Pump();
    if (store->console_expanded.Get()) {
        ctx.Flag("Shortcuts", "Second Primary+J did not collapse task console");
    }

    ui.PressKey(Key::D, PrimaryShiftModifier());
    ui.Pump(300ms);
    ui.Pump();
    if (!store->dark_mode.Get()) {
        ctx.Flag("Shortcuts", "Primary+Shift+D did not enable dark mode");
    }
    ui.PressKey(Key::D, PrimaryShiftModifier());
    ui.Pump(300ms);
    ui.Pump();
    if (store->dark_mode.Get()) {
        ctx.Flag("Shortcuts", "Second Primary+Shift+D did not restore light mode");
    }

    ui.PressKey(Key::Slash, PrimaryModifier());
    ui.Pump(500ms);
    ui.Pump();
    if (!ui.Find(UiSelector::Key("shortcuts-dialog")).Exists()) {
        ctx.Flag("Shortcuts", "Primary+/ did not open shortcuts-dialog");
    } else {
        AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "ShortcutsDialog", "page-store", false, true);
        ui.PressKey(Key::Escape);
        try {
            ui.PumpUntil([&] { return !ui.Find(UiSelector::Key("shortcuts-dialog")).Exists(); },
                         {.timeout = 2s, .step = 50ms});
        } catch (const UiTestFailure& ex) {
            ctx.Flag("Shortcuts", std::string("Escape did not close shortcuts-dialog: ") + ex.what());
        }
    }

    ui.PressKey(Key::U, PrimaryShiftModifier());
    ui.Pump(500ms);
    ui.Pump();
    const bool upd_open = std::ranges::any_of(
        ui.Find(UiSelector::Text("批量更新软件包")).All(),
        [](const UiNodeInfo& n) { return n.participates_in_layout && n.in_viewport; }
    );
    if (store->active_nav.Get() != 1 || !upd_open) {
        ctx.Flag("Shortcuts", "Primary+Shift+U did not open BatchUpdateDialog on Packages page");
    } else {
        ui.PressKey(Key::Escape);
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
            ctx.Flag("Shortcuts", std::string("Escape did not close BatchUpdateDialog: ") + ex.what());
        }
    }
}

void TestMacOsTitleBarMetrics(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    store->active_nav = 0;
    for (const auto [w, h] : std::array<std::pair<float, float>, 2>{{{1180.0F, 760.0F}, {880.0F, 560.0F}}}) {
        ui.SetWindowMetrics({
            .viewport = {w, h},
            .title_bar = WindowTitleBarMetrics{
                .height = 44.0F,
                .left_inset = 78.0F,
                .right_inset = 0.0F,
                .maximized = false,
            },
        });
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "MacTitleBar", "page-dashboard");
    }
    ui.SetWindowMetrics({.viewport = {1180.0F, 760.0F}});
    ui.Pump();
}

void TestStorePaginationAndErrorStates(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    ui.SetWindowMetrics({.viewport = {1180.0F, 760.0F}});
    store->active_nav = 2;
    const auto saved_results = store->search_results.Get();
    const auto saved_error = store->search_error.Get();

    std::vector<SearchPkg> many;
    many.reserve(80);
    for (int i = 0; i < 80; ++i) {
        many.push_back(SearchPkg{
            .package = "xim:pkg-" + std::to_string(i),
            .ns = "xim",
            .name = "pkg-" + std::to_string(i),
            .version = "1.0." + std::to_string(i),
            .description = "Sample package description #" + std::to_string(i),
            .status = "stable",
            .index = "xim",
        });
    }
    store->search_error = std::string{};
    store->search_results = std::move(many);
    ui.Pump();
    AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "Store[80Results]", "page-store", true);

    if (ui.Find(UiSelector::Key("store-load-more")).Exists()) {
        ctx.Flag("StoreVirtualScroll", "Manual store-load-more button should not exist");
    }
    if (!ui.Find(UiSelector::Key("store-results-list")).Exists()) {
        ctx.Flag("StoreVirtualScroll", "Missing store-results-list virtual list");
    }
    if (!ui.Find(UiSelector::Key("search-xim:pkg-0-0")).Exists()) {
        ctx.Flag("StoreVirtualScroll", "Missing initial virtualized item search-xim:pkg-0-0");
    }
    if (ui.Find(UiSelector::Key("search-xim:pkg-79-79")).Exists()) {
        ctx.Flag("StoreVirtualScroll", "Tail item search-xim:pkg-79-79 should not be eagerly mounted before scrolling");
    }

    try {
        auto list_q = ui.Find(UiSelector::Key("store-results-list"));
        auto tail_q = list_q.ScrollUntil(
            UiSelector::Key("search-xim:pkg-79-79"),
            {.step = {0.0F, 420.0F}, .maximum_steps = 30}
        );
        ui.Pump();
        if (!tail_q.Exists() || !tail_q.One().in_viewport) {
            ctx.Flag("StoreVirtualScroll", "Scrolling store-results-list did not reveal search-xim:pkg-79-79");
        }
        AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "Store[80ResultsScrolled]", "page-store", true);
    } catch (const UiTestFailure& ex) {
        ctx.Flag("StoreVirtualScroll", std::string("ScrollUntil search-xim:pkg-79-79 failed: ") + ex.what());
    }

    store->search_results = std::vector<SearchPkg>{};
    store->search_error = std::string("search failed: index not synced");
    ui.Pump();
    if (!ui.Find(UiSelector::Text("搜索软件包失败")).Exists()) {
        ctx.Flag("StoreError", "Store error state message not rendered");
    }
    AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "Store[ErrorState]", "page-store", true);

    store->search_error = saved_error;
    store->search_results = saved_results;
    ui.Pump();
}

void TestSearchBoxes(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    ui.SetWindowMetrics({.viewport = {1180.0F, 760.0F}});
    store->selected_subos = "default";
    ui.Pump();

    auto verify_search_box = [&](int nav_idx,
                                 const std::string& page_key,
                                 const std::string& input_key,
                                 bool has_split,
                                 const std::string& sample_text) {
        const std::string scope = "SearchBox[" + input_key + "]";
        store->active_nav = nav_idx;
        ui.Pump();

        if (ui.Find(UiSelector::Key("store-search-clear")).Exists()) {
            ctx.Flag(scope, "External store-search-clear button should not exist");
        }

        const auto field_q = ui.Find(UiSelector::Key(input_key));
        if (!field_q.Exists()) {
            ctx.Flag(scope, "Missing search input key: " + input_key);
            return;
        }
        if (field_q.One().value.value_or("") != "") {
            ctx.Flag(scope, "Expected initial empty value, got '" + field_q.One().value.value_or("") + "'");
        }

        const auto clear_q = ui.FindSemantics(UiSemanticSelector::AllOf(
            UiSemanticSelector::Role(SemanticRole::Button),
            UiSemanticSelector::Label("清空搜索")
        ));
        if (clear_q.Exists()) {
            ctx.Flag(scope, "Clear button semantic node should be hidden when input is empty");
        }

        field_q.Tap();
        field_q.EnterText(sample_text);
        if (field_q.One().value.value_or("") != sample_text) {
            ctx.Flag(scope, "EnterText failed: expected '" + sample_text + "', got '" +
                                field_q.One().value.value_or("") + "'");
            return;
        }
        if (clear_q.Count() != 1) {
            ctx.Flag(scope, "Expected 1 clear button semantic node after text entry, got " +
                                std::to_string(clear_q.Count()));
            return;
        }

        const Rect field_bounds = field_q.One().bounds;
        const Rect clear_bounds = clear_q.One().bounds;
        if (clear_bounds.x < field_bounds.x - 0.5F ||
            clear_bounds.y < field_bounds.y - 0.5F ||
            clear_bounds.x + clear_bounds.width > field_bounds.x + field_bounds.width + 0.5F ||
            clear_bounds.y + clear_bounds.height > field_bounds.y + field_bounds.height + 0.5F) {
            ctx.Flag(scope, "Clear button bounds " + FmtRect(clear_bounds) +
                                " not contained in search input bounds " + FmtRect(field_bounds));
        }
        if (clear_bounds.x < field_bounds.x + field_bounds.width * 0.5F) {
            ctx.Flag(scope, "Clear button bounds " + FmtRect(clear_bounds) +
                                " not on trailing half of " + FmtRect(field_bounds));
        }
        if (std::abs(clear_bounds.width - clear_bounds.height) >= 0.5F ||
            clear_bounds.width < 24.0F || clear_bounds.width > 30.0F) {
            ctx.Flag(scope, "Unexpected clear button action size: " + FmtRect(clear_bounds));
        }

        AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "SearchPopulated[" + input_key + "]", page_key, has_split);

        const Point clear_center{
            clear_bounds.x + clear_bounds.width * 0.5F,
            clear_bounds.y + clear_bounds.height * 0.5F,
        };
        ui.SendPointer(PointerEvent{
            .type = PointerEventType::Move,
            .pointer_id = 50,
            .position = {0.0F, 0.0F},
            .device_kind = PointerDeviceKind::Mouse,
        });
        ui.Pump();
        const UiSnapshot before_hover = ui.CaptureSnapshot();

        ui.SendPointer(PointerEvent{
            .type = PointerEventType::Move,
            .pointer_id = 50,
            .position = clear_center,
            .device_kind = PointerDeviceKind::Mouse,
        });
        ui.Pump();
        const UiSnapshot after_hover = ui.CaptureSnapshot();
        if (before_hover == after_hover) {
            ctx.Flag(scope, "Hovering internal clear button did not update visual snapshot");
        }

        ui.TapAt(clear_center, {.device_kind = PointerDeviceKind::Mouse, .pointer_id = 51});
        ui.Pump();
        ui.SendPointer(PointerEvent{
            .type = PointerEventType::Move,
            .pointer_id = 50,
            .position = {0.0F, 0.0F},
            .device_kind = PointerDeviceKind::Mouse,
        });
        ui.Pump();

        if (field_q.One().value.value_or("") != "") {
            ctx.Flag(scope, "Tapping internal clear button did not clear input text (got '" +
                                field_q.One().value.value_or("") + "')");
        }
        if (clear_q.Exists()) {
            ctx.Flag(scope, "Clear button semantic node still exists after clearing input");
        }

        AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "SearchCleared[" + input_key + "]", page_key, has_split);
    };

    verify_search_box(2, "page-store", "store-search-input", true, "gcc");
    verify_search_box(1, "page-packages", "packages-filter-input", true, "llvm");
    verify_search_box(3, "page-subos", "subos-shim-filter-input", true, "gcc");
    store->luban_tab = 0;
    verify_search_box(4, "page-luban", "luban-filter-input", true, "dev");

    store->SetThemeMode(ThemeMode::Dark);
    ui.Pump();
    verify_search_box(2, "page-store", "store-search-input", true, "rust");
    store->SetThemeMode(ThemeMode::Auto);
    ui.Pump();
}

void TestThemeAndDisabledControls(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    const auto srgb_channel = [](float c) -> float {
        return (c <= 0.04045F) ? (c / 12.92F) : std::pow((c + 0.055F) / 1.055F, 2.4F);
    };
    const auto srgb_luminance = [&](Color c) -> float {
        return 0.2126F * srgb_channel(c.red) +
               0.7152F * srgb_channel(c.green) +
               0.0722F * srgb_channel(c.blue);
    };
    const auto contrast_ratio = [&](Color a, Color b) -> float {
        const float l1 = srgb_luminance(a);
        const float l2 = srgb_luminance(b);
        return (std::max(l1, l2) + 0.05F) / (std::min(l1, l2) + 0.05F);
    };
    const auto blend_over = [](Color fg, float opacity, Color bg) -> Color {
        const float a = std::clamp(fg.alpha * opacity, 0.0F, 1.0F);
        return Color{
            fg.red * a + bg.red * (1.0F - a),
            fg.green * a + bg.green * (1.0F - a),
            fg.blue * a + bg.blue * (1.0F - a),
            1.0F,
        };
    };

    const auto presets = BrandPresets();
    for (const bool dark : {false, true}) {
        for (std::size_t idx = 0; idx < presets.size(); ++idx) {
            const ThemeSpec theme = AppThemeSpec(dark, presets[idx]);
            const StatusPalette sp = ResolveStatusPalette(theme);
            const std::string scope = std::string("ThemeToken[") + (dark ? "Dark" : "Light") +
                                      ",preset=" + std::to_string(idx) + "]";

            if (!(theme.colors.error == sp.danger)) {
                ctx.Flag(scope, "theme.colors.error does not match sp.danger");
            }
            if (!(sp.sidebar_bg == theme.colors.surface_container_low)) {
                ctx.Flag(scope, "sp.sidebar_bg does not match surface_container_low");
            }
            if (!(sp.card_bg == theme.colors.surface_container)) {
                ctx.Flag(scope, "sp.card_bg does not match surface_container");
            }
            if (theme.interactions.disabled_opacity < 0.48F) {
                ctx.Flag(scope, "disabled_opacity below 0.48F: " + std::to_string(theme.interactions.disabled_opacity));
            }

            const Color disabled_ghost = blend_over(
                sp.ghost_disabled_fg, theme.interactions.disabled_opacity, theme.colors.surface);
            const float disabled_contrast = contrast_ratio(disabled_ghost, theme.colors.surface);
            const float enabled_contrast = contrast_ratio(theme.colors.on_surface_variant, theme.colors.surface);
            if (disabled_contrast < 2.8F) {
                ctx.Flag(scope, "Disabled ghost button contrast too low: " + std::to_string(disabled_contrast));
            }
            if (disabled_contrast >= enabled_contrast) {
                ctx.Flag(scope, "Disabled ghost contrast (" + std::to_string(disabled_contrast) +
                                    ") not visually distinct from enabled (" + std::to_string(enabled_contrast) + ")");
            }

            const float danger_contrast = contrast_ratio(sp.on_danger_fill, sp.danger_fill);
            if (danger_contrast < 4.5F) {
                ctx.Flag(scope, "Danger fill contrast below 4.5:1: " + std::to_string(danger_contrast));
            }
        }
    }

    const auto saved_lines = store->console_lines.Get();
    ui.SetWindowMetrics({.viewport = {1180.0F, 760.0F}});
    store->active_nav = 2;
    store->console_lines = std::vector<std::string>{};
    store->SetThemeMode(ThemeMode::Light);
    ui.Pump();

    const auto copy_q = ui.FindSemantics(UiSemanticSelector::AllOf(
        UiSemanticSelector::Role(SemanticRole::Button),
        UiSemanticSelector::Label("复制")
    ));
    const auto clear_q = ui.FindSemantics(UiSemanticSelector::AllOf(
        UiSemanticSelector::Role(SemanticRole::Button),
        UiSemanticSelector::Label("清空")
    ));
    if (!copy_q.Exists() || copy_q.One().enabled) {
        ctx.Flag("TaskConsoleEmpty(Light)", "Expected disabled '复制' button when console_lines is empty");
    }
    if (!clear_q.Exists() || clear_q.One().enabled) {
        ctx.Flag("TaskConsoleEmpty(Light)", "Expected disabled '清空' button when console_lines is empty");
    }
    AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "TaskConsoleEmpty(Light)", "page-store", true);

    store->console_lines = std::vector<std::string>{"sample log line"};
    ui.Pump();
    if (!copy_q.Exists() || !copy_q.One().enabled) {
        ctx.Flag("TaskConsoleNonEmpty(Light)", "Expected enabled '复制' button when console_lines is non-empty");
    }
    if (!clear_q.Exists() || !clear_q.One().enabled) {
        ctx.Flag("TaskConsoleNonEmpty(Light)", "Expected enabled '清空' button when console_lines is non-empty");
    }

    store->console_lines = saved_lines;
    store->SetThemeMode(ThemeMode::Auto);
    ui.Pump();
}

void TestUnifiedRefreshStates(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    ui.SetWindowMetrics({.viewport = {1180.0F, 760.0F}});

    auto verify_in_place = [&](const std::string& scope, const std::string& key, auto&& set_loading) {
        auto q = ui.Find(UiSelector::Key(key));
        if (!q.Exists()) {
            ctx.Flag(scope, "Missing refresh control key: " + key);
            return;
        }
        const UiNodeInfo idle_info = q.One();
        if (q.Find(UiSelector::Enabled(false)).Exists() || !q.Find(UiSelector::Enabled(true)).Exists()) {
            ctx.Flag(scope, "Refresh control '" + key + "' expected enabled before loading");
        }
        if (!q.Find(UiSelector::Type<Image>()).Exists()) {
            ctx.Flag(scope, "Refresh control '" + key + "' missing refresh Image before loading");
        }

        set_loading(true);
        ui.Pump();

        auto q_loading = ui.Find(UiSelector::Key(key));
        if (!q_loading.Exists()) {
            ctx.Flag(scope, "Refresh control '" + key + "' disappeared while loading");
            set_loading(false);
            ui.Pump();
            return;
        }
        const UiNodeInfo loading_info = q_loading.One();
        if (!q_loading.Find(UiSelector::Enabled(false)).Exists()) {
            ctx.Flag(scope, "Refresh control '" + key + "' expected disabled interactive node while loading");
        }
        if (q_loading.Find(UiSelector::Type<Image>()).Exists()) {
            ctx.Flag(scope, "Refresh control '" + key + "' still rendering static icon while loading");
        }
        if (std::fabs(loading_info.bounds.x - idle_info.bounds.x) > 1.0F ||
            std::fabs(loading_info.bounds.width - idle_info.bounds.width) > 1.0F) {
            ctx.Flag(scope, "Refresh control '" + key + "' shifted/resized while loading: idle=" +
                                FmtRect(idle_info.bounds) + " loading=" + FmtRect(loading_info.bounds));
        }

        set_loading(false);
        ui.Pump();

        auto q_restored = ui.Find(UiSelector::Key(key));
        if (!q_restored.Exists() || q_restored.Find(UiSelector::Enabled(false)).Exists() ||
            !q_restored.Find(UiSelector::Type<Image>()).Exists()) {
            ctx.Flag(scope, "Refresh control '" + key + "' did not restore enabled state and icon after loading");
        }
    };

    // 1. Dashboard & TitleBar in Light and Dark modes
    store->active_nav = 0;
    for (const ThemeMode mode : {ThemeMode::Light, ThemeMode::Dark}) {
        const std::string mode_tag = (mode == ThemeMode::Light) ? "Light" : "Dark";
        store->SetThemeMode(mode);
        store->refreshing = false;
        ui.Pump();

        verify_in_place("UnifiedRefresh[" + mode_tag + " / title]", "title-refresh-btn", [&](bool v) {
            store->refreshing = v;
        });
        verify_in_place("UnifiedRefresh[" + mode_tag + " / dashboard]", "dashboard-refresh-btn", [&](bool v) {
            store->refreshing = v;
        });

        store->refreshing = true;
        ui.Pump();
        AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "DashboardRefreshing[" + mode_tag + "]", "page-dashboard");
        store->refreshing = false;
        ui.Pump();
    }

    store->SetThemeMode(ThemeMode::Light);

    // 2. Indexes page
    store->active_nav = 5;
    ui.Pump();
    verify_in_place("UnifiedRefresh / Indexes(local)", "indexes-refresh-btn", [&](bool v) {
        store->loading_indexes = v;
    });
    verify_in_place("UnifiedRefresh / Indexes(versions)", "indexes-refresh-btn", [&](bool v) {
        store->loading_index_versions = v;
    });

    // 3. Settings Tab 0 / 1 / 2
    store->active_nav = 6;
    store->settings_tab = 0;
    ui.Pump();
    const AppUpdateState saved_upd = store->app_update_state.Get();
    try {
        (void)ui.Find(UiSelector::Key("settings-scroll")).ScrollUntil(UiSelector::Key("about-check-update-btn"));
        ui.Pump();
    } catch (const UiTestFailure& ex) {
        ctx.Flag("UnifiedRefresh / SettingsAbout", std::string("ScrollUntil about-check-update-btn failed: ") + ex.what());
    }
    verify_in_place("UnifiedRefresh / SettingsAbout", "about-check-update-btn", [&](bool v) {
        AppUpdateState next = saved_upd;
        next.phase = v ? UpdatePhase::Checking : UpdatePhase::Available;
        store->app_update_state = std::move(next);
    });
    store->app_update_state = saved_upd;
    ui.Find(UiSelector::Key("settings-scroll")).ScrollBy({0.0F, -2000.0F});
    ui.Pump();

    store->settings_tab = 1;
    ui.Pump();
    verify_in_place("UnifiedRefresh / SettingsConfig", "settings-refresh-cfg-btn", [&](bool v) {
        store->loading_xlings_config = v;
    });
    try {
        (void)ui.Find(UiSelector::Key("settings-scroll")).ScrollUntil(UiSelector::Key("settings-refresh-themes-btn"));
        ui.Pump();
    } catch (const UiTestFailure& ex) {
        ctx.Flag("UnifiedRefresh / SettingsThemes", std::string("ScrollUntil settings-refresh-themes-btn failed: ") + ex.what());
    }
    verify_in_place("UnifiedRefresh / SettingsThemes", "settings-refresh-themes-btn", [&](bool v) {
        store->loading_config_themes = v;
    });
    ui.Find(UiSelector::Key("settings-scroll")).ScrollBy({0.0F, -2000.0F});
    ui.Pump();

    store->settings_tab = 2;
    ui.Pump();
    verify_in_place("UnifiedRefresh / SettingsNetwork", "settings-refresh-net-btn", [&](bool v) {
        store->loading_network = v;
    });
    try {
        (void)ui.Find(UiSelector::Key("settings-scroll")).ScrollUntil(UiSelector::Key("settings-refresh-profiles-btn"));
        ui.Pump();
    } catch (const UiTestFailure& ex) {
        ctx.Flag("UnifiedRefresh / SettingsProfiles", std::string("ScrollUntil settings-refresh-profiles-btn failed: ") + ex.what());
    }
    verify_in_place("UnifiedRefresh / SettingsProfiles", "settings-refresh-profiles-btn", [&](bool v) {
        store->loading_profiles = v;
    });
    ui.Find(UiSelector::Key("settings-scroll")).ScrollBy({0.0F, -2000.0F});
    ui.Pump();

    // 4. SubOS detail slot stability + events/policy refresh
    store->active_nav = 3;
    ui.Pump();
    if (SafeTap(ctx, "UnifiedRefresh / SubOS[Events]", ui.Find(UiSelector::Text("审计")), "审计")) {
        ui.Pump();
        auto find_detail_active_badge = [&]() -> std::optional<Rect> {
            std::optional<Rect> best;
            for (const auto& n : ui.Find(UiSelector::Text("当前活跃")).All()) {
                if (n.participates_in_layout && n.in_viewport && n.bounds.x > 500.0F) {
                    if (!best.has_value() || n.bounds.x > best->x) {
                        best = n.bounds;
                    }
                }
            }
            return best;
        };
        const std::optional<Rect> active_badge_before = find_detail_active_badge();
        if (!active_badge_before.has_value()) {
            ctx.Flag("UnifiedRefresh / SubOSDetailHeader", "Expected visible SubOS detail '当前活跃' badge");
        }
        verify_in_place("UnifiedRefresh / SubOSEvents", "subos-refresh-events-btn", [&](bool v) {
            store->loading_subos_detail = v;
        });
        if (active_badge_before.has_value()) {
            store->loading_subos_detail = true;
            ui.Pump();
            const std::optional<Rect> active_badge_after = find_detail_active_badge();
            if (!active_badge_after.has_value() ||
                std::fabs(active_badge_after->x - active_badge_before->x) > 1.0F) {
                ctx.Flag("UnifiedRefresh / SubOSDetailHeader",
                         "SubOS '当前活跃' badge shifted horizontally when loading_subos_detail turned true");
            }
            store->loading_subos_detail = false;
            ui.Pump();
        }
    }
    if (SafeTap(ctx, "UnifiedRefresh / SubOS[Policy]", ui.Find(UiSelector::Text("策略")), "策略")) {
        ui.Pump();
        verify_in_place("UnifiedRefresh / SubOSPolicy", "subos-refresh-policy-btn", [&](bool v) {
            store->loading_subos_governance = v;
        });
    }

    // 5. LubanOS outer refresh + detail refresh
    store->active_nav = 4;
    store->luban_tab = 0;
    ui.Pump();
    verify_in_place("UnifiedRefresh / Luban", "luban-refresh-btn", [&](bool v) {
        store->loading_luban = v;
    });
    if (SafeTap(ctx, "UnifiedRefresh / Luban[HistoryLogs]", ui.Find(UiSelector::Text("历史与日志")), "历史与日志")) {
        ui.Pump();
        verify_in_place("UnifiedRefresh / LubanDetail", "luban-refresh-detail-btn", [&](bool v) {
            store->loading_luban_detail = v;
        });
    }

    store->SetThemeMode(ThemeMode::Auto);
    store->active_nav = 0;
    store->settings_tab = 0;
    ui.Pump();
}

void TestLubanStackStudio(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    ui.SetWindowMetrics({.viewport = {1180.0F, 760.0F}});
    store->active_nav = 4;
    store->luban_tab = 2;
    ui.Pump();

    auto scroll_to = [&](const std::string& key) {
        try {
            (void)ui.Find(UiSelector::Key("luban-stack-scroll")).ScrollUntil(UiSelector::Key(key));
            ui.Pump();
        } catch (const UiTestFailure& ex) {
            ctx.Flag("LubanStackStudio", "ScrollUntil '" + key + "' failed: " + ex.what());
        }
    };

    if (!ui.Find(UiSelector::Key("luban-stack-editor")).Exists()) {
        ctx.Flag("LubanStackStudio", "Missing luban-stack-editor");
        return;
    }
    if (!ui.Find(UiSelector::Key("luban-stack-valid-badge")).Exists()) {
        ctx.Flag("LubanStackStudio", "Expected initial preset draft to validate");
    }
    AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "LubanStackStudio[Default]", "page-luban");

    if (SafeTap(ctx, "LubanStackStudio[Preset1]", ui.Find(UiSelector::Key("luban-stack-preset-1")), "luban-stack-preset-1")) {
        ui.Pump();
        if (!ui.Find(UiSelector::Key("luban-stack-valid-badge")).Exists()) {
            ctx.Flag("LubanStackStudio[Preset1]", "Preset 1 did not produce valid state");
        }
        scroll_to("luban-stack-apply-btn");
        if (!ui.Find(UiSelector::Key("luban-plan-act-dev-core-0")).Exists() ||
            !ui.Find(UiSelector::Key("luban-plan-act-agent-box-1")).Exists()) {
            ctx.Flag("LubanStackStudio[Preset1]", "Plan diff missing expected dev-core / agent-box actions");
        }
        if (ui.Find(UiSelector::Key("luban-stack-apply-btn")).Find(UiSelector::Enabled(false)).Exists()) {
            ctx.Flag("LubanStackStudio[Preset1]", "Apply button should be enabled for valid seeded draft");
        }
    }

    ui.Find(UiSelector::Key("luban-stack-scroll")).ScrollBy({0.0F, -2000.0F});
    ui.Pump();

    scroll_to("luban-stack-fmt-json");
    if (SafeTap(ctx, "LubanStackStudio[JSON]", ui.Find(UiSelector::Key("luban-stack-fmt-json")), "luban-stack-fmt-json")) {
        ui.Pump();
        const std::string val = ui.Find(UiSelector::Key("luban-stack-editor")).One().value.value_or("");
        if (val.empty() || val.front() != '{') {
            ctx.Flag("LubanStackStudio[JSON]", "Expected JSON conversion in luban-stack-editor");
        }
        if (!ui.Find(UiSelector::Key("luban-stack-valid-badge")).Exists()) {
            ctx.Flag("LubanStackStudio[JSON]", "Converted JSON failed validation");
        }
    }

    if (SafeTap(ctx, "LubanStackStudio[TOML]", ui.Find(UiSelector::Key("luban-stack-fmt-toml")), "luban-stack-fmt-toml")) {
        ui.Pump();
        const std::string val = ui.Find(UiSelector::Key("luban-stack-editor")).One().value.value_or("");
        if (val.find("[[environments]]") == std::string::npos) {
            ctx.Flag("LubanStackStudio[TOML]", "Expected TOML [[environments]] table in luban-stack-editor");
        }
        if (!ui.Find(UiSelector::Key("luban-stack-valid-badge")).Exists()) {
            ctx.Flag("LubanStackStudio[TOML]", "Converted TOML failed validation");
        }
    }

    if (SafeTap(ctx, "LubanStackStudio[ExportEnvs]",
                ui.Find(UiSelector::Key("luban-stack-export-envs-btn")),
                "luban-stack-export-envs-btn")) {
        ui.Pump();
        const std::string val = ui.Find(UiSelector::Key("luban-stack-editor")).One().value.value_or("");
        if (val.find("dev-core") == std::string::npos || val.find("agent-box") == std::string::npos) {
            ctx.Flag("LubanStackStudio[ExportEnvs]", "Export from current environments missed dev-core or agent-box");
        }
        if (!ui.Find(UiSelector::Key("luban-stack-valid-badge")).Exists()) {
            ctx.Flag("LubanStackStudio[ExportEnvs]", "Exported stack failed validation");
        }
    }

    scroll_to("luban-stack-editor");
    auto ed_q = ui.Find(UiSelector::Key("luban-stack-editor"));
    ed_q.Tap();
    ed_q.EnterText("\nbroken_line_without_equals");
    ui.Pump();
    if (!ui.Find(UiSelector::Key("luban-stack-errors")).Exists()) {
        ctx.Flag("LubanStackStudio[InvalidInput]", "Expected validation error banner on invalid TOML input");
    }
    scroll_to("luban-stack-apply-btn");
    if (!ui.Find(UiSelector::Key("luban-stack-apply-btn")).Find(UiSelector::Enabled(false)).Exists()) {
        ctx.Flag("LubanStackStudio[InvalidInput]", "Apply button should be disabled when manifest has syntax errors");
    }

    ui.Find(UiSelector::Key("luban-stack-scroll")).ScrollBy({0.0F, -2000.0F});
    ui.Pump();
    if (SafeTap(ctx, "LubanStackStudio[Restore]", ui.Find(UiSelector::Key("luban-stack-preset-0")), "luban-stack-preset-0")) {
        ui.Pump();
    }

    ui.SetWindowMetrics({.viewport = {880.0F, 560.0F}});
    ui.Pump();
    AuditCurrentScreen(ctx, ui, 880.0F, 560.0F, "LubanStackStudio[880x560]", "page-luban");

    ui.SetWindowMetrics({.viewport = {1180.0F, 760.0F}});
    store->active_nav = 0;
    store->luban_tab = 0;
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
    TestCrossPlatformShortcuts(ctx, ui, store);
    TestMacOsTitleBarMetrics(ctx, ui, store);
    TestStorePaginationAndErrorStates(ctx, ui, store);
    TestDuplicateDataNoDuplicateKeys(ctx, ui, store);
    TestSearchBoxes(ctx, ui, store);
    TestThemeAndDisabledControls(ctx, ui, store);
    TestUnifiedRefreshStates(ctx, ui, store);
    TestLubanStackStudio(ctx, ui, store);

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

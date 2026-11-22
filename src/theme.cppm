export module hxlings.theme;

import std;
import huxerui;

using namespace huxerui;

export namespace hxlings {

struct StatusPalette {
    Color success;
    Color success_bg;
    Color warning;
    Color warning_bg;
    Color danger;
    Color danger_bg;
    Color info;
    Color info_bg;
    Color muted;
    Color muted_bg;
    Color sidebar_bg;
    Color card_bg;
    Color card_elevated_bg;
    Color border_soft;
    Color terminal_bg;
    Color terminal_fg;
    Color terminal_muted;

    bool operator==(const StatusPalette&) const = default;
};

Color WithAlpha(Color color, float alpha) {
    color.alpha = alpha;
    return color;
}

Color ScaleRgb(Color color, float factor) {
    color.red = std::clamp(color.red * factor, 0.0F, 1.0F);
    color.green = std::clamp(color.green * factor, 0.0F, 1.0F);
    color.blue = std::clamp(color.blue * factor, 0.0F, 1.0F);
    return color;
}

std::array<Color, 6> BrandPresets() {
    return {
        Color::Rgb(47, 111, 237),   // Cobalt Blue (default)
        Color::Rgb(88, 86, 214),    // Indigo
        Color::Rgb(13, 148, 136),   // Teal
        Color::Rgb(22, 163, 74),    // Emerald
        Color::Rgb(217, 119, 6),    // Amber
        Color::Rgb(219, 39, 119),   // Rose
    };
}

Color DefaultBrandColor() {
    return BrandPresets().front();
}

ThemeSpec AppThemeSpec(bool dark, Color brand) {
    ThemeSpec spec = dark ? FlatDarkThemeSpec() : FlatLightThemeSpec();
    spec.colors.primary = brand;
    spec.colors.primary_container = dark ? WithAlpha(brand, 0.22F) : WithAlpha(brand, 0.13F);
    spec.colors.on_primary_container = dark ? Color::Rgb(219, 234, 254) : ScaleRgb(brand, 0.72F);
    spec.interactions.focus_ring = FocusRing{brand, 2.0F, 2.0F};
    if (dark) {
        spec.colors.background = Color::Rgb(15, 18, 24);
        spec.colors.surface = Color::Rgb(22, 27, 34);
        spec.colors.surface_container_low = Color::Rgb(19, 23, 30);
        spec.colors.surface_container = Color::Rgb(26, 32, 42);
        spec.colors.surface_container_high = Color::Rgb(33, 40, 52);
        spec.colors.surface_container_highest = Color::Rgb(43, 52, 66);
        spec.colors.on_surface = Color::Rgb(236, 240, 246);
        spec.colors.on_surface_variant = Color::Rgb(156, 168, 186);
        spec.colors.outline = Color::Rgb(62, 74, 94);
    } else {
        spec.colors.background = Color::Rgb(244, 246, 250);
        spec.colors.surface = Color::White();
        spec.colors.surface_container_low = Color::Rgb(249, 250, 252);
        spec.colors.surface_container = Color::White();
        spec.colors.surface_container_high = Color::Rgb(241, 244, 249);
        spec.colors.surface_container_highest = Color::Rgb(230, 235, 242);
        spec.colors.on_surface = Color::Rgb(24, 30, 41);
        spec.colors.on_surface_variant = Color::Rgb(95, 108, 128);
        spec.colors.outline = Color::Rgb(203, 213, 225);
    }
    return spec;
}

StatusPalette ResolveStatusPalette(const ThemeSpec& theme) {
    const bool dark = theme.brightness == ThemeBrightness::Dark;
    if (dark) {
        return StatusPalette{
            .success = Color::Rgb(74, 222, 128),
            .success_bg = Color::Rgb(22, 101, 52, 0.35F),
            .warning = Color::Rgb(251, 191, 36),
            .warning_bg = Color::Rgb(146, 64, 14, 0.38F),
            .danger = Color::Rgb(248, 113, 113),
            .danger_bg = Color::Rgb(153, 27, 27, 0.38F),
            .info = Color::Rgb(96, 165, 250),
            .info_bg = Color::Rgb(30, 64, 175, 0.35F),
            .muted = Color::Rgb(148, 163, 184),
            .muted_bg = Color::Rgb(51, 65, 85, 0.55F),
            .sidebar_bg = Color::Rgb(18, 22, 29),
            .card_bg = Color::Rgb(24, 29, 38),
            .card_elevated_bg = Color::Rgb(30, 36, 48),
            .border_soft = Color::Rgb(255, 255, 255, 0.09F),
            .terminal_bg = Color::Rgb(11, 14, 20),
            .terminal_fg = Color::Rgb(226, 232, 240),
            .terminal_muted = Color::Rgb(148, 163, 184),
        };
    }
    return StatusPalette{
        .success = Color::Rgb(21, 128, 61),
        .success_bg = Color::Rgb(220, 252, 231),
        .warning = Color::Rgb(180, 83, 9),
        .warning_bg = Color::Rgb(254, 243, 199),
        .danger = Color::Rgb(185, 28, 28),
        .danger_bg = Color::Rgb(254, 226, 226),
        .info = Color::Rgb(29, 78, 216),
        .info_bg = Color::Rgb(219, 234, 254),
        .muted = Color::Rgb(100, 116, 139),
        .muted_bg = Color::Rgb(226, 232, 240),
        .sidebar_bg = Color::Rgb(249, 250, 253),
        .card_bg = Color::White(),
        .card_elevated_bg = Color::Rgb(248, 250, 252),
        .border_soft = Color::Rgb(15, 23, 42, 0.09F),
        .terminal_bg = Color::Rgb(17, 22, 31),
        .terminal_fg = Color::Rgb(229, 237, 247),
        .terminal_muted = Color::Rgb(148, 163, 184),
    };
}

} // namespace hxlings

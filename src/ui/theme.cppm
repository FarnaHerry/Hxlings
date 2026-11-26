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
    Color danger_fill;
    Color on_danger_fill;
    Color info;
    Color info_bg;
    Color muted;
    Color muted_bg;
    Color sidebar_bg;
    Color card_bg;
    Color card_elevated_bg;
    Color border_soft;
    Color hover_subtle;
    Color press_subtle;
    Color hover_on_accent;
    Color press_on_accent;
    Color hover_primary;
    Color press_primary;
    Color ghost_disabled_fg;
    Color icon_muted;
    Color shadow_elevated;
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

Color MixColor(Color a, Color b, float t) {
    const float k = std::clamp(t, 0.0F, 1.0F);
    return Color{
        a.red + (b.red - a.red) * k,
        a.green + (b.green - a.green) * k,
        a.blue + (b.blue - a.blue) * k,
        a.alpha + (b.alpha - a.alpha) * k,
    };
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
    spec.colors.primary_container = dark ? WithAlpha(brand, 0.22F) : WithAlpha(brand, 0.12F);
    spec.colors.on_primary_container = dark ? MixColor(brand, Color::White(), 0.78F) : ScaleRgb(brand, 0.72F);
    spec.interactions.focus_ring = FocusRing{brand, 2.0F, 2.0F};
    spec.interactions.disabled_opacity = 0.52F;
    if (dark) {
        spec.colors.background = Color::Rgb(15, 18, 24);
        spec.colors.surface = Color::Rgb(22, 27, 34);
        spec.colors.surface_container_low = Color::Rgb(18, 22, 29);
        spec.colors.surface_container = Color::Rgb(24, 30, 40);
        spec.colors.surface_container_high = Color::Rgb(33, 41, 54);
        spec.colors.surface_container_highest = Color::Rgb(45, 55, 72);
        spec.colors.on_surface = Color::Rgb(236, 240, 246);
        spec.colors.on_surface_variant = Color::Rgb(156, 168, 186);
        spec.colors.outline = Color::Rgb(62, 74, 94);
        spec.colors.error = Color::Rgb(248, 113, 113);
    } else {
        spec.colors.background = Color::Rgb(244, 246, 250);
        spec.colors.surface = Color::White();
        spec.colors.surface_container_low = Color::Rgb(248, 250, 252);
        spec.colors.surface_container = Color::White();
        spec.colors.surface_container_high = Color::Rgb(241, 245, 249);
        spec.colors.surface_container_highest = Color::Rgb(226, 232, 240);
        spec.colors.on_surface = Color::Rgb(15, 23, 42);
        spec.colors.on_surface_variant = Color::Rgb(71, 85, 105);
        spec.colors.outline = Color::Rgb(203, 213, 225);
        spec.colors.error = Color::Rgb(185, 28, 28);
    }
    return spec;
}

StatusPalette ResolveStatusPalette(const ThemeSpec& theme) {
    const bool dark = theme.brightness == ThemeBrightness::Dark;
    const Color hover_on_accent = Color::Rgb(255, 255, 255, 0.14F);
    const Color press_on_accent = Color::Rgb(255, 255, 255, 0.24F);
    if (dark) {
        return StatusPalette{
            .success = Color::Rgb(74, 222, 128),
            .success_bg = Color::Rgb(22, 101, 52, 0.35F),
            .warning = Color::Rgb(251, 191, 36),
            .warning_bg = Color::Rgb(146, 64, 14, 0.38F),
            .danger = theme.colors.error,
            .danger_bg = Color::Rgb(153, 27, 27, 0.38F),
            .danger_fill = Color::Rgb(220, 38, 38),
            .on_danger_fill = Color::White(),
            .info = Color::Rgb(96, 165, 250),
            .info_bg = Color::Rgb(30, 64, 175, 0.35F),
            .muted = theme.colors.on_surface_variant,
            .muted_bg = WithAlpha(theme.colors.surface_container_highest, 0.78F),
            .sidebar_bg = theme.colors.surface_container_low,
            .card_bg = theme.colors.surface_container,
            .card_elevated_bg = theme.colors.surface_container_high,
            .border_soft = Color::Rgb(255, 255, 255, 0.09F),
            .hover_subtle = Color::Rgb(255, 255, 255, 0.07F),
            .press_subtle = Color::Rgb(255, 255, 255, 0.13F),
            .hover_on_accent = hover_on_accent,
            .press_on_accent = press_on_accent,
            .hover_primary = WithAlpha(theme.colors.primary, 0.14F),
            .press_primary = WithAlpha(theme.colors.primary, 0.22F),
            .ghost_disabled_fg = theme.colors.on_surface_variant,
            .icon_muted = WithAlpha(theme.colors.on_surface_variant, 0.60F),
            .shadow_elevated = Color::Rgb(0, 0, 0, 0.42F),
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
        .danger = theme.colors.error,
        .danger_bg = Color::Rgb(254, 226, 226),
        .danger_fill = theme.colors.error,
        .on_danger_fill = Color::White(),
        .info = Color::Rgb(29, 78, 216),
        .info_bg = Color::Rgb(219, 234, 254),
        .muted = theme.colors.on_surface_variant,
        .muted_bg = theme.colors.surface_container_highest,
        .sidebar_bg = theme.colors.surface_container_low,
        .card_bg = theme.colors.surface_container,
        .card_elevated_bg = theme.colors.surface_container_low,
        .border_soft = Color::Rgb(15, 23, 42, 0.10F),
        .hover_subtle = Color::Rgb(15, 23, 42, 0.06F),
        .press_subtle = Color::Rgb(15, 23, 42, 0.11F),
        .hover_on_accent = hover_on_accent,
        .press_on_accent = press_on_accent,
        .hover_primary = WithAlpha(theme.colors.primary, 0.10F),
        .press_primary = WithAlpha(theme.colors.primary, 0.18F),
        .ghost_disabled_fg = theme.colors.on_surface,
        .icon_muted = WithAlpha(theme.colors.on_surface_variant, 0.65F),
        .shadow_elevated = Color::Rgb(15, 23, 42, 0.18F),
        .terminal_bg = Color::Rgb(17, 22, 31),
        .terminal_fg = Color::Rgb(229, 237, 247),
        .terminal_muted = Color::Rgb(148, 163, 184),
    };
}

} // namespace hxlings

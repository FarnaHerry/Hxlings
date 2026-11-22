export module hxlings.components;

import std;
import huxerui;
import hxlings.theme;
import hxlings.icons;
import hxlings.shortcuts;

using namespace huxerui;

export namespace hxlings {

enum class ButtonTone {
    Primary,
    Secondary,
    Danger,
    Ghost,
};

[[huxerui::composable]]
View ActionButton(
    std::string label,
    ButtonTone tone,
    std::function<void()> on_click,
    bool enabled = true,
    std::optional<VectorAsset> icon = std::nullopt,
    bool compact = false
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    Color bg = theme.colors.primary;
    Color fg = theme.colors.on_primary;
    Color border_col = Color::Transparent();
    float border_w = 0.0F;
    Color hover_overlay = Color::Rgb(255, 255, 255, 0.14F);
    Color press_overlay = Color::Rgb(255, 255, 255, 0.24F);

    const bool dark = theme.brightness == ThemeBrightness::Dark;

    switch (tone) {
        case ButtonTone::Primary:
            break;
        case ButtonTone::Secondary:
            bg = theme.colors.surface_container_high;
            fg = theme.colors.on_surface;
            border_col = sp.border_soft;
            border_w = 1.0F;
            hover_overlay = dark ? Color::Rgb(255, 255, 255, 0.08F) : Color::Rgb(15, 23, 42, 0.06F);
            press_overlay = dark ? Color::Rgb(255, 255, 255, 0.14F) : Color::Rgb(15, 23, 42, 0.12F);
            break;
        case ButtonTone::Danger:
            bg = sp.danger;
            fg = Color::White();
            break;
        case ButtonTone::Ghost:
            bg = Color::Transparent();
            fg = theme.colors.on_surface_variant;
            hover_overlay = dark ? Color::Rgb(255, 255, 255, 0.08F) : Color::Rgb(15, 23, 42, 0.06F);
            press_overlay = dark ? Color::Rgb(255, 255, 255, 0.14F) : Color::Rgb(15, 23, 42, 0.12F);
            break;
    }

    const Color effective_fg = enabled ? fg : WithAlpha(fg, 0.45F);
    const Color effective_bg = enabled ? bg : WithAlpha(bg, 0.45F);

    std::vector<View> items;
    if (icon.has_value()) {
        const float sz = compact ? 14.0F : 16.0F;
        items.push_back(
            Image(*icon)
                .Fit(ImageFit::Contain)
                .Align(HorizontalAlignment::Center, VerticalAlignment::Center)
                .Tint(effective_fg)
                .With(Frame{.width = sz, .height = sz})
        );
    }
    if (!label.empty()) {
        items.push_back(
            Text(label, TextRole::Label)
                .Style(TextStyle{
                    .font = Font::System(compact ? 12.5F : 13.5F).WithWeight(FontWeight::Medium),
                    .foreground = effective_fg,
                })
        );
    }

    const EdgeInsets pad = compact
                               ? EdgeInsets::Symmetric(10.0F, 5.0F)
                               : EdgeInsets::Symmetric(14.0F, 8.0F);

    Indication ind{
        .hover = IndicationLayer{.fill = hover_overlay},
        .press = IndicationLayer{.fill = press_overlay},
    };

    return Row(std::move(items))
        .With(
            Padding(pad),
            Spacing(6.0F),
            CrossAlign(CrossAxisAlignment::Center),
            Background(effective_bg),
            Border{border_col, border_w},
            CornerRadius(8.0F),
            std::move(ind),
            Focusable(enabled),
            Enabled(enabled),
            PointerCursor(enabled ? PointerCursorKind::Hand : PointerCursorKind::Default),
            Semantics{.role = SemanticRole::Button, .label = label.empty() ? std::string("Action") : label}
        )
        .OnClick(std::move(on_click));
}

[[huxerui::composable]]
View StatusBadge(std::string label, Color fg, Color bg) {
    return Row {
        Text(std::move(label), TextRole::Label)
            .Style(TextStyle{
                .font = Font::System(11.5F).WithWeight(FontWeight::SemiBold),
                .foreground = fg,
            }),
    }.With(
        Padding(EdgeInsets::Symmetric(8.0F, 3.0F)),
        Background(bg),
        CornerRadius(999.0F),
        CrossAlign(CrossAxisAlignment::Center)
    );
}

[[huxerui::composable]]
View Card(View content, float padding = 16.0F) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    return Column {
        std::move(content),
    }.With(
        Padding(padding),
        Background(sp.card_bg),
        Border{sp.border_soft, 1.0F},
        CornerRadius(12.0F),
        CrossAlign(CrossAxisAlignment::Stretch)
    );
}

[[huxerui::composable]]
View StatCard(
    std::string title,
    std::string value,
    std::string subtitle,
    VectorAsset icon,
    Color accent,
    std::function<void()> on_click = {}
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const bool clickable = static_cast<bool>(on_click);

    View body = Column {
        Row {
            Text(std::move(title), TextRole::Label)
                .Style(TextStyle{
                    .font = Font::System(13.0F).WithWeight(FontWeight::Medium),
                    .foreground = theme.colors.on_surface_variant,
                }),
            Spacer{}.With(Grow(1.0F)),
            Row {
                Image(std::move(icon))
                    .Fit(ImageFit::Contain)
                    .Align(HorizontalAlignment::Center, VerticalAlignment::Center)
                    .Tint(accent)
                    .With(Frame{.width = 16.0F, .height = 16.0F}),
            }.With(
                Padding(6.0F),
                Background(WithAlpha(accent, 0.14F)),
                CornerRadius(8.0F)
            ),
        }.With(CrossAlign(CrossAxisAlignment::Center)),
        Text(std::move(value), TextRole::Title)
            .Style(TextStyle{
                .font = Font::System(24.0F).WithWeight(FontWeight::Bold),
                .foreground = theme.colors.on_surface,
            }),
        Text(std::move(subtitle), TextRole::Label)
            .Style(TextStyle{
                .font = Font::System(12.0F),
                .foreground = theme.colors.on_surface_variant,
            }),
    }.With(
        Padding(16.0F),
        Spacing(8.0F),
        Background(sp.card_bg),
        Border{sp.border_soft, 1.0F},
        CornerRadius(12.0F),
        CrossAlign(CrossAxisAlignment::Stretch)
    );

    if (clickable) {
        return std::move(body)
            .With(
                Focusable(true),
                PointerCursor(PointerCursorKind::Hand)
            )
            .OnClick(std::move(on_click));
    }
    return body;
}

[[huxerui::composable]]
View SectionHeader(std::string title, std::string subtitle = {}, View trailing = {}) {
    const ThemeSpec& theme = UseTheme();
    std::vector<View> text_col;
    text_col.push_back(
        Text(std::move(title), TextRole::Title)
            .Style(TextStyle{
                .font = Font::System(16.0F).WithWeight(FontWeight::SemiBold),
                .foreground = theme.colors.on_surface,
            })
    );
    if (!subtitle.empty()) {
        text_col.push_back(
            Text(std::move(subtitle), TextRole::Label)
                .Style(TextStyle{
                    .font = Font::System(12.5F),
                    .foreground = theme.colors.on_surface_variant,
                })
        );
    }

    return Row {
        Column(std::move(text_col)).With(Spacing(3.0F), Grow(1.0F)),
        std::move(trailing),
    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center));
}

[[huxerui::composable]]
View KeyValueRow(
    std::string label,
    std::string value,
    bool highlight = false,
    bool monospace = false
) {
    const ThemeSpec& theme = UseTheme();
    const Font val_font = monospace
                              ? Font::Monospace(12.5F)
                              : Font::System(13.0F).WithWeight(highlight ? FontWeight::SemiBold : FontWeight::Regular);
    return Row {
        Text(std::move(label), TextRole::Label)
            .Style(TextStyle{
                .font = Font::System(12.5F).WithWeight(FontWeight::Medium),
                .foreground = theme.colors.on_surface_variant,
            })
            .With(Frame{.width = 140.0F}),
        SelectionArea(
            Text(value.empty() ? std::string("—") : std::move(value), TextRole::Body)
                .Style(TextStyle{
                    .font = val_font,
                    .foreground = highlight ? theme.colors.primary : theme.colors.on_surface,
                })
        ).With(Grow(1.0F)),
    }.With(Spacing(12.0F), Padding(EdgeInsets::Symmetric(0.0F, 4.0F)), CrossAlign(CrossAxisAlignment::Start));
}

[[huxerui::composable]]
View EmptyState(VectorAsset icon, std::string title, std::string subtitle = {}) {
    const ThemeSpec& theme = UseTheme();
    std::vector<View> items;
    items.push_back(
        Image(std::move(icon))
            .Fit(ImageFit::Contain)
            .Tint(WithAlpha(theme.colors.on_surface_variant, 0.55F))
            .With(Frame{.width = 40.0F, .height = 40.0F})
    );
    items.push_back(
        Text(std::move(title), TextRole::Title)
            .Style(TextStyle{
                .font = Font::System(15.0F).WithWeight(FontWeight::SemiBold),
                .foreground = theme.colors.on_surface,
            })
    );
    if (!subtitle.empty()) {
        items.push_back(
            Text(std::move(subtitle), TextRole::Body)
                .Style(TextStyle{
                    .font = Font::System(13.0F),
                    .foreground = theme.colors.on_surface_variant,
                })
                .Align(TextAlign::Center)
        );
    }

    return Column(std::move(items)).With(
        Grow(1.0F),
        Padding(28.0F),
        Spacing(10.0F),
        MainAlign(MainAxisAlignment::Center),
        CrossAlign(CrossAxisAlignment::Center)
    );
}

[[huxerui::composable]]
View CodeBox(std::string text, float min_height = 110.0F, float max_height = 250.0F) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const std::string display = text.empty() ? std::string("(暂无输出)") : std::move(text);

    return Column {
        ScrollView(
            SelectionArea(
                Text(display, TextRole::Body)
                    .Style(TextStyle{
                        .font = Font::Monospace(12.0F),
                        .foreground = text.empty() ? sp.terminal_muted : sp.terminal_fg,
                    })
            ).With(Padding(12.0F))
        ).ScrollAxis(Axis::Vertical)
            .With(Grow(1.0F), ScrollBar{}),
    }.With(
        Frame{.min_height = min_height, .max_height = max_height},
        Background(sp.terminal_bg),
        Border{sp.border_soft, 1.0F},
        CornerRadius(10.0F),
        ClipChildren(),
        CrossAlign(CrossAxisAlignment::Stretch)
    );
}

[[huxerui::composable]]
View ResizableSplit(
    View left,
    View right,
    float initial_left_width = 340.0F,
    float min_left_width = 240.0F,
    float max_left_width = 560.0F
) {
    auto frames = UseState(std::array<Frame, 2>{
        Frame{.width = initial_left_width, .min_width = min_left_width, .max_width = max_left_width},
        Frame{.min_width = 300.0F},
    });
    auto grows = UseState(std::array<Grow, 2>{Grow{0.0F}, Grow{1.0F}});

    const auto& cur_frames = frames.Get();
    const auto& cur_grows = grows.Get();

    return SplitLayout {
        View(left).With(cur_frames[0], cur_grows[0]).Key("split-left"),
        View(right).With(cur_frames[1], cur_grows[1]).Key("split-right"),
    }.Orientation(Axis::Horizontal)
        .OnChanged([frames, grows, min_left_width, max_left_width](const std::vector<SplitChange>& changes) mutable {
            auto next_frames = frames.Get();
            auto next_grows = grows.Get();
            for (const auto& ch : changes) {
                if (ch.index < 2) {
                    next_frames[ch.index] = ch.frame;
                    if (ch.index == 0 && next_frames[0].width.has_value()) {
                        next_frames[0].width = std::clamp(*next_frames[0].width, min_left_width, max_left_width);
                        next_frames[0].min_width = min_left_width;
                        next_frames[0].max_width = max_left_width;
                    }
                    next_grows[ch.index] = ch.grow;
                }
            }
            frames = std::move(next_frames);
            grows = std::move(next_grows);
        })
        .With(Grow(1.0F));
}

[[huxerui::composable]]
View DialogCardShell(std::string title, View body, View actions, float width = 440.0F) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    return Column {
        Text(std::move(title), TextRole::Title)
            .Style(TextStyle{
                .font = Font::System(18.0F).WithWeight(FontWeight::Bold),
                .foreground = theme.colors.on_surface,
            }),
        std::move(body),
        std::move(actions),
    }.With(
        Frame{.width = width},
        Padding(22.0F),
        Spacing(16.0F),
        Background(theme.colors.surface),
        Border{sp.border_soft, 1.0F},
        CornerRadius(14.0F),
        Shadow{Color::Rgb(0, 0, 0, 0.28F), {}, 24.0F, 0.0F},
        CrossAlign(CrossAxisAlignment::Stretch)
    );
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

} // namespace hxlings

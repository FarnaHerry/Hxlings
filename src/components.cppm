export module hxlings.components;

import std;
import huxerui;
import xlings.process;
import xlings.parser;
import hxlings.theme;
import hxlings.icons;
import hxlings.shortcuts;
import hxlings.store;

using namespace huxerui;

namespace hxlings::detail {

TextFieldStyle BuildSearchTextFieldStyle(const ThemeSpec& theme) {
    const auto scale_alpha = [](Color c, float factor) {
        c.alpha *= factor;
        return c;
    };
    const Color placeholder = scale_alpha(theme.colors.on_surface, 0.55F);
    const Color border = scale_alpha(theme.colors.on_surface, 0.4F);
    const Color hovered_border = scale_alpha(theme.colors.on_surface, 0.7F);
    const Color disabled_content = scale_alpha(theme.colors.on_surface, theme.interactions.disabled_opacity);
    const Color disabled_border = scale_alpha(border, theme.interactions.disabled_opacity);

    return TextFieldStyle{
        .variant = TextFieldVariant::Outlined,
        .show_label = true,
        .standard =
            {
                .background = Color::Transparent(),
                .border = border,
                .hovered_border = hovered_border,
                .focused_border = theme.colors.primary,
                .disabled_border = disabled_border,
                .corner_radii = {},
                .minimum_height = 40.0F,
            },
        .filled =
            {
                .background = theme.colors.surface_container_highest,
                .border = border,
                .hovered_border = hovered_border,
                .focused_border = theme.colors.primary,
                .disabled_border = disabled_border,
                .corner_radii = CornerRadii::Top(theme.shapes.extra_small + 2.0F),
                .minimum_height = 44.0F,
            },
        .outlined =
            {
                .background = Color::Transparent(),
                .border = border,
                .hovered_border = hovered_border,
                .focused_border = theme.colors.primary,
                .disabled_border = disabled_border,
                .corner_radii = CornerRadii{theme.shapes.extra_small + 2.0F},
                .minimum_height = 36.0F,
            },
        .text_style =
            TextStyle{Font::FromFamily(theme.typography.family, theme.typography.body_medium), theme.colors.on_surface},
        .label_style =
            TextStyle{Font::FromFamily(theme.typography.family, theme.typography.body_medium), placeholder},
        .floating_label_style =
            TextStyle{Font::FromFamily(theme.typography.family, theme.typography.body_small), placeholder},
        .placeholder_style =
            TextStyle{Font::FromFamily(theme.typography.family, theme.typography.body_medium), placeholder},
        .disabled_text = disabled_content,
        .focused_label = theme.colors.primary,
        .disabled_label = disabled_content,
        .error_label = theme.colors.error,
        .disabled_placeholder = disabled_content,
        .disabled_supporting_text = disabled_content,
        .leading_icon = placeholder,
        .focused_leading_icon = theme.colors.primary,
        .disabled_leading_icon = disabled_content,
        .error_leading_icon = theme.colors.error,
        .trailing_icon = theme.colors.on_surface_variant,
        .focused_trailing_icon = theme.colors.on_surface_variant,
        .disabled_trailing_icon = disabled_content,
        .error_trailing_icon = theme.colors.error,
        .selection =
            Color{
                theme.colors.primary.red,
                theme.colors.primary.green,
                theme.colors.primary.blue,
                0.22F,
            },
        .caret = theme.colors.primary,
        .error_caret = theme.colors.error,
        .composition = theme.colors.primary,
        .caret_width = 2.0F,
        .border_width = 1.0F,
        .focused_border_width = 2.0F,
        .padding = EdgeInsets::Symmetric(10.0F, theme.spacing.small),
        .leading_icon_size = 18.0F,
        .trailing_icon_size = 14.0F,
        .icon_spacing = theme.spacing.small,
        .label_spacing = theme.spacing.extra_small,
        .label_cutout_padding = theme.spacing.extra_small,
        .label_animation_duration = theme.motion.reduced_motion ? 0.0 : theme.motion.fast,
        .caret_blink_interval = theme.motion.reduced_motion ? 0.0 : 0.5,
        .validation_error = theme.colors.error,
        .validation_border_width = 1.0F,
        .focused_validation_border_width = 2.0F,
        .validation_text_style =
            TextStyle{Font::FromFamily(theme.typography.family, theme.typography.body_small), theme.colors.error},
        .validation_spacing = theme.spacing.extra_small,
    };
}

IconButtonStyle BuildSearchClearActionStyle(const ThemeSpec& theme) {
    Color disabled_fg = theme.colors.on_surface;
    disabled_fg.alpha *= theme.interactions.disabled_opacity;
    return IconButtonStyle{
        .foreground = theme.colors.on_surface_variant,
        .disabled_foreground = disabled_fg,
        .icon_size = 14.0F,
        .minimum_interactive_size = 26.0F,
        .state_layer_size = 24.0F,
        .corner_radius = theme.shapes.full,
        .indication = std::nullopt,
    };
}

} // namespace hxlings::detail

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
    Color hover_overlay = sp.hover_on_accent;
    Color press_overlay = sp.press_on_accent;

    switch (tone) {
        case ButtonTone::Primary:
            break;
        case ButtonTone::Secondary:
            bg = theme.colors.surface_container_high;
            fg = theme.colors.on_surface;
            border_col = sp.border_soft;
            border_w = 1.0F;
            hover_overlay = sp.hover_subtle;
            press_overlay = sp.press_subtle;
            break;
        case ButtonTone::Danger:
            bg = sp.danger_fill;
            fg = sp.on_danger_fill;
            break;
        case ButtonTone::Ghost:
            bg = Color::Transparent();
            fg = enabled ? theme.colors.on_surface_variant : sp.ghost_disabled_fg;
            hover_overlay = sp.hover_subtle;
            press_overlay = sp.press_subtle;
            break;
    }

    std::vector<View> items;
    if (icon.has_value()) {
        const float sz = compact ? 14.0F : 16.0F;
        items.push_back(
            Image(*icon)
                .Fit(ImageFit::Contain)
                .Align(HorizontalAlignment::Center, VerticalAlignment::Center)
                .Tint(fg)
                .With(Frame{.width = sz, .height = sz})
        );
    }
    if (!label.empty()) {
        items.push_back(
            Text(label, TextRole::Label)
                .Style(TextStyle{
                    .font = Font::System(compact ? 12.5F : 13.5F).WithWeight(FontWeight::Medium),
                    .foreground = fg,
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
            Background(bg),
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

struct SearchBoxOptions {
    std::string placeholder{};
    std::optional<ViewHandle> focus_handle{};
    std::string input_key{};
    std::string clear_label = "清空搜索";
    std::function<void()> on_submitted{};
    std::function<void()> on_clear{};
};

[[huxerui::composable]]
View SearchBox(State<TextEditingValue> value, SearchBoxOptions options = {}) {
    const ThemeSpec& theme = UseTheme();
    ThemeDefinition overrides;
    overrides.Set(detail::BuildSearchTextFieldStyle(theme));
    overrides.Set(detail::BuildSearchClearActionStyle(theme));

    TextField field = TextField(value)
        .Placeholder(options.placeholder)
        .Variant(TextFieldVariant::Outlined)
        .LeadingIcon(icons::SearchIcon())
        .OnChanged([value](const TextEditingValue& next) mutable { value = next; });

    if (options.on_submitted) {
        field = std::move(field).OnSubmitted(options.on_submitted);
    }

    if (!value.Get().text.empty()) {
        const bool blank_label = std::ranges::all_of(options.clear_label, [](unsigned char ch) {
            return std::isspace(ch) != 0;
        });
        const std::string clear_label = blank_label ? std::string("清空搜索") : options.clear_label;
        field = std::move(field)
            .TrailingIcon(icons::CloseIcon(), clear_label)
            .OnTrailingIconClick([value, on_clear = options.on_clear]() mutable {
                value = TextEditingValue::FromText("");
                if (on_clear) {
                    on_clear();
                }
            });
    }

    if (options.focus_handle.has_value()) {
        field = std::move(field).With(*options.focus_handle);
    }
    if (!options.input_key.empty()) {
        field = std::move(field).Key(options.input_key);
    }

    return Theme(std::move(overrides), std::move(field));
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
View ExternalLinkRow(
    std::string url,
    std::string link_key = {}
) {
    const auto store = UseService<XlingsStore>();
    const ToastHandle toast = UseToast();
    const ApplicationHandle app = UseApplication();
    const auto clipboard = app.Clipboard();
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    const std::string normalized = NormalizeBrowserUrl(url);
    const std::string target = normalized.empty() ? std::move(url) : normalized;
    const bool valid = !normalized.empty();

    View link_btn = Row {
        Image(icons::ExternalLinkIcon())
            .Fit(ImageFit::Contain)
            .Tint(valid ? theme.colors.primary : sp.ghost_disabled_fg)
            .With(Frame{.width = 14.0F, .height = 14.0F}),
        Text(target.empty() ? std::string("—") : target, TextRole::Body)
            .Style(TextStyle{
                .font = Font::Monospace(12.5F),
                .foreground = valid ? theme.colors.primary : theme.colors.on_surface,
            })
            .With(Grow(1.0F)),
    }.With(
        Grow(1.0F),
        Spacing(6.0F),
        CrossAlign(CrossAxisAlignment::Center),
        Padding(EdgeInsets::Symmetric(6.0F, 3.0F)),
        CornerRadius(6.0F),
        Indication{
            .hover = IndicationLayer{.fill = sp.hover_primary},
            .press = IndicationLayer{.fill = sp.press_primary},
        },
        Focusable(valid),
        Enabled(valid),
        PointerCursor(valid ? PointerCursorKind::Hand : PointerCursorKind::Default),
        Semantics{.role = SemanticRole::Button, .label = "打开链接 " + target}
    ).OnClick([store, toast, target] {
        store->OpenUrl(toast, target);
    });
    if (!link_key.empty()) {
        link_btn = std::move(link_btn).Key(std::move(link_key));
    }

    View copy_btn = ActionButton(
        "",
        ButtonTone::Ghost,
        [clipboard, toast, target] {
            const bool copied = clipboard && clipboard->SupportsSynchronousText() && clipboard->WriteText(target);
            toast.Show(copied ? "已复制链接" : "复制到剪贴板失败");
        },
        !target.empty(),
        icons::CopyIcon(),
        true
    );

    return Row {
        std::move(link_btn),
        std::move(copy_btn),
    }.With(Spacing(4.0F), CrossAlign(CrossAxisAlignment::Center));
}

[[huxerui::composable]]
View KeyValueRow(
    std::string label,
    std::string value,
    bool highlight = false,
    bool monospace = false,
    std::string link_key = {}
) {
    const ThemeSpec& theme = UseTheme();
    const Font val_font = monospace
                              ? Font::Monospace(12.5F)
                              : Font::System(13.0F).WithWeight(highlight ? FontWeight::SemiBold : FontWeight::Regular);
    const bool is_url = IsSafeExternalUrl(value);
    const std::string display_val = (!is_url && IsIsoTimestampWithZone(value))
                                        ? FormatLocalTimestamp(value)
                                        : value;
    View value_view = is_url
                          ? ExternalLinkRow(std::move(value), std::move(link_key)).With(Grow(1.0F))
                          : SelectionArea(
                                Text(display_val.empty() ? std::string("—") : display_val, TextRole::Body)
                                    .Style(TextStyle{
                                        .font = val_font,
                                        .foreground = highlight ? theme.colors.primary : theme.colors.on_surface,
                                    })
                            ).With(Grow(1.0F));
    return Row {
        Text(std::move(label), TextRole::Label)
            .Style(TextStyle{
                .font = Font::System(12.5F).WithWeight(FontWeight::Medium),
                .foreground = theme.colors.on_surface_variant,
            })
            .With(Frame{.width = 140.0F}),
        std::move(value_view),
    }.With(Spacing(12.0F), Padding(EdgeInsets::Symmetric(0.0F, 4.0F)), CrossAlign(CrossAxisAlignment::Start));
}

[[huxerui::composable]]
View EmptyState(VectorAsset icon, std::string title, std::string subtitle = {}) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    std::vector<View> items;
    items.push_back(
        Image(std::move(icon))
            .Fit(ImageFit::Contain)
            .Tint(sp.icon_muted)
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
        Shadow{sp.shadow_elevated, {}, 24.0F, 0.0F},
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

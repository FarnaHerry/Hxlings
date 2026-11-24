export module hxlings.console;

import std;
import huxerui;
import xlings.types;
import hxlings.theme;
import hxlings.icons;
import hxlings.shortcuts;
import hxlings.store;
import hxlings.components;

using namespace huxerui;

export namespace hxlings {

[[huxerui::composable]]
View TaskConsole() {
    const auto store = UseService<XlingsStore>();
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const ToastHandle toast = UseToast();
    const auto clipboard = UseApplication().Clipboard();

    const bool expanded = store->console_expanded.Get();
    const bool running = store->task_running.Get();
    const std::string title = store->task_title.Get();
    const std::string last_status = store->task_last_status.Get();
    const bool failed = store->task_last_failed.Get();
    const std::optional<float> progress = store->task_progress.Get();
    const auto& lines = store->console_lines.Get();

    View status_indicator = Row {
        running
            ? InlineSpinner(16.0F, theme.colors.primary)
            : View(Row{}.With(
                  Frame{.width = 9.0F, .height = 9.0F},
                  CornerRadius(5.0F),
                  Background(failed ? sp.danger : sp.success)
              )),
    }.With(
        Frame{.width = 16.0F, .height = 16.0F},
        MainAlign(MainAxisAlignment::Center),
        CrossAlign(CrossAxisAlignment::Center)
    );

    View progress_view = View{};
    if (running) {
        progress_view = (progress.has_value()
                             ? ProgressBar(*progress)
                             : ProgressBar())
                            .With(Frame{.width = 140.0F, .height = 6.0F});
    }

    View cancel_btn = View{};
    if (running) {
        cancel_btn = ActionButton(
            "取消",
            ButtonTone::Danger,
            [store] { store->CancelCurrentOperation(); },
            true,
            std::nullopt,
            true
        );
    }

    const std::string status_text = running ? ("执行中: " + title) : last_status;

    View header = Row {
        std::move(status_indicator),
        Image(icons::TerminalIcon())
            .Fit(ImageFit::Contain)
            .Tint(theme.colors.on_surface_variant)
            .With(Frame{.width = 15.0F, .height = 15.0F}),
        Text(status_text, TextRole::Label)
            .Style(TextStyle{
                .font = Font::System(12.5F).WithWeight(FontWeight::Medium),
                .foreground = failed && !running ? sp.danger : theme.colors.on_surface,
            })
            .With(Grow(1.0F)),
        std::move(progress_view),
        std::move(cancel_btn),
        StatusBadge(std::to_string(lines.size()) + " 行", theme.colors.on_surface_variant, sp.muted_bg),
        ActionButton(
            "复制",
            ButtonTone::Ghost,
            [lines, clipboard, toast] {
                if (!clipboard || !clipboard->SupportsSynchronousText()) {
                    toast.Show("剪贴板不可用");
                    return;
                }
                std::string joined;
                for (const auto& l : lines) {
                    joined += l;
                    joined.push_back('\n');
                }
                if (clipboard->WriteText(joined)) {
                    toast.Show("已复制任务日志");
                } else {
                    toast.Show("复制到剪贴板失败");
                }
            },
            !lines.empty(),
            icons::CopyIcon(),
            true
        ),
        ActionButton(
            "清空",
            ButtonTone::Ghost,
            [store] { store->ClearConsole(); },
            !lines.empty(),
            std::nullopt,
            true
        ),
        ActionButton(
            expanded ? "收起日志" : "展开日志",
            ButtonTone::Secondary,
            [store, expanded] { store->console_expanded = !expanded; },
            true,
            std::nullopt,
            true
        ).With(Tooltip("切换任务日志 (" + FormatShortcut("J") + ")")),
    }.With(
        Padding(EdgeInsets::Symmetric(16.0F, 8.0F)),
        Spacing(10.0F),
        Background(theme.colors.surface),
        CrossAlign(CrossAxisAlignment::Center)
    );

    if (!expanded) {
        return header;
    }

    std::string joined_logs;
    for (const auto& l : lines) {
        joined_logs += l;
        joined_logs.push_back('\n');
    }

    View log_area = Column {
        ScrollView(
            SelectionArea(
                Text(joined_logs.empty() ? std::string("暂无任务输出日志") : std::move(joined_logs), TextRole::Body)
                    .Style(TextStyle{
                        .font = Font::Monospace(12.0F),
                        .foreground = lines.empty() ? sp.terminal_muted : sp.terminal_fg,
                    })
            ).With(Padding(12.0F))
        ).ScrollAxis(Axis::Vertical)
            .With(Grow(1.0F), ScrollBar{}),
    }.With(
        Frame{.height = 190.0F},
        Background(sp.terminal_bg),
        ClipChildren(),
        CrossAlign(CrossAxisAlignment::Stretch)
    );

    return Column {
        std::move(header),
        Divider(),
        std::move(log_area),
    }.With(CrossAlign(CrossAxisAlignment::Stretch));
}

} // namespace hxlings

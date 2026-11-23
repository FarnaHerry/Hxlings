export module hxlings.page.settings;

import std;
import mcpplibs.os;
import huxerui;
import app.resources;
import xlings.types;
import xlings.parser;
import xlings.process;
import hxlings.theme;
import hxlings.icons;
import hxlings.shortcuts;
import hxlings.settings;
import hxlings.updater;
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

namespace detail {

std::vector<std::string> SplitWhitespaceTokens(std::string_view sv) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < sv.size()) {
        while (i < sv.size() && (sv[i] == ' ' || sv[i] == '\t' || sv[i] == '\r' || sv[i] == '\n')) ++i;
        if (i >= sv.size()) break;
        const std::size_t start = i;
        while (i < sv.size() && sv[i] != ' ' && sv[i] != '\t' && sv[i] != '\r' && sv[i] != '\n') ++i;
        out.emplace_back(sv.substr(start, i - start));
    }
    return out;
}

} // namespace detail

[[huxerui::composable]]
View SelfInitDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast
) {
    auto home_input = UseState(TextEditingValue::FromText(""));
    auto force_init = UseState(false);

    auto submit = [ctx, store, tasks, toast, home_input, force_init]() {
        const std::string home = home_input.Get().text;
        const bool force = force_init.Get();
        ctx.Dismiss();
        store->RunSelfInitAsync(tasks, toast, home, force);
    };

    View body = Column {
        Text("调用 `xlings self init [--home <DIR>] [--force]` 初始化或切换 XLINGS_HOME。", TextRole::Body),
        TextField(home_input)
            .Label("指定主目录 (--home，可留空)")
            .Placeholder("留空则初始化默认 XLINGS_HOME")
            .Variant(TextFieldVariant::Outlined)
            .OnChanged([home_input](const TextEditingValue& v) mutable { home_input = v; })
            .OnSubmitted(submit),
        Checkbox("替换已有选中目录 (--force，不删除/移动原文件)", force_init.Get())
            .OnChanged([force_init](bool v) mutable { force_init = v; }),
    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch));

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton("执行初始化", ButtonTone::Primary, submit, true, icons::CheckIcon()),
    }.With(Spacing(10.0F));

    return DialogCardShell("初始化 xlings 目录 (self init)", std::move(body), std::move(actions), 480.0F);
}

[[huxerui::composable]]
View ConfigCliExtensionsCard(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const ConfigSnapshot& cfg = store->config.Get();
    const auto& themes = store->config_themes.Get();
    const bool loading_themes = store->loading_config_themes.Get();

    auto custom_theme_input = UseState(TextEditingValue::FromText(""));
    auto runtime_input = UseState(TextEditingValue::FromText(""));
    auto archive_input = UseState(TextEditingValue::FromText(""));

    Lifecycle([store, tasks] {
        if (!store->skip_initial_refresh && store->config_themes.Get().empty()) {
            store->RefreshConfigThemes(tasks, false);
        }
    });

    std::vector<View> theme_chips;
    if (themes.empty()) {
        const std::string cur = cfg.theme.empty() ? std::string("default") : cfg.theme;
        theme_chips.push_back(
            ActionButton(
                cur + " (当前)",
                ButtonTone::Primary,
                [store, tasks, toast, cur] { store->SetConfigThemeAsync(tasks, toast, cur); },
                true,
                std::nullopt,
                true
            ).Key("cli-theme-fallback")
        );
    } else {
        for (std::size_t i = 0; i < themes.size(); ++i) {
            const auto& t = themes[i];
            const bool active = t.active || (!cfg.theme.empty() && cfg.theme == t.name);
            theme_chips.push_back(
                ActionButton(
                    active ? (t.name + " ✓") : t.name,
                    active ? ButtonTone::Primary : ButtonTone::Secondary,
                    [store, tasks, toast, name = t.name] { store->SetConfigThemeAsync(tasks, toast, name); },
                    true,
                    std::nullopt,
                    true
                ).Key("cli-theme-" + std::to_string(i) + "-" + t.name)
            );
        }
    }

    std::vector<View> archive_rows;
    for (std::size_t i = 0; i < cfg.archive_dirs.size(); ++i) {
        const std::string& dir = cfg.archive_dirs[i];
        archive_rows.push_back(
            Row {
                Text(dir, TextRole::Body)
                    .Style(TextStyle{.font = Font::Monospace(12.5F), .foreground = theme.colors.on_surface})
                    .With(Grow(1.0F)),
                ActionButton(
                    "移除",
                    ButtonTone::Danger,
                    [store, tasks, toast, dir] { store->RemoveArchiveDirAsync(tasks, toast, dir); },
                    true,
                    icons::TrashIcon(),
                    true
                ),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 6.0F)),
                Spacing(8.0F),
                CrossAlign(CrossAxisAlignment::Center),
                Background(theme.colors.surface_container_low),
                CornerRadius(8.0F)
            ).Key("archive-dir-" + std::to_string(i))
        );
    }

    return Card(
        Column {
            SectionHeader(
                "CLI 主题、默认运行时与离线归档",
                "管理 `xlings config --theme`、`--runtime` 与 `--add-archives/--rm-archives`",
                ActionButton(
                    loading_themes ? "加载中..." : "刷新主题列表",
                    ButtonTone::Secondary,
                    [store, tasks] { store->RefreshConfigThemes(tasks, true); },
                    !loading_themes,
                    icons::RefreshIcon(),
                    true
                )
            ),
            Text(
                "CLI 配色主题 (当前: " + (cfg.theme.empty() ? std::string("default") : cfg.theme) + ")",
                TextRole::Label
            ).Style(TextStyle{.font = Font::System(13.0F).WithWeight(FontWeight::SemiBold)}),
            Flow(std::move(theme_chips)).With(Spacing(8.0F)),
            Row {
                TextField(custom_theme_input)
                    .Placeholder("自定义主题名或 JSON 文件路径...")
                    .Variant(TextFieldVariant::Outlined)
                    .OnChanged([custom_theme_input](const TextEditingValue& v) mutable { custom_theme_input = v; })
                    .With(Grow(1.0F)),
                ActionButton(
                    "应用主题",
                    ButtonTone::Secondary,
                    [store, tasks, toast, custom_theme_input] {
                        const std::string val = custom_theme_input.Get().text;
                        if (val.empty()) {
                            toast.Show("请输入主题名称或 JSON 路径");
                            return;
                        }
                        store->SetConfigThemeAsync(tasks, toast, val);
                    },
                    true,
                    icons::CheckIcon(),
                    true
                ),
            }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
            Divider(),
            Row {
                Text("默认运行时 (--runtime)", TextRole::Label)
                    .Style(TextStyle{.font = Font::System(13.0F).WithWeight(FontWeight::SemiBold)})
                    .With(Frame{.width = 160.0F}),
                StatusBadge(
                    cfg.default_runtime.empty() ? std::string("未固定 (auto)") : cfg.default_runtime,
                    sp.info,
                    sp.info_bg
                ),
            }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
            Row {
                TextField(runtime_input)
                    .Placeholder("例如: glibc@2.44.3 或 auto")
                    .Variant(TextFieldVariant::Outlined)
                    .OnChanged([runtime_input](const TextEditingValue& v) mutable { runtime_input = v; })
                    .With(Grow(1.0F)),
                ActionButton(
                    "固定运行时",
                    ButtonTone::Primary,
                    [store, tasks, toast, runtime_input] {
                        const std::string val = runtime_input.Get().text;
                        if (val.empty()) {
                            toast.Show("请输入运行时绑定（如 glibc@2.44.3 或 auto）");
                            return;
                        }
                        store->SetConfigRuntimeAsync(tasks, toast, val);
                    },
                    true,
                    icons::CheckIcon(),
                    true
                ),
            }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
            Divider(),
            Text(
                "离线归档目录 (" + std::to_string(cfg.archive_dirs.size()) + ")",
                TextRole::Label
            ).Style(TextStyle{.font = Font::System(13.0F).WithWeight(FontWeight::SemiBold)}),
            Row {
                TextField(archive_input)
                    .Placeholder("输入本地归档目录路径 (--add-archives)")
                    .Variant(TextFieldVariant::Outlined)
                    .OnChanged([archive_input](const TextEditingValue& v) mutable { archive_input = v; })
                    .With(Grow(1.0F)),
                ActionButton(
                    "添加目录",
                    ButtonTone::Secondary,
                    [store, tasks, toast, archive_input] {
                        const std::string val = archive_input.Get().text;
                        if (val.empty()) {
                            toast.Show("请输入归档目录路径");
                            return;
                        }
                        store->AddArchiveDirAsync(tasks, toast, val);
                    },
                    true,
                    icons::PlusIcon(),
                    true
                ),
            }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
            archive_rows.empty()
                ? Text("尚未配置离线归档目录", TextRole::Body)
                      .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                : View(Column(std::move(archive_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).Key("settings-cli-ext-card");
}

[[huxerui::composable]]
View NetworkProbeCard(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const auto& endpoints = store->network_endpoints.Get();
    const auto& probe = store->network_probe_result.Get();
    const bool loading_net = store->loading_network.Get();
    const bool probing = store->probing_url.Get();

    auto custom_url = UseState(TextEditingValue::FromText("https://github.com/FarnaHerry/Hxlings"));

    Lifecycle([store, tasks] {
        if (!store->skip_initial_refresh && store->network_endpoints.Get().empty()) {
            store->RefreshNetworkEndpoints(tasks, false);
        }
    });

    std::vector<View> ep_rows;
    for (std::size_t i = 0; i < endpoints.size(); ++i) {
        const auto& ep = endpoints[i];
        ep_rows.push_back(
            Row {
                Column {
                    Row {
                        StatusBadge(
                            ep.region.empty() ? std::string("NET") : ep.region,
                            sp.info,
                            sp.info_bg
                        ),
                        ep.resource.empty()
                            ? View{}
                            : StatusBadge(ep.resource, theme.colors.on_surface_variant, theme.colors.surface_container_high),
                    }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                    ExternalLinkRow(ep.url, "net-ep-" + std::to_string(i)),
                }.With(Spacing(4.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                ActionButton(
                    probing ? "探测中" : "探测",
                    ButtonTone::Secondary,
                    [store, tasks, toast, url = ep.url] { store->ProbeNetworkUrlAsync(tasks, toast, url); },
                    !probing,
                    icons::GlobeIcon(),
                    true
                ),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                Spacing(8.0F),
                CrossAlign(CrossAxisAlignment::Center),
                Background(theme.colors.surface_container_low),
                CornerRadius(8.0F)
            ).Key("net-ep-row-" + std::to_string(i))
        );
    }

    View probe_summary = View{};
    if (probe.has_value()) {
        const std::string target_label = probe->host.empty() ? probe->url : probe->host;
        std::string detail;
        if (!probe->ok) {
            detail = target_label + " · " +
                     (probe->error_message.empty() ? std::string("探测失败") : probe->error_message);
        } else {
            const long long ms = std::llround(probe->elapsed_seconds * 1000.0);
            detail = target_label + " · HTTP " + std::to_string(probe->http_status) +
                     " · " + std::to_string(ms) + " ms" +
                     (probe->proxy ? " · 走代理" : " · 直连") +
                     (!probe->reachable && !probe->status.empty() ? (" · " + probe->status) : "") +
                     (probe->error_message.empty() ? "" : (" · " + probe->error_message));
        }
        probe_summary = Row {
            StatusBadge(
                probe->reachable ? "可达" : (probe->ok ? "不可达" : "失败"),
                probe->reachable ? sp.success : sp.danger,
                probe->reachable ? sp.success_bg : sp.danger_bg
            ),
            Text(detail, TextRole::Body)
                .Style(TextStyle{.font = Font::Monospace(12.0F), .foreground = theme.colors.on_surface})
                .With(Grow(1.0F)),
        }.With(
            Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
            Spacing(8.0F),
            CrossAlign(CrossAxisAlignment::Center),
            Background(theme.colors.surface_container_low),
            CornerRadius(8.0F)
        );
    }

    return Card(
        Column {
            SectionHeader(
                "网络端点发现与连通性探测",
                "查询 `interface network_endpoints` 并通过 `interface probe_url` 测试下载链路延迟",
                ActionButton(
                    loading_net ? "刷新中..." : "刷新端点",
                    ButtonTone::Secondary,
                    [store, tasks] { store->RefreshNetworkEndpoints(tasks, true); },
                    !loading_net,
                    icons::RefreshIcon(),
                    true
                )
            ),
            ep_rows.empty()
                ? Text("暂无网络端点记录（点击右上角刷新端点）", TextRole::Body)
                      .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                : View(Column(std::move(ep_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
            Row {
                TextField(custom_url)
                    .Placeholder("输入待测 http/https URL...")
                    .Variant(TextFieldVariant::Outlined)
                    .OnChanged([custom_url](const TextEditingValue& v) mutable { custom_url = v; })
                    .With(Grow(1.0F)),
                ActionButton(
                    probing ? "探测中..." : "立即探测",
                    ButtonTone::Primary,
                    [store, tasks, toast, custom_url] {
                        store->ProbeNetworkUrlAsync(tasks, toast, custom_url.Get().text);
                    },
                    !probing,
                    icons::GlobeIcon(),
                    true
                ),
            }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
            std::move(probe_summary),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).Key("settings-network-probe-card");
}

[[huxerui::composable]]
View ScriptRunnerCard(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast
) {
    const auto script_file = UseState(TextEditingValue::FromText(""));
    const auto script_args = UseState(TextEditingValue::FromText(""));
    const bool running = store->running_script.Get();
    const std::string& output = store->script_output.Get();

    return Card(
        Column {
            SectionHeader(
                "脚本运行器 (xlings script)",
                "运行本地 xlings 自动化脚本文件并实时查看输出"
            ),
            Row {
                TextField(script_file)
                    .Label("脚本路径 (.lua / 脚本文件)")
                    .Placeholder("/path/to/setup.lua")
                    .Variant(TextFieldVariant::Outlined)
                    .OnChanged([script_file](const TextEditingValue& v) mutable { script_file = v; })
                    .With(Grow(1.0F)),
                TextField(script_args)
                    .Label("脚本参数（空格分隔）")
                    .Placeholder("--flag value")
                    .Variant(TextFieldVariant::Outlined)
                    .OnChanged([script_args](const TextEditingValue& v) mutable { script_args = v; })
                    .With(Grow(1.0F)),
            }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::End)),
            Row {
                ActionButton(
                    running ? "运行中..." : "运行脚本",
                    ButtonTone::Primary,
                    [store, tasks, toast, script_file, script_args] {
                        store->RunScriptAsync(
                            tasks,
                            toast,
                            script_file.Get().text,
                            detail::SplitWhitespaceTokens(script_args.Get().text)
                        );
                    },
                    !running,
                    icons::FileCodeIcon(),
                    true
                ),
            },
            CodeBox(output, 100.0F, 210.0F),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).Key("settings-script-runner-card");
}

[[huxerui::composable]]
View SelfHomeSetupCard(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    DialogHandle dialog
) {
    return Card(
        Column {
            SectionHeader(
                "目录初始化与布局迁移",
                "管理 `xlings self migrate`（迁移旧版布局）与 `xlings self init`（初始化主目录）"
            ),
            Flow {
                ActionButton(
                    "迁移旧目录布局 (self migrate)",
                    ButtonTone::Secondary,
                    [dialog, store, tasks, toast] {
                        dialog.Show(
                            "迁移目录布局",
                            "确认执行 `xlings self migrate` 将旧版目录结构迁移到当前规范布局吗？",
                            "确认迁移",
                            "取消",
                            [store, tasks, toast] { store->RunSelfMigrateAsync(tasks, toast); },
                            {}
                        );
                    },
                    true,
                    icons::WrenchIcon(),
                    true
                ),
                ActionButton(
                    "初始化目录 (self init)",
                    ButtonTone::Secondary,
                    [dialog, store, tasks, toast] {
                        dialog.Show([store, tasks, toast](DialogContext ctx) {
                            return SelfInitDialogContent(ctx, store, tasks, toast);
                        });
                    },
                    true,
                    icons::PlusIcon(),
                    true
                ),
            }.With(Spacing(10.0F)),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).Key("settings-self-setup-card");
}

[[huxerui::composable]]
View AppUpdateSection(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    ApplicationHandle app,
    std::shared_ptr<HttpClient> http
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const AppUpdateState& upd = store->app_update_state.Get();

    const bool checking = upd.phase == UpdatePhase::Checking;
    const bool downloading = upd.phase == UpdatePhase::Downloading;
    const bool verifying = upd.phase == UpdatePhase::Verifying;
    const bool installing = upd.phase == UpdatePhase::Installing;
    const bool is_busy = checking || downloading || verifying || installing;

    std::string badge_text = "未检查";
    Color badge_fg = theme.colors.on_surface_variant;
    Color badge_bg = sp.muted_bg;
    switch (upd.phase) {
        case UpdatePhase::Idle:
            break;
        case UpdatePhase::Checking:
            badge_text = "检查中...";
            badge_fg = sp.info;
            badge_bg = sp.info_bg;
            break;
        case UpdatePhase::UpToDate:
            badge_text = "已是最新";
            badge_fg = sp.success;
            badge_bg = sp.success_bg;
            break;
        case UpdatePhase::Available:
            badge_text = "发现新版本";
            badge_fg = theme.colors.primary;
            badge_bg = theme.colors.primary_container;
            break;
        case UpdatePhase::Downloading:
            badge_text = "下载中";
            badge_fg = sp.info;
            badge_bg = sp.info_bg;
            break;
        case UpdatePhase::Verifying:
            badge_text = "校验中...";
            badge_fg = sp.info;
            badge_bg = sp.info_bg;
            break;
        case UpdatePhase::ReadyToInstall:
            badge_text = "待安装";
            badge_fg = sp.success;
            badge_bg = sp.success_bg;
            break;
        case UpdatePhase::Installing:
            badge_text = "安装中...";
            badge_fg = sp.info;
            badge_bg = sp.info_bg;
            break;
        case UpdatePhase::Error:
            badge_text = "更新失败";
            badge_fg = sp.danger;
            badge_bg = sp.danger_bg;
            break;
    }

    const std::string rel_url = (upd.latest.has_value() && !upd.latest->html_url.empty())
                                    ? upd.latest->html_url
                                    : std::string(kAppReleasePageUrl);

    std::vector<View> action_buttons;
    action_buttons.push_back(
        ActionButton(
            checking ? "检查中..." : "检查更新",
            ButtonTone::Secondary,
            [store, tasks, toast, http] { store->CheckForAppUpdateAsync(tasks, toast, http); },
            !is_busy,
            icons::RefreshIcon(),
            true
        ).Key("about-check-update-btn")
    );
    if (upd.latest.has_value() && upd.phase != UpdatePhase::UpToDate &&
        upd.selected_asset.has_value() && !is_busy && upd.phase != UpdatePhase::ReadyToInstall) {
        action_buttons.push_back(
            ActionButton(
                "下载更新",
                ButtonTone::Primary,
                [store, tasks, toast, http] { store->DownloadAppUpdateAsync(tasks, toast, http); },
                true,
                icons::DownloadIcon(),
                true
            ).Key("about-download-update-btn")
        );
    }
    if (downloading || verifying) {
        action_buttons.push_back(
            ActionButton(
                "取消下载",
                ButtonTone::Danger,
                [store] { store->CancelAppUpdate(); },
                true,
                std::nullopt,
                true
            ).Key("about-cancel-update-btn")
        );
    }
    if (upd.phase == UpdatePhase::ReadyToInstall && !upd.downloaded_file_path.empty()) {
        action_buttons.push_back(
            ActionButton(
                "立即安装更新",
                ButtonTone::Primary,
                [store, toast, app] { store->ApplyAppUpdateAsync(toast, app); },
                true,
                icons::CheckIcon(),
                true
            ).Key("about-apply-update-btn")
        );
    }
    if (!upd.downloaded_file_path.empty()) {
        action_buttons.push_back(
            ActionButton(
                "打开下载目录",
                ButtonTone::Ghost,
                [store, toast] {
                    if (store->skip_initial_refresh) return;
                    if (!OpenLocalPath(ResolveUpdateDownloadDir())) {
                        toast.Show("无法打开更新下载目录");
                    }
                },
                true,
                std::nullopt,
                true
            ).Key("about-open-update-dir-btn")
        );
    }
    action_buttons.push_back(
        ActionButton(
            "查看发布页",
            ButtonTone::Ghost,
            [store, toast, rel_url] { store->OpenUrl(toast, rel_url); },
            true,
            icons::ExternalLinkIcon(),
            true
        ).Key("about-release-page-btn")
    );

    std::vector<View> items;
    items.push_back(
        Row {
            Text("软件自更新", TextRole::Label)
                .Style(TextStyle{.font = Font::System(13.5F).WithWeight(FontWeight::SemiBold)}),
            StatusBadge(badge_text, badge_fg, badge_bg),
            is_busy ? View(ProgressCircle().With(Frame{.width = 15.0F, .height = 15.0F})) : View{},
            Spacer{}.With(Grow(1.0F)),
            upd.checked_at.empty()
                ? View{}
                : Text("上次检查: " + upd.checked_at, TextRole::Label)
                      .Style(TextStyle{.font = Font::Monospace(11.5F), .foreground = theme.colors.on_surface_variant}),
        }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center))
    );
    items.push_back(Flow(std::move(action_buttons)).With(Spacing(8.0F)));

    if (downloading || verifying) {
        View bar = (upd.total_bytes > 0)
                       ? View(ProgressBar(std::clamp(
                             static_cast<float>(upd.downloaded_bytes) / static_cast<float>(upd.total_bytes),
                             0.0F,
                             1.0F)))
                       : View(ProgressBar());
        const std::string bytes_text = upd.total_bytes > 0
                                           ? (FormatBytes(upd.downloaded_bytes) + " / " + FormatBytes(upd.total_bytes))
                                           : FormatBytes(upd.downloaded_bytes);
        items.push_back(
            Row {
                std::move(bar).With(Grow(1.0F)),
                Text(bytes_text, TextRole::Label)
                    .Style(TextStyle{.font = Font::Monospace(11.5F), .foreground = theme.colors.on_surface_variant}),
            }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Center))
        );
    }

    if (upd.latest.has_value()) {
        const auto& rel = *upd.latest;
        const std::string pub = rel.published_at.empty()
                                    ? std::string{}
                                    : (" · 发布于 " + FormatLocalTimestamp(rel.published_at));
        items.push_back(
            Text("最新版本: " + rel.tag_name + pub, TextRole::Label)
                .Style(TextStyle{.font = Font::System(12.5F), .foreground = theme.colors.on_surface})
        );
        if (upd.selected_asset.has_value()) {
            const auto& asset = *upd.selected_asset;
            items.push_back(
                Row {
                    Text(
                        "适配安装包: " + asset.name +
                            (asset.size_bytes > 0 ? (" (" + FormatBytes(asset.size_bytes) + ")") : ""),
                        TextRole::Label
                    ).Style(TextStyle{.font = Font::Monospace(11.5F), .foreground = theme.colors.on_surface_variant})
                        .With(Grow(1.0F)),
                    upd.sha256_verified ? StatusBadge("SHA-256 已验证", sp.success, sp.success_bg) : View{},
                }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center))
            );
        } else if (upd.phase == UpdatePhase::Available) {
            items.push_back(
                Text("未找到适配当前平台的预编译安装包，请点击“查看发布页”手动下载。", TextRole::Label)
                    .Style(TextStyle{.foreground = sp.warning})
            );
        }
        if (!rel.body.empty()) {
            items.push_back(CodeBox(rel.body, 72.0F, 130.0F));
        }
    }

    const std::string& msg = !upd.error_message.empty() ? upd.error_message : upd.status_message;
    if (!msg.empty()) {
        items.push_back(
            Text(msg, TextRole::Label)
                .Style(TextStyle{
                    .font = Font::System(12.0F),
                    .foreground = upd.phase == UpdatePhase::Error ? sp.danger : theme.colors.on_surface_variant,
                })
        );
    }

    return Column(std::move(items))
        .With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))
        .Key("settings-app-update-box");
}

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
    const auto http = UseService<HttpClient>();
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
                Text(FormatTimestampsInText(p.summary.empty() ? p.raw_line : p.summary), TextRole::Body)
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
                        StatusBadge("v0.1.4", theme.colors.primary, theme.colors.primary_container),
                    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Text("基于 C++23 Modules、mcpp 与 HuxerUI 构建的跨平台 xlings 桌面管理器", TextRole::Label)
                        .Style(TextStyle{.font = Font::System(12.0F),
                                         .foreground = theme.colors.on_surface_variant}),
                }.With(Spacing(3.0F), Grow(1.0F)),
            }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
            Divider(),
            KeyValueRow("应用版本", std::string("Hxlings v") + std::string(kAppVersion) + " · HuxerUI", false, true),
            KeyValueRow("xlings CLI", cli_version_text, false, true),
            KeyValueRow("开源协议", "MIT License", false, false),
            KeyValueRow("Hxlings 仓库", "https://github.com/FarnaHerry/Hxlings", true, true, "about-link-hxlings"),
            KeyValueRow("HuxerUI 官方", "https://github.com/HuxerUI/HuxerUI", true, true, "about-link-huxerui"),
            KeyValueRow("xlings 官方", "https://github.com/openxlings/xlings", true, true, "about-link-xlings"),
            KeyValueRow("mcpp 官方", "https://github.com/mcpp-community/mcpp", true, true, "about-link-mcpp"),
            Divider(),
            AppUpdateSection(store, tasks, toast, app, http),
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
            ConfigCliExtensionsCard(store, tasks, toast),
        }.With(Padding(22.0F), Spacing(16.0F), CrossAlign(CrossAxisAlignment::Stretch)).Key("settings-tab-1");
    } else {
        body_content = Column {
            NetworkProbeCard(store, tasks, toast),
            std::move(doctor_card),
            std::move(maintenance_card),
            ScriptRunnerCard(store, tasks, toast),
            SelfHomeSetupCard(store, tasks, toast, dialog),
        }.With(Padding(22.0F), Spacing(16.0F), CrossAlign(CrossAxisAlignment::Stretch)).Key("settings-tab-2");
    }

    return Column {
        std::move(tab_bar),
        Divider(),
        ScrollView(std::move(body_content))
            .ScrollAxis(Axis::Vertical)
            .With(Grow(1.0F), ScrollBar{})
            .Key("settings-scroll"),
    }.With(Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch));
}

} // namespace hxlings

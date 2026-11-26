export module hxlings.page.settings.cards;

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

export namespace hxlings::settings_detail {


std::size_t MatchOptionIndex(std::string_view val, std::span<const std::string_view> opts, std::size_t fallback = 0) {
    for (std::size_t i = 0; i < opts.size(); ++i) {
        if (val.find(opts[i]) != std::string_view::npos) return i;
    }
    return fallback;
}


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

} // namespace hxlings::settings_detail


export namespace hxlings {

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
                RefreshButton(
                    "刷新主题列表",
                    [store, tasks] { store->RefreshConfigThemes(tasks, true); },
                    loading_themes
                ).Key("settings-refresh-themes-btn")
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
    const std::string& probing_target = store->probing_target_url.Get();
    const bool probing_custom = store->probing_custom_url.Get();

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
                    "探测",
                    ButtonTone::Secondary,
                    [store, tasks, toast, url = ep.url] { store->ProbeNetworkUrlAsync(tasks, toast, url, false); },
                    !probing,
                    icons::GlobeIcon(),
                    true,
                    probing && !probing_custom && probing_target == ep.url
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
                RefreshButton(
                    "刷新端点",
                    [store, tasks] { store->RefreshNetworkEndpoints(tasks, true); },
                    loading_net
                ).Key("settings-refresh-net-btn")
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
                    "立即探测",
                    ButtonTone::Primary,
                    [store, tasks, toast, custom_url] {
                        store->ProbeNetworkUrlAsync(tasks, toast, custom_url.Get().text, true);
                    },
                    !probing,
                    icons::GlobeIcon(),
                    true,
                    probing && probing_custom
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
                    "运行脚本",
                    ButtonTone::Primary,
                    [store, tasks, toast, script_file, script_args] {
                        store->RunScriptAsync(
                            tasks,
                            toast,
                            script_file.Get().text,
                            settings_detail::SplitWhitespaceTokens(script_args.Get().text)
                        );
                    },
                    true,
                    icons::FileCodeIcon(),
                    true,
                    running
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
        RefreshButton(
            "检查更新",
            [store, tasks, toast, http] { store->CheckForAppUpdateAsync(tasks, toast, http); },
            checking,
            ButtonTone::Secondary,
            !is_busy,
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

} // namespace hxlings

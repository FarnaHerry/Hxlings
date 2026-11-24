export module hxlings.page.luban;

import std;
import huxerui;
import hxlings.theme;
import hxlings.icons;
import hxlings.store;
import hxlings.components;
import hxlings.luban.types;
import hxlings.luban.stack;

using namespace huxerui;

namespace hxlings::detail {

inline std::string TrimLubanText(std::string_view s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])) != 0) ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])) != 0) --e;
    return std::string(s.substr(b, e - b));
}

inline bool IsValidLubanEnvName(std::string_view name) {
    if (name.empty()) return false;
    return std::ranges::all_of(name, [](unsigned char ch) {
        return std::isalnum(ch) != 0 || ch == '_' || ch == '-';
    });
}

inline std::vector<std::string> SplitCsvItems(std::string_view raw) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= raw.size()) {
        const std::size_t sep = raw.find_first_of(",;\n", start);
        const std::string item = TrimLubanText(
            sep == std::string_view::npos ? raw.substr(start) : raw.substr(start, sep - start));
        if (!item.empty()) {
            out.push_back(item);
        }
        if (sep == std::string_view::npos) break;
        start = sep + 1;
    }
    return out;
}

inline std::string JoinCsvItems(const std::vector<std::string>& items) {
    std::string out;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].empty()) continue;
        if (!out.empty()) out += ", ";
        out += items[i];
    }
    return out;
}

inline std::vector<std::string> SplitLubanCommandArgv(std::string_view line) {
    std::vector<std::string> argv;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) != 0) ++i;
        if (i >= line.size()) break;
        std::string token;
        char quote = 0;
        while (i < line.size()) {
            const char ch = line[i];
            if (quote != 0) {
                if (ch == quote) {
                    quote = 0;
                    ++i;
                } else if (ch == '\\' && quote == '"' && i + 1 < line.size()) {
                    token.push_back(line[i + 1]);
                    i += 2;
                } else {
                    token.push_back(ch);
                    ++i;
                }
            } else {
                if (std::isspace(static_cast<unsigned char>(ch)) != 0) break;
                if (ch == '"' || ch == '\'') {
                    quote = ch;
                    ++i;
                } else {
                    token.push_back(ch);
                    ++i;
                }
            }
        }
        if (!token.empty()) argv.push_back(std::move(token));
    }
    return argv;
}

inline std::string ToLowerCopy(std::string_view text) {
    std::string out(text);
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

inline std::string DisplayVal(const std::string& val, std::string_view fallback = "—") {
    return val.empty() ? std::string(fallback) : val;
}

} // namespace hxlings::detail

export namespace hxlings {

[[huxerui::composable]]
View LubanOptionChips(
    const std::vector<std::pair<std::string, std::string>>& options,
    const std::string& selected,
    std::function<void(std::string)> on_select,
    std::string key_prefix
) {
    std::vector<View> items;
    items.reserve(options.size());
    for (std::size_t i = 0; i < options.size(); ++i) {
        const std::string& val = options[i].first;
        const std::string& label = options[i].second;
        const bool is_sel = (selected == val);
        items.push_back(
            ActionButton(
                label,
                is_sel ? ButtonTone::Primary : ButtonTone::Secondary,
                [on_select, val] { on_select(val); },
                true,
                std::nullopt,
                true
            ).Key(key_prefix + "-" + std::to_string(i))
        );
    }
    return Flow(std::move(items)).With(Spacing(6.0F));
}

[[huxerui::composable]]
View NewLubanEnvDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    std::string default_edition = {},
    std::string default_desktop = {}
) {
    const auto& catalog = store->luban_catalog.Get();
    const std::string init_edition = !default_edition.empty()
                                         ? default_edition
                                         : (!default_desktop.empty() ? std::string("desktop") : std::string("core"));
    const std::string init_mode = !default_desktop.empty() ? std::string("sandbox") : std::string("sandbox");

    auto name_input = UseState(TextEditingValue::FromText(""));
    auto edition_opt = UseState(init_edition);
    auto desktop_opt = UseState(default_desktop);
    auto mode_opt = UseState(init_mode);
    auto persist_opt = UseState(std::string{});
    auto policy_input = UseState(TextEditingValue::FromText(""));
    auto proxy_input = UseState(TextEditingValue::FromText(""));
    auto xpkgs_input = UseState(TextEditingValue::FromText(""));
    auto allows_input = UseState(TextEditingValue::FromText(default_desktop.empty() ? "" : "display,gpu"));
    auto no_password_opt = UseState(false);
    auto dry_run_opt = UseState(false);
    auto err_msg = UseState(std::string{});

    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    std::vector<std::pair<std::string, std::string>> edition_opts;
    if (!catalog.distributions.empty()) {
        for (const auto& d : catalog.distributions) {
            edition_opts.emplace_back(d.name, d.title_zh.empty() ? d.name : (d.name + " (" + d.title_zh + ")"));
        }
    } else {
        edition_opts = {{"core", "core"}, {"dev", "dev"}, {"desktop", "desktop"}};
    }

    std::vector<std::pair<std::string, std::string>> desktop_opts{{"", "无桌面"}};
    if (!catalog.desktops.empty()) {
        for (const auto& d : catalog.desktops) {
            desktop_opts.emplace_back(d.name, d.name);
        }
    } else {
        desktop_opts.emplace_back("xfce", "xfce");
        desktop_opts.emplace_back("gnome", "gnome");
    }

    const std::vector<std::pair<std::string, std::string>> mode_opts{
        {"sandbox", "sandbox"}, {"machine", "machine"}
    };
    const std::vector<std::pair<std::string, std::string>> persist_opts{
        {"", "默认"}, {"all", "all"}, {"home", "home"}, {"none", "none"}
    };

    auto submit = [ctx, store, tasks, toast, name_input, edition_opt, desktop_opt, mode_opt, persist_opt,
                   policy_input, proxy_input, xpkgs_input, allows_input, no_password_opt, dry_run_opt, err_msg]() mutable {
        const std::string name = detail::TrimLubanText(name_input.Get().text);
        if (!detail::IsValidLubanEnvName(name)) {
            err_msg = std::string("环境名称不能为空，且仅支持字母、数字、_ 或 -");
            return;
        }
        LubanCreateOptions opts{
            .name = name,
            .edition = edition_opt.Get(),
            .desktop = desktop_opt.Get(),
            .mode = mode_opt.Get(),
            .persist = persist_opt.Get(),
            .policy = detail::TrimLubanText(policy_input.Get().text),
            .proxy = detail::TrimLubanText(proxy_input.Get().text),
            .no_password = no_password_opt.Get(),
            .dry_run = dry_run_opt.Get(),
            .xpkgs = detail::SplitCsvItems(xpkgs_input.Get().text),
            .allows = detail::SplitCsvItems(allows_input.Get().text),
        };
        ctx.Dismiss();
        store->CreateLubanEnvAsync(tasks, toast, std::move(opts));
    };

    View body = ScrollView(
        Column {
            TextField(name_input)
                .Label("环境名称 *")
                .Placeholder("例如: cpp23-sandbox, ai-lab")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([name_input, err_msg](const TextEditingValue& v) mutable {
                    name_input = v;
                    err_msg = std::string{};
                })
                .OnSubmitted(submit),
            Text("发行版 (--edition)", TextRole::Label),
            LubanOptionChips(
                edition_opts,
                edition_opt.Get(),
                [edition_opt](std::string v) mutable { edition_opt = std::move(v); },
                "new-luban-ed"
            ),
            Text("桌面环境 (--desktop)", TextRole::Label),
            LubanOptionChips(
                desktop_opts,
                desktop_opt.Get(),
                [desktop_opt](std::string v) mutable { desktop_opt = std::move(v); },
                "new-luban-desk"
            ),
            Text("运行模式 (--mode)", TextRole::Label),
            LubanOptionChips(
                mode_opts,
                mode_opt.Get(),
                [mode_opt](std::string v) mutable { mode_opt = std::move(v); },
                "new-luban-mode"
            ),
            Text("持久化策略 (--persist)", TextRole::Label),
            LubanOptionChips(
                persist_opts,
                persist_opt.Get(),
                [persist_opt](std::string v) mutable { persist_opt = std::move(v); },
                "new-luban-persist"
            ),
            TextField(policy_input)
                .Label("隔离策略预设 (--policy)")
                .Placeholder("例如: dev, strict, locked (可留空)")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([policy_input](const TextEditingValue& v) mutable { policy_input = v; }),
            TextField(proxy_input)
                .Label("代理端点 (--proxy)")
                .Placeholder("例如: socks5h://127.0.0.1:1080")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([proxy_input](const TextEditingValue& v) mutable { proxy_input = v; }),
            TextField(xpkgs_input)
                .Label("预装软件包 (--xpkg，逗号分隔)")
                .Placeholder("例如: xim:gcc, xim:cmake, xim:ninja")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([xpkgs_input](const TextEditingValue& v) mutable { xpkgs_input = v; }),
            TextField(allows_input)
                .Label("宿主授权 (--allow，逗号分隔)")
                .Placeholder("例如: display, audio, gpu, ssh-agent")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([allows_input](const TextEditingValue& v) mutable { allows_input = v; }),
            Checkbox("免密用户配置 (--no-password)", no_password_opt.Get())
                .OnChanged([no_password_opt](bool v) mutable { no_password_opt = v; }),
            Checkbox("仅预览构建计划 (--dry-run)", dry_run_opt.Get())
                .OnChanged([dry_run_opt](bool v) mutable { dry_run_opt = v; }),
            err_msg.Get().empty()
                ? View{}
                : Text(err_msg.Get(), TextRole::Label).Style(TextStyle{.foreground = sp.danger}),
        }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Frame{.max_height = 330.0F}, ScrollBar{});

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton(
            dry_run_opt.Get() ? "预览创建" : "创建环境",
            ButtonTone::Primary,
            submit,
            true,
            icons::PlusIcon()
        ),
    }.With(Spacing(10.0F));

    return DialogCardShell("新建 LubanOS 环境", std::move(body), std::move(actions), 520.0F)
        .Key("new-luban-env-dialog");
}

[[huxerui::composable]]
View LubanImportDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast
) {
    auto path_input = UseState(TextEditingValue::FromText(""));
    auto name_input = UseState(TextEditingValue::FromText(""));
    auto err_msg = UseState(std::string{});

    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    auto submit = [ctx, store, tasks, toast, path_input, name_input, err_msg]() mutable {
        const std::string archive = detail::TrimLubanText(path_input.Get().text);
        if (archive.empty()) {
            err_msg = std::string("请输入要导入的归档文件或镜像路径");
            return;
        }
        const std::string cname = detail::TrimLubanText(name_input.Get().text);
        if (!cname.empty() && !detail::IsValidLubanEnvName(cname)) {
            err_msg = std::string("自定义环境名仅支持字母、数字、_ 或 -");
            return;
        }
        ctx.Dismiss();
        store->ImportLubanEnvAsync(tasks, toast, archive, cname);
    };

    View body = Column {
        Text("从 `.luban` / `.tar` / `.qcow2` 镜像档案创建并还原 Luban 环境。", TextRole::Body),
        TextField(path_input)
            .Label("镜像文件或目录路径 *")
            .Placeholder("例如: ./dev-core.luban 或 ./rootfs.tar")
            .Variant(TextFieldVariant::Outlined)
            .OnChanged([path_input, err_msg](const TextEditingValue& v) mutable {
                path_input = v;
                err_msg = std::string{};
            })
            .OnSubmitted(submit),
        TextField(name_input)
            .Label("指定新环境名称 (--name，可留空)")
            .Placeholder("留空则使用归档内置名称")
            .Variant(TextFieldVariant::Outlined)
            .OnChanged([name_input](const TextEditingValue& v) mutable { name_input = v; }),
        err_msg.Get().empty()
            ? View{}
            : Text(err_msg.Get(), TextRole::Label).Style(TextStyle{.foreground = sp.danger}),
    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch));

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton("开始导入", ButtonTone::Primary, submit, true, icons::DownloadIcon()),
    }.With(Spacing(10.0F));

    return DialogCardShell("导入 Luban 环境", std::move(body), std::move(actions), 480.0F)
        .Key("luban-import-dialog");
}

[[huxerui::composable]]
View LubanExportDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    std::string env_name
) {
    auto format_opt = UseState(std::string("luban"));
    auto path_input = UseState(TextEditingValue::FromText("./" + env_name + ".luban"));
    auto size_input = UseState(TextEditingValue::FromText(""));
    auto boot_opt = UseState(std::string{});
    auto err_msg = UseState(std::string{});

    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const std::string fmt = format_opt.Get();

    const std::vector<std::pair<std::string, std::string>> formats{
        {"luban", "luban (.luban)"},
        {"tar", "tar (Podman/容器)"},
        {"qcow2", "qcow2 (虚拟机)"},
        {"iso", "iso (引导镜像)"},
    };
    const std::vector<std::pair<std::string, std::string>> boot_opts{
        {"", "默认"}, {"uefi", "uefi"}, {"bios", "bios"}
    };

    auto submit = [ctx, store, tasks, toast, env_name, format_opt, path_input, size_input, boot_opt, err_msg]() mutable {
        const std::string out_path = detail::TrimLubanText(path_input.Get().text);
        if (out_path.empty()) {
            err_msg = std::string("请输入导出文件路径");
            return;
        }
        LubanExportOptions opts{
            .name = env_name,
            .file_path = out_path,
            .format = format_opt.Get(),
            .size = detail::TrimLubanText(size_input.Get().text),
            .boot = boot_opt.Get(),
        };
        ctx.Dismiss();
        store->ExportLubanEnvAsync(tasks, toast, std::move(opts));
    };

    View body = ScrollView(
        Column {
            Text("导出格式 (--format)", TextRole::Label),
            LubanOptionChips(
                formats,
                fmt,
                [format_opt, path_input, env_name](std::string v) mutable {
                    format_opt = v;
                    const std::string ext = (v == "luban" || v == "tar" || v == "qcow2" || v == "iso")
                                                ? ("." + v)
                                                : ".luban";
                    path_input = TextEditingValue::FromText("./" + env_name + ext);
                },
                "luban-exp-fmt"
            ),
            TextField(path_input)
                .Label("导出文件路径 *")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([path_input, err_msg](const TextEditingValue& v) mutable {
                    path_input = v;
                    err_msg = std::string{};
                }),
            (fmt == "tar")
                ? Card(
                      Column {
                          Text("容器互操作提示 (Podman / Docker)", TextRole::Label)
                              .Style(TextStyle{
                                  .font = Font::System(12.5F).WithWeight(FontWeight::SemiBold),
                                  .foreground = sp.info,
                              }),
                          SelectionArea(
                              Text(
                                  "podman import " + detail::DisplayVal(path_input.Get().text, "./" + env_name + ".tar") +
                                      " luban-" + env_name + ":latest",
                                  TextRole::Body
                              ).Style(TextStyle{.font = Font::Monospace(12.0F), .foreground = theme.colors.on_surface})
                          ),
                      }.With(Spacing(4.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                      10.0F
                  )
                : View{},
            (fmt == "qcow2" || fmt == "iso")
                ? TextField(size_input)
                      .Label("磁盘镜像容量 (--size，可选)")
                      .Placeholder("例如: 8G")
                      .Variant(TextFieldVariant::Outlined)
                      .OnChanged([size_input](const TextEditingValue& v) mutable { size_input = v; })
                : View{},
            (fmt == "qcow2" || fmt == "iso")
                ? Column {
                      Text("引导方式 (--boot)", TextRole::Label),
                      LubanOptionChips(
                          boot_opts,
                          boot_opt.Get(),
                          [boot_opt](std::string v) mutable { boot_opt = std::move(v); },
                          "luban-exp-boot"
                      ),
                  }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))
                : View{},
            err_msg.Get().empty()
                ? View{}
                : Text(err_msg.Get(), TextRole::Label).Style(TextStyle{.foreground = sp.danger}),
        }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Frame{.max_height = 330.0F}, ScrollBar{});

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton("导出镜像", ButtonTone::Primary, submit, true, icons::DownloadIcon()),
    }.With(Spacing(10.0F));

    return DialogCardShell("导出 Luban 环境 (" + env_name + ")", std::move(body), std::move(actions), 500.0F)
        .Key("luban-export-dialog");
}

[[huxerui::composable]]
View LubanUpgradeDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    std::string env_name
) {
    auto to_input = UseState(TextEditingValue::FromText(""));
    auto dry_run_opt = UseState(false);

    auto submit = [ctx, store, tasks, toast, env_name, to_input, dry_run_opt]() {
        ctx.Dismiss();
        store->UpgradeLubanEnvAsync(
            tasks, toast, env_name, detail::TrimLubanText(to_input.Get().text), dry_run_opt.Get());
    };

    View body = Column {
        Text("升级环境 `" + env_name + "` 的基础发行版层并生成可回滚的新代数。", TextRole::Body),
        TextField(to_input)
            .Label("目标版本 (--to，可留空)")
            .Placeholder("留空则升级到当前通道最新版本")
            .Variant(TextFieldVariant::Outlined)
            .OnChanged([to_input](const TextEditingValue& v) mutable { to_input = v; }),
        Checkbox("仅预览计划 (--dry-run)", dry_run_opt.Get())
            .OnChanged([dry_run_opt](bool v) mutable { dry_run_opt = v; }),
    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch));

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton(
            dry_run_opt.Get() ? "预览升级" : "执行升级",
            ButtonTone::Primary,
            submit,
            true,
            icons::RefreshIcon()
        ),
    }.With(Spacing(10.0F));

    return DialogCardShell("升级 Luban 环境 (" + env_name + ")", std::move(body), std::move(actions), 480.0F)
        .Key("luban-upgrade-dialog");
}

[[huxerui::composable]]
View LubanRollbackDialogContent(
    DialogContext ctx,
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    std::string env_name
) {
    auto gen_input = UseState(TextEditingValue::FromText(""));
    const std::string& history_text = store->selected_luban_history.Get();

    auto submit = [ctx, store, tasks, toast, env_name, gen_input]() {
        ctx.Dismiss();
        store->RollbackLubanEnvAsync(tasks, toast, env_name, detail::TrimLubanText(gen_input.Get().text));
    };

    View body = ScrollView(
        Column {
            Text("环境 `" + env_name + "` 的历史代记录:", TextRole::Label),
            CodeBox(history_text, 80.0F, 150.0F),
            TextField(gen_input)
                .Label("回滚目标代数 (--to <N>，留空回滚到上一代)")
                .Placeholder("例如: 1")
                .Variant(TextFieldVariant::Outlined)
                .OnChanged([gen_input](const TextEditingValue& v) mutable { gen_input = v; })
                .OnSubmitted(submit),
        }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Frame{.max_height = 330.0F}, ScrollBar{});

    View actions = Row {
        Spacer{}.With(Grow(1.0F)),
        ActionButton("取消", ButtonTone::Secondary, [ctx] { ctx.Dismiss(); }),
        ActionButton("确认回滚", ButtonTone::Danger, submit, true, icons::RefreshIcon()),
    }.With(Spacing(10.0F));

    return DialogCardShell("回滚 Luban 环境 (" + env_name + ")", std::move(body), std::move(actions), 500.0F)
        .Key("luban-rollback-dialog");
}

[[huxerui::composable]]
View LubanNotInstalledView(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    return Column {
        Card(
            Column {
                Row {
                    Image(icons::LubanIcon())
                        .Fit(ImageFit::Contain)
                        .Tint(theme.colors.primary)
                        .With(Frame{.width = 28.0F, .height = 28.0F}),
                    Column {
                        Text("未检测到 Luban CLI (`luban`)", TextRole::Title)
                            .Style(TextStyle{
                                .font = Font::System(17.0F).WithWeight(FontWeight::Bold),
                                .foreground = theme.colors.on_surface,
                            }),
                        Text(
                            "LubanOS 负责容器化沙箱、独立 Rootfs 发行版与 QEMU/KVM 桌面环境；安装 `xim:luban` 后即可在此统一编排管理。",
                            TextRole::Body
                        ).Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
                    }.With(Spacing(4.0F), Grow(1.0F)),
                }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
                Flow {
                    ActionButton(
                        "安装 luban (xim:luban)",
                        ButtonTone::Primary,
                        [store, tasks, toast] { store->InstallLubanComponentAsync(tasks, toast, "xim:luban"); },
                        true,
                        icons::DownloadIcon()
                    ),
                    ActionButton(
                        "安装 LubanOS 桌面 (xim:luban-os)",
                        ButtonTone::Secondary,
                        [store, tasks, toast] { store->InstallLubanComponentAsync(tasks, toast, "xim:luban-os"); },
                        true,
                        icons::DownloadIcon()
                    ),
                    RefreshButton(
                        "重新检测",
                        [store, tasks] { store->RefreshLuban(tasks, true); },
                        store->loading_luban.Get(),
                        ButtonTone::Secondary,
                        true,
                        false
                    ).Key("luban-redetect-btn"),
                }.With(Spacing(8.0F)),
            }.With(Spacing(14.0F), CrossAlign(CrossAxisAlignment::Stretch)),
            20.0F
        ),
    }.With(Grow(1.0F), Padding(20.0F), Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch));
}

[[huxerui::composable]]
View LubanEnvironmentsTabView(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    DialogHandle dialog,
    ViewHandle filter_handle
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);

    const auto& host = store->luban_host_status.Get();
    const auto& envs = store->luban_envs.Get();
    const std::string selected_name = store->selected_luban_env.Get();
    const auto status_opt = store->selected_luban_status.Get();
    const auto config_opt = store->selected_luban_config.Get();
    const std::string& history_text = store->selected_luban_history.Get();
    const std::string& logs_text = store->selected_luban_logs.Get();
    const bool loading_detail = store->loading_luban_detail.Get();
    const std::string& run_output = store->luban_run_output.Get();
    const bool running_cmd = store->running_luban_cmd.Get();

    auto env_filter = UseState(TextEditingValue::FromText(""));
    auto detail_tab = UseState<std::size_t>(0);
    auto run_cmd_input = UseState(TextEditingValue::FromText("uname -a"));

    const LubanEnvConfigDetail base_cfg = config_opt.value_or(LubanEnvConfigDetail{});
    auto cfg_mode = UseState(base_cfg.mode);
    auto cfg_persist = UseState(base_cfg.persist);
    auto cfg_observe = UseState(base_cfg.observe_level);
    auto cfg_fetch = UseState(base_cfg.fetch_policy);
    auto cfg_policy = UseState(TextEditingValue::FromText(base_cfg.policy_extends));
    auto cfg_proxy = UseState(TextEditingValue::FromText(base_cfg.proxy));
    auto cfg_allow = UseState(TextEditingValue::FromText(""));
    auto cfg_mount = UseState(TextEditingValue::FromText(""));

    const std::string sync_fingerprint = selected_name + "|" + base_cfg.name + "|" + base_cfg.mode + "|" +
                                         base_cfg.persist + "|" + base_cfg.observe_level + "|" +
                                         base_cfg.fetch_policy + "|" + base_cfg.policy_extends + "|" + base_cfg.proxy;
    const std::size_t cfg_hash = std::hash<std::string>{}(sync_fingerprint);

    Lifecycle([base_cfg, cfg_mode, cfg_persist, cfg_observe, cfg_fetch, cfg_policy, cfg_proxy,
               cfg_allow, cfg_mount]() mutable {
        cfg_mode = base_cfg.mode;
        cfg_persist = base_cfg.persist;
        cfg_observe = base_cfg.observe_level;
        cfg_fetch = base_cfg.fetch_policy;
        cfg_policy = TextEditingValue::FromText(base_cfg.policy_extends);
        cfg_proxy = TextEditingValue::FromText(base_cfg.proxy);
        cfg_allow = TextEditingValue::FromText("");
        cfg_mount = TextEditingValue::FromText("");
    }, cfg_hash);

    const LubanEnvironment* selected_env = nullptr;
    for (const auto& e : envs) {
        if (e.name == selected_name) {
            selected_env = &e;
            break;
        }
    }

    std::vector<View> host_items;
    host_items.push_back(StatusBadge(
        host.backend_name.empty() ? std::string("后端待初始化") : ("后端 " + host.backend_name),
        host.backend_name.empty() ? sp.warning : sp.info,
        host.backend_name.empty() ? sp.warning_bg : sp.info_bg
    ));
    host_items.push_back(StatusBadge(
        host.sandbox_ready ? "Sandbox 就绪" : "Sandbox 未就绪",
        host.sandbox_ready ? sp.success : sp.danger,
        host.sandbox_ready ? sp.success_bg : sp.danger_bg
    ));
    host_items.push_back(StatusBadge(
        host.machine_ready ? "KVM 就绪" : "KVM 未就绪",
        host.machine_ready ? sp.success : sp.warning,
        host.machine_ready ? sp.success_bg : sp.warning_bg
    ));
    host_items.push_back(StatusBadge(
        host.desktop_ready ? "Desktop 就绪" : "Desktop 未安装",
        host.desktop_ready ? sp.success : theme.colors.on_surface_variant,
        host.desktop_ready ? sp.success_bg : sp.muted_bg
    ));
    if (!host.missing_tools.empty()) {
        const auto& missing = host.missing_tools.front();
        const std::string pkg_spec = missing.package.empty() ? ("xim:" + missing.name) : missing.package;
        host_items.push_back(
            ActionButton(
                "补齐 " + missing.name,
                ButtonTone::Secondary,
                [store, tasks, toast, pkg_spec] {
                    store->InstallLubanComponentAsync(tasks, toast, pkg_spec);
                },
                true,
                icons::DownloadIcon(),
                true
            ).Key("luban-missing-tool-btn")
        );
    }
    host_items.push_back(
        ActionButton(
            "新建环境",
            ButtonTone::Primary,
            [dialog, store, tasks, toast] {
                dialog.Show([store, tasks, toast](DialogContext ctx) {
                    return NewLubanEnvDialogContent(ctx, store, tasks, toast);
                });
            },
            true,
            icons::PlusIcon(),
            true
        )
    );
    host_items.push_back(
        ActionButton(
            "导入镜像",
            ButtonTone::Secondary,
            [dialog, store, tasks, toast] {
                dialog.Show([store, tasks, toast](DialogContext ctx) {
                    return LubanImportDialogContent(ctx, store, tasks, toast);
                });
            },
            true,
            icons::DownloadIcon(),
            true
        )
    );
    host_items.push_back(
        ActionButton(
            "初始化隔离",
            ButtonTone::Secondary,
            [store, tasks, toast] { store->SetupLubanHostAsync(tasks, toast); },
            true,
            icons::WrenchIcon(),
            true
        )
    );

    View host_strip = Row {
        Flow(std::move(host_items)).With(Spacing(6.0F)).With(Grow(1.0F)),
    }.With(
        Padding(EdgeInsets::Symmetric(16.0F, 8.0F)),
        Background(sp.card_elevated_bg),
        CrossAlign(CrossAxisAlignment::Center)
    );

    const std::string needle = detail::ToLowerCopy(detail::TrimLubanText(env_filter.Get().text));
    std::vector<View> env_cards;
    for (std::size_t i = 0; i < envs.size(); ++i) {
        const auto& env = envs[i];
        if (!needle.empty()) {
            const std::string hay = detail::ToLowerCopy(env.name + " " + env.edition + " " + env.kind);
            if (hay.find(needle) == std::string::npos) continue;
        }
        const bool is_sel = (env.name == selected_name);

        std::vector<View> card_badges;
        if (!env.kind.empty()) {
            card_badges.push_back(StatusBadge(env.kind, sp.info, sp.info_bg));
        }
        if (!env.edition.empty()) {
            card_badges.push_back(StatusBadge(env.edition, theme.colors.on_surface_variant, sp.muted_bg));
        }
        card_badges.push_back(StatusBadge(
            std::to_string(env.packages) + " 包",
            theme.colors.on_surface_variant,
            sp.muted_bg
        ));
        for (const auto& dk : env.desktops) {
            if (!dk.empty()) {
                card_badges.push_back(StatusBadge("桌面:" + dk, sp.info, sp.info_bg));
            }
        }

        env_cards.push_back(
            Column {
                Row {
                    Text(env.name, TextRole::Title)
                        .Style(TextStyle{
                            .font = Font::System(14.5F).WithWeight(FontWeight::Bold),
                            .foreground = is_sel ? theme.colors.primary : theme.colors.on_surface,
                        })
                        .With(Grow(1.0F)),
                    StatusBadge(
                        env.running ? "运行中" : "已停止",
                        env.running ? sp.success : theme.colors.on_surface_variant,
                        env.running ? sp.success_bg : sp.muted_bg
                    ),
                }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                Flow(std::move(card_badges)).With(Spacing(5.0F)),
                env.shell.empty()
                    ? View{}
                    : Text("Shell: " + env.shell, TextRole::Label)
                          .Style(TextStyle{.font = Font::Monospace(11.5F), .foreground = theme.colors.on_surface_variant}),
            }.With(
                Padding(12.0F),
                Spacing(6.0F),
                Background(is_sel ? theme.colors.primary_container : sp.card_bg),
                Border{is_sel ? theme.colors.primary : sp.border_soft, is_sel ? 1.5F : 1.0F},
                CornerRadius(11.0F),
                Focusable(true),
                PointerCursor(PointerCursorKind::Hand),
                CrossAlign(CrossAxisAlignment::Stretch)
            )
            .OnClick([store, tasks, name = env.name] {
                store->LoadLubanDetail(tasks, name, false);
            })
            .Key("luban-env-" + env.name + "-" + std::to_string(i))
        );
    }

    View left_pane = Column {
        SearchBox(env_filter, {
            .placeholder = "筛选环境...",
            .focus_handle = filter_handle,
            .input_key = "luban-filter-input",
        }),
        env_cards.empty()
            ? EmptyState(
                  icons::LubanIcon(),
                  envs.empty() ? "暂无 Luban 环境" : "无匹配环境",
                  envs.empty() ? "可点击上方「新建环境」或切换到目录页从模板创建" : "尝试更换筛选关键词"
              )
            : View(ScrollView(
                       Column(std::move(env_cards)).With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))
                   ).ScrollAxis(Axis::Vertical)
                       .With(Grow(1.0F), ScrollBar{})),
    }.With(Grow(1.0F), Padding(12.0F), Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch));

    View right_pane = View{};
    if (selected_env == nullptr) {
        right_pane = EmptyState(icons::LubanIcon(), "选择一个 Luban 环境查看详情");
    } else {
        const LubanEnvironment cur = *selected_env;
        const std::size_t dtab = detail_tab.Get();
        const LubanEnvStatusDetail st_detail = status_opt.value_or(LubanEnvStatusDetail{});

        View tab_body = View{};
        if (dtab == 0) {
            const std::vector<std::pair<std::string, std::string>> edit_modes{
                {"sandbox", "sandbox"}, {"machine", "machine"}
            };
            const std::vector<std::pair<std::string, std::string>> edit_persists{
                {"all", "all"}, {"home", "home"}, {"none", "none"}
            };
            const std::vector<std::pair<std::string, std::string>> edit_obs{
                {"off", "off"}, {"basic", "basic"}, {"standard", "standard"}, {"full", "full"}
            };
            const std::vector<std::pair<std::string, std::string>> edit_fetch{
                {"auto", "auto"}, {"ask", "ask"}, {"deny", "deny"}
            };

            const std::string desktop_summary = !st_detail.desktop.empty()
                                                    ? st_detail.desktop
                                                    : detail::JoinCsvItems(
                                                          st_detail.desktops.empty() ? cur.desktops : st_detail.desktops);

            tab_body = ScrollView(
                Column {
                    KeyValueRow("模式 / 持久化",
                                detail::DisplayVal(st_detail.mode.empty() ? base_cfg.mode : st_detail.mode) + " · " +
                                    detail::DisplayVal(st_detail.persist.empty() ? base_cfg.persist : st_detail.persist)),
                    KeyValueRow("入口 / 网络",
                                detail::DisplayVal(st_detail.enter) + " · " +
                                    detail::DisplayVal(st_detail.network.empty() ? base_cfg.net : st_detail.network)),
                    KeyValueRow("Shell / 桌面",
                                detail::DisplayVal(st_detail.shell.empty() ? cur.shell : st_detail.shell) + " · " +
                                    detail::DisplayVal(desktop_summary)),
                    KeyValueRow("载体 / 挂载",
                                detail::DisplayVal(st_detail.carrier.empty() ? cur.carrier : st_detail.carrier) +
                                    (base_cfg.mounts.empty() ? "" : (" · " + detail::JoinCsvItems(base_cfg.mounts))),
                                false,
                                true),
                    st_detail.notes.empty()
                        ? View{}
                        : KeyValueRow("环境说明", detail::JoinCsvItems(st_detail.notes)),
                    st_detail.unmet.empty()
                        ? View{}
                        : KeyValueRow("未满足依赖", detail::JoinCsvItems(st_detail.unmet), true),
                    Divider(),
                    Text("在线配置修改 (`luban config " + cur.name + "`)", TextRole::Label)
                        .Style(TextStyle{.font = Font::System(13.0F).WithWeight(FontWeight::SemiBold)}),
                    Text("运行模式 (--mode)", TextRole::Label),
                    LubanOptionChips(
                        edit_modes, cfg_mode.Get(), [cfg_mode](std::string v) mutable { cfg_mode = std::move(v); }, "cfg-mode"),
                    Text("持久化 (--persist)", TextRole::Label),
                    LubanOptionChips(
                        edit_persists,
                        cfg_persist.Get(),
                        [cfg_persist](std::string v) mutable { cfg_persist = std::move(v); },
                        "cfg-persist"
                    ),
                    Text("审计观测 (--observe)", TextRole::Label),
                    LubanOptionChips(
                        edit_obs,
                        cfg_observe.Get(),
                        [cfg_observe](std::string v) mutable { cfg_observe = std::move(v); },
                        "cfg-obs"
                    ),
                    Text("包拉取策略 (--fetch)", TextRole::Label),
                    LubanOptionChips(
                        edit_fetch,
                        cfg_fetch.Get(),
                        [cfg_fetch](std::string v) mutable { cfg_fetch = std::move(v); },
                        "cfg-fetch"
                    ),
                    TextField(cfg_policy)
                        .Label("隔离策略 (--policy)")
                        .Placeholder("例如: dev / strict / locked")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([cfg_policy](const TextEditingValue& v) mutable { cfg_policy = v; }),
                    TextField(cfg_proxy)
                        .Label("网络代理 (--proxy)")
                        .Placeholder("例如: socks5h://127.0.0.1:1080")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([cfg_proxy](const TextEditingValue& v) mutable { cfg_proxy = v; }),
                    TextField(cfg_allow)
                        .Label("新增授权能力 (--allow，逗号分隔)")
                        .Placeholder("例如: display, gpu")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([cfg_allow](const TextEditingValue& v) mutable { cfg_allow = v; }),
                    TextField(cfg_mount)
                        .Label("追加宿主挂载 (--mount，逗号分隔)")
                        .Placeholder("例如: /path/src:/workspace:rw")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([cfg_mount](const TextEditingValue& v) mutable { cfg_mount = v; }),
                    Row {
                        ActionButton(
                            "保存配置",
                            ButtonTone::Primary,
                            [store, tasks, toast, cur_name = cur.name, base_cfg, cfg_mode, cfg_persist, cfg_observe,
                             cfg_fetch, cfg_policy, cfg_proxy, cfg_allow, cfg_mount] {
                                LubanConfigPatch patch;
                                if (!cfg_mode.Get().empty() && cfg_mode.Get() != base_cfg.mode) {
                                    patch.mode = cfg_mode.Get();
                                }
                                if (!cfg_persist.Get().empty() && cfg_persist.Get() != base_cfg.persist) {
                                    patch.persist = cfg_persist.Get();
                                }
                                if (!cfg_observe.Get().empty() && cfg_observe.Get() != base_cfg.observe_level) {
                                    patch.observe = cfg_observe.Get();
                                }
                                if (!cfg_fetch.Get().empty() && cfg_fetch.Get() != base_cfg.fetch_policy) {
                                    patch.fetch = cfg_fetch.Get();
                                }
                                const std::string pol = detail::TrimLubanText(cfg_policy.Get().text);
                                if (!pol.empty() && pol != base_cfg.policy_extends) {
                                    patch.policy = pol;
                                }
                                const std::string prx = detail::TrimLubanText(cfg_proxy.Get().text);
                                if (prx != base_cfg.proxy && !prx.empty()) {
                                    patch.proxy = prx;
                                }
                                patch.allow = detail::SplitCsvItems(cfg_allow.Get().text);
                                patch.mounts = detail::SplitCsvItems(cfg_mount.Get().text);
                                store->ConfigureLubanEnvAsync(tasks, toast, cur_name, std::move(patch));
                            },
                            true,
                            icons::CheckIcon(),
                            true
                        ),
                        Spacer{}.With(Grow(1.0F)),
                    },
                }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ).ScrollAxis(Axis::Vertical)
                .With(Grow(1.0F), ScrollBar{});
        } else if (dtab == 1) {
            auto run_submit = [store, tasks, toast, cur_name = cur.name, run_cmd_input]() {
                store->RunLubanCommandInlineAsync(
                    tasks, toast, cur_name, detail::SplitLubanCommandArgv(run_cmd_input.Get().text));
            };

            tab_body = ScrollView(
                Column {
                    SectionHeader(
                        "环境内指令运行 (`luban run " + cur.name + "`)",
                        "向隔离环境传递命令并即时收集标准输出"
                    ),
                    Row {
                        TextField(run_cmd_input)
                            .Label("命令行")
                            .Placeholder("例如: xlings --version 或 bash -lc 'uname -a'")
                            .Variant(TextFieldVariant::Outlined)
                            .OnChanged([run_cmd_input](const TextEditingValue& v) mutable { run_cmd_input = v; })
                            .OnSubmitted(run_submit)
                            .With(Grow(1.0F)),
                        ActionButton(
                            "运行命令",
                            ButtonTone::Primary,
                            run_submit,
                            true,
                            icons::PlayIcon(),
                            false,
                            running_cmd
                        ).Key("luban-run-btn"),
                    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Flow {
                        ActionButton(
                            "uname -a",
                            ButtonTone::Secondary,
                            [run_cmd_input]() mutable { run_cmd_input = TextEditingValue::FromText("uname -a"); },
                            true,
                            std::nullopt,
                            true
                        ),
                        ActionButton(
                            "xlings --version",
                            ButtonTone::Secondary,
                            [run_cmd_input]() mutable {
                                run_cmd_input = TextEditingValue::FromText("xlings --version");
                            },
                            true,
                            std::nullopt,
                            true
                        ),
                        ActionButton(
                            "env",
                            ButtonTone::Secondary,
                            [run_cmd_input]() mutable { run_cmd_input = TextEditingValue::FromText("env"); },
                            true,
                            std::nullopt,
                            true
                        ),
                    }.With(Spacing(6.0F)),
                    CodeBox(run_output, 120.0F, 220.0F),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ).ScrollAxis(Axis::Vertical)
                .With(Grow(1.0F), ScrollBar{});
        } else {
            tab_body = ScrollView(
                Column {
                    SectionHeader(
                        "环境代历史 (`luban history " + cur.name + "`)",
                        "记录发行版升级、快照和回滚代数",
                        RefreshButton(
                            "刷新详情",
                            [store, tasks, cur_name = cur.name] {
                                store->LoadLubanDetail(tasks, cur_name, true);
                            },
                            loading_detail
                        ).Key("luban-refresh-detail-btn")
                    ),
                    CodeBox(history_text, 76.0F, 130.0F),
                    SectionHeader(
                        "聚合日志 (`luban logs " + cur.name + "`)",
                        "隔离事件与最近运行日志"
                    ),
                    CodeBox(logs_text, 96.0F, 190.0F),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ).ScrollAxis(Axis::Vertical)
                .With(Grow(1.0F), ScrollBar{});
        }

        right_pane = Column {
            Card(
                Column {
                    Row {
                        Text(cur.name, TextRole::Title)
                            .Style(TextStyle{
                                .font = Font::System(18.0F).WithWeight(FontWeight::Bold),
                                .foreground = theme.colors.on_surface,
                            }),
                        StatusBadge(
                            cur.running ? "运行中" : "已停止",
                            cur.running ? sp.success : theme.colors.on_surface_variant,
                            cur.running ? sp.success_bg : sp.muted_bg
                        ),
                        cur.kind.empty() ? View{} : StatusBadge(cur.kind, sp.info, sp.info_bg),
                        Spacer{}.With(Grow(1.0F)),
                        LoadingIndicatorSlot(loading_detail, 16.0F),
                    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Flow {
                        ActionButton(
                            "停止",
                            ButtonTone::Secondary,
                            [store, tasks, toast, name = cur.name] { store->StopLubanEnvAsync(tasks, toast, name); },
                            true,
                            icons::TrashIcon(),
                            true
                        ),
                        ActionButton(
                            "升级",
                            ButtonTone::Secondary,
                            [dialog, store, tasks, toast, name = cur.name] {
                                dialog.Show([store, tasks, toast, name](DialogContext ctx) {
                                    return LubanUpgradeDialogContent(ctx, store, tasks, toast, name);
                                });
                            },
                            true,
                            icons::RefreshIcon(),
                            true
                        ),
                        ActionButton(
                            "回滚",
                            ButtonTone::Secondary,
                            [dialog, store, tasks, toast, name = cur.name] {
                                dialog.Show([store, tasks, toast, name](DialogContext ctx) {
                                    return LubanRollbackDialogContent(ctx, store, tasks, toast, name);
                                });
                            },
                            true,
                            icons::WrenchIcon(),
                            true
                        ),
                        ActionButton(
                            "导出镜像",
                            ButtonTone::Secondary,
                            [dialog, store, tasks, toast, name = cur.name] {
                                dialog.Show([store, tasks, toast, name](DialogContext ctx) {
                                    return LubanExportDialogContent(ctx, store, tasks, toast, name);
                                });
                            },
                            true,
                            icons::DownloadIcon(),
                            true
                        ),
                        ActionButton(
                            "删除",
                            ButtonTone::Danger,
                            [dialog, store, tasks, toast, name = cur.name] {
                                dialog.Show(
                                    "删除 Luban 环境",
                                    "确定要删除环境 `" + name + "` 及其持久化数据吗？此操作不可撤销。",
                                    "删除",
                                    "取消",
                                    [store, tasks, toast, name] { store->RemoveLubanEnvAsync(tasks, toast, name); },
                                    {}
                                );
                            },
                            true,
                            icons::TrashIcon(),
                            true
                        ),
                    }.With(Spacing(6.0F)),
                }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                12.0F
            ),
            Tabs({"概览与配置", "执行命令", "历史与日志"}, dtab)
                .OnChanged([detail_tab](std::size_t idx) mutable { detail_tab = idx; }),
            Card(std::move(tab_body), 14.0F).With(Grow(1.0F)),
        }.With(
            Grow(1.0F),
            Padding(14.0F),
            Spacing(10.0F),
            CrossAlign(CrossAxisAlignment::Stretch)
        );
    }

    return Column {
        std::move(host_strip),
        Divider(),
        ResizableSplit(std::move(left_pane), std::move(right_pane), 260.0F, 210.0F, 420.0F),
    }.With(Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch));
}

[[huxerui::composable]]
View LubanCatalogTabView(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    DialogHandle dialog
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const auto& catalog = store->luban_catalog.Get();

    auto build_cards = [&](const std::vector<LubanCatalogItem>& items, bool is_desktop, std::string key_prefix) {
        std::vector<View> cards;
        for (std::size_t i = 0; i < items.size(); ++i) {
            const auto& item = items[i];
            std::vector<View> badges;
            if (!item.version.empty()) {
                badges.push_back(StatusBadge("v" + item.version, sp.info, sp.info_bg));
            }
            if (!item.status.empty()) {
                const bool stable = (item.status == "stable" || item.status == "ready" || item.status == "available");
                badges.push_back(StatusBadge(
                    item.status,
                    stable ? sp.success : sp.warning,
                    stable ? sp.success_bg : sp.warning_bg
                ));
            }
            if (item.tier > 0) {
                badges.push_back(StatusBadge("Tier " + std::to_string(item.tier), theme.colors.on_surface_variant, sp.muted_bg));
            }
            if (!item.gpu_hint.empty()) {
                badges.push_back(StatusBadge("GPU: " + item.gpu_hint, theme.colors.on_surface_variant, sp.muted_bg));
            }
            for (const auto& hl : item.highlights) {
                if (!hl.empty()) {
                    badges.push_back(StatusBadge(hl, theme.colors.on_surface_variant, sp.muted_bg));
                }
            }

            const std::string title = item.title_zh.empty() ? item.name : (item.title_zh + " (" + item.name + ")");
            const std::string desc = !item.summary_zh.empty() ? item.summary_zh : item.description;

            cards.push_back(
                Column {
                    Row {
                        Column {
                            Text(title, TextRole::Title)
                                .Style(TextStyle{
                                    .font = Font::System(15.0F).WithWeight(FontWeight::Bold),
                                    .foreground = theme.colors.on_surface,
                                }),
                            item.ref.empty()
                                ? View{}
                                : Text(item.ref, TextRole::Label)
                                      .Style(TextStyle{.font = Font::Monospace(11.5F), .foreground = theme.colors.on_surface_variant}),
                        }.With(Spacing(3.0F), Grow(1.0F)),
                        ActionButton(
                            is_desktop ? "创建桌面环境" : "以此创建环境",
                            ButtonTone::Primary,
                            [dialog, store, tasks, toast, is_desktop, name = item.name] {
                                dialog.Show([store, tasks, toast, is_desktop, name](DialogContext ctx) {
                                    return NewLubanEnvDialogContent(
                                        ctx,
                                        store,
                                        tasks,
                                        toast,
                                        is_desktop ? std::string("desktop") : name,
                                        is_desktop ? name : std::string{}
                                    );
                                });
                            },
                            item.available,
                            icons::PlusIcon(),
                            true
                        ),
                    }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Flow(std::move(badges)).With(Spacing(6.0F)),
                    desc.empty()
                        ? View{}
                        : Text(desc, TextRole::Body)
                              .Style(TextStyle{.font = Font::System(13.0F), .foreground = theme.colors.on_surface_variant}),
                }.With(
                    Padding(12.0F),
                    Spacing(8.0F),
                    Background(theme.colors.surface_container_low),
                    Border{sp.border_soft, 1.0F},
                    CornerRadius(10.0F),
                    CrossAlign(CrossAxisAlignment::Stretch)
                ).Key(key_prefix + item.name + "-" + std::to_string(i))
            );
        }
        return cards;
    };

    auto dist_cards = build_cards(catalog.distributions, false, "luban-dist-");
    auto desk_cards = build_cards(catalog.desktops, true, "luban-desk-");

    return ScrollView(
        Column {
            Card(
                Column {
                    SectionHeader(
                        "官方发行版 (Distributions · " + std::to_string(catalog.distributions.size()) + ")",
                        "来自 `luban search --json`，支持一键预填生成定制隔离环境"
                    ),
                    dist_cards.empty()
                        ? EmptyState(icons::LubanIcon(), "未获取到发行版目录", "点击右上角「刷新 Luban」同步最新官方目录")
                        : View(Column(std::move(dist_cards)).With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader(
                        "官方桌面环境 (Desktops · " + std::to_string(catalog.desktops.size()) + ")",
                        "配合 `xim:luban-os` 提供隔离 Linux 桌面会话"
                    ),
                    desk_cards.empty()
                        ? EmptyState(icons::StoreIcon(), "未获取到桌面目录", "请确认当前架构与 Luban 索引支持情况")
                        : View(Column(std::move(desk_cards)).With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
        }.With(Padding(18.0F), Spacing(14.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Grow(1.0F), ScrollBar{})
        .Key("luban-stack-scroll");
}

[[huxerui::composable]]
View LubanStacksStudioTabView(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    DialogHandle dialog
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const auto clipboard = UseApplication().Clipboard();

    const auto presets = BuiltinLubanStacks();
    const std::string initial_text = presets.empty() ? std::string{} : SerializeLubanStackToml(presets.front());

    auto format_state = UseState(std::string("toml"));
    auto draft_input = UseState(TextEditingValue::FromText(initial_text));

    Lifecycle([store, tasks] {
        if (!store->skip_initial_refresh) {
            store->RefreshSavedLubanStacks(tasks);
        }
    });

    const std::string fmt = format_state.Get();
    const std::string raw_text = draft_input.Get().text;
    const LubanStackParseResult parsed = ParseLubanStackAuto(raw_text);
    const auto& envs = store->luban_envs.Get();
    const auto& cfg_cache = store->luban_config_cache.Get();
    const auto& saved_stacks = store->saved_luban_stacks.Get();
    const bool luban_avail = store->luban_available.Get();
    const bool task_busy = store->task_running.Get();

    const LubanStackPlan plan = parsed.ok
                                    ? BuildLubanStackPlan(parsed.spec, envs, cfg_cache)
                                    : LubanStackPlan{};

    std::vector<View> preset_buttons;
    preset_buttons.reserve(presets.size());
    for (std::size_t i = 0; i < presets.size(); ++i) {
        const auto& st = presets[i];
        preset_buttons.push_back(
            ActionButton(
                st.name + " (" + std::to_string(st.environments.size()) + ")",
                ButtonTone::Secondary,
                [st, format_state, draft_input, toast]() mutable {
                    const std::string next = (format_state.Get() == "json")
                                                 ? SerializeLubanStackJson(st)
                                                 : SerializeLubanStackToml(st);
                    draft_input = TextEditingValue::FromText(next);
                    toast.Show("已载入模板: " + st.name);
                },
                true,
                std::nullopt,
                true
            ).Key("luban-stack-preset-" + std::to_string(i))
        );
    }

    std::vector<View> saved_rows;
    saved_rows.reserve(saved_stacks.size());
    for (std::size_t i = 0; i < saved_stacks.size(); ++i) {
        const auto& info = saved_stacks[i];
        std::string fmt_upper = detail::ToLowerCopy(info.format) == "toml" ? "TOML" : "JSON";
        saved_rows.push_back(
            Row {
                Column {
                    Flow {
                        StatusBadge(fmt_upper, sp.info, sp.info_bg),
                        Text(info.display_name, TextRole::Body)
                            .Style(TextStyle{
                                .font = Font::System(13.5F).WithWeight(FontWeight::SemiBold),
                                .foreground = theme.colors.on_surface,
                            }),
                        StatusBadge(
                            std::to_string(info.env_count) + " 环境",
                            theme.colors.on_surface_variant,
                            sp.muted_bg
                        ),
                    }.With(Spacing(6.0F)),
                    info.description.empty()
                        ? View{}
                        : Text(info.description, TextRole::Label)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
                }.With(Spacing(4.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                Flow {
                    ActionButton(
                        "加载",
                        ButtonTone::Secondary,
                        [info, format_state, draft_input, toast]() mutable {
                            const auto content = LoadLubanStackFile(info.path);
                            if (!content.has_value()) {
                                toast.Show("读取蓝图文件失败: " + info.filename);
                                return;
                            }
                            format_state = (info.format == "json") ? "json" : "toml";
                            draft_input = TextEditingValue::FromText(*content);
                            toast.Show("已加载本地蓝图: " + info.display_name);
                        },
                        true,
                        std::nullopt,
                        true
                    ).Key("luban-saved-load-" + std::to_string(i)),
                    ActionButton(
                        "删除",
                        ButtonTone::Danger,
                        [dialog, store, tasks, toast, info] {
                            dialog.Show(
                                "删除本地蓝图",
                                "确定删除本地蓝图 `" + info.filename + "` 吗？",
                                "删除",
                                "取消",
                                [store, tasks, toast, info] {
                                    store->DeleteSavedLubanStack(tasks, toast, info.path, info.display_name);
                                },
                                {}
                            );
                        },
                        true,
                        icons::TrashIcon(),
                        true
                    ).Key("luban-saved-del-" + std::to_string(i)),
                }.With(Spacing(6.0F)),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                Spacing(8.0F),
                CrossAlign(CrossAxisAlignment::Center),
                Background(theme.colors.surface_container_low),
                CornerRadius(8.0F)
            ).Key("luban-saved-stack-" + info.id + "-" + std::to_string(i))
        );
    }

    std::vector<View> err_lines;
    if (!parsed.ok) {
        err_lines.push_back(
            Text("清单校验未通过:", TextRole::Label)
                .Style(TextStyle{
                    .font = Font::System(12.5F).WithWeight(FontWeight::SemiBold),
                    .foreground = sp.danger,
                })
        );
        for (std::size_t i = 0; i < parsed.errors.size(); ++i) {
            err_lines.push_back(
                Text("• " + parsed.errors[i], TextRole::Label)
                    .Style(TextStyle{.foreground = sp.danger})
                    .Key("luban-stack-err-" + std::to_string(i))
            );
        }
    }

    View validation_box = parsed.ok
                              ? View(Row {
                                    StatusBadge(
                                        "语法有效 (" + parsed.detected_format + ") · " +
                                            std::to_string(parsed.spec.environments.size()) + " 个环境",
                                        sp.success,
                                        sp.success_bg
                                    ).Key("luban-stack-valid-badge"),
                                    Spacer{}.With(Grow(1.0F)),
                                })
                              : View(Column(std::move(err_lines))
                                    .With(
                                        Padding(10.0F),
                                        Spacing(4.0F),
                                        Background(sp.danger_bg),
                                        Border{WithAlpha(sp.danger, 0.35F), 1.0F},
                                        CornerRadius(8.0F),
                                        CrossAlign(CrossAxisAlignment::Stretch)
                                    )
                                    .Key("luban-stack-errors"));

    std::vector<View> plan_rows;
    if (parsed.ok) {
        plan_rows.reserve(plan.actions.size());
        for (std::size_t i = 0; i < plan.actions.size(); ++i) {
            const auto& act = plan.actions[i];
            std::string badge_txt = "+ 新建";
            Color fg = sp.success;
            Color bg = sp.success_bg;
            if (act.kind == LubanStackActionKind::Configure) {
                badge_txt = "~ 更新配置";
                fg = sp.warning;
                bg = sp.warning_bg;
            } else if (act.kind == LubanStackActionKind::UpToDate) {
                badge_txt = "= 已就绪";
                fg = sp.info;
                bg = sp.info_bg;
            }
            plan_rows.push_back(
                Column {
                    Flow {
                        StatusBadge(badge_txt, fg, bg),
                        Text(act.env_name, TextRole::Body)
                            .Style(TextStyle{
                                .font = Font::Monospace(13.0F).WithWeight(FontWeight::SemiBold),
                                .foreground = theme.colors.on_surface,
                            }),
                    }.With(Spacing(8.0F)),
                    Text(act.summary, TextRole::Label)
                        .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
                }.With(
                    Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                    Spacing(4.0F),
                    Background(theme.colors.surface_container_low),
                    CornerRadius(8.0F),
                    CrossAlign(CrossAxisAlignment::Stretch)
                ).Key("luban-plan-act-" + act.env_name + "-" + std::to_string(i))
            );
        }
    }

    return ScrollView(
        Column {
            Card(
                Column {
                    SectionHeader(
                        "蓝图模板与本地库",
                        "本地蓝图目录: ~/.config/hxlings/luban-stacks · 支持 TOML 与 JSON 双向切换",
                        ActionButton(
                            "重载本地库",
                            ButtonTone::Secondary,
                            [store, tasks] { store->RefreshSavedLubanStacks(tasks); },
                            !store->skip_initial_refresh,
                            icons::RefreshIcon(),
                            true
                        )
                    ),
                    Text("内置精选模板", TextRole::Label),
                    Flow(std::move(preset_buttons)).With(Spacing(8.0F)),
                    Divider(),
                    Text(
                        "已保存本地蓝图 (" + std::to_string(saved_stacks.size()) + ")",
                        TextRole::Label
                    ),
                    saved_rows.empty()
                        ? Text("暂无本地保存蓝图，可在下方编辑有效清单后点击「保存到本地」", TextRole::Label)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        : View(Column(std::move(saved_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader(
                        "声明式清单编辑器 (TOML / JSON)",
                        "支持注释 TOML 与标准 JSON 格式实时解析与无损互转 (`hxlings.luban-stack/v1`)"
                    ),
                    Flow {
                        ActionButton(
                            "TOML",
                            fmt == "toml" ? ButtonTone::Primary : ButtonTone::Secondary,
                            [format_state, draft_input, parsed, toast]() mutable {
                                format_state = std::string("toml");
                                if (parsed.ok) {
                                    draft_input = TextEditingValue::FromText(SerializeLubanStackToml(parsed.spec));
                                } else {
                                    toast.Show("已切换为 TOML 模式；当前清单存在语法错误，请修正后转换");
                                }
                            },
                            true,
                            std::nullopt,
                            true
                        ).Key("luban-stack-fmt-toml"),
                        ActionButton(
                            "JSON",
                            fmt == "json" ? ButtonTone::Primary : ButtonTone::Secondary,
                            [format_state, draft_input, parsed, toast]() mutable {
                                format_state = std::string("json");
                                if (parsed.ok) {
                                    draft_input = TextEditingValue::FromText(SerializeLubanStackJson(parsed.spec));
                                } else {
                                    toast.Show("已切换为 JSON 模式；当前清单存在语法错误，请修正后转换");
                                }
                            },
                            true,
                            std::nullopt,
                            true
                        ).Key("luban-stack-fmt-json"),
                        ActionButton(
                            "从当前环境导出",
                            ButtonTone::Secondary,
                            [store, format_state, draft_input, toast]() mutable {
                                const auto exported = ExportEnvironmentsToStack(
                                    "local-snapshot",
                                    store->luban_envs.Get(),
                                    store->luban_config_cache.Get()
                                );
                                if (exported.environments.empty()) {
                                    toast.Show("当前没有可导出的非默认环境");
                                    return;
                                }
                                const std::string serialized = (format_state.Get() == "json")
                                                                   ? SerializeLubanStackJson(exported)
                                                                   : SerializeLubanStackToml(exported);
                                draft_input = TextEditingValue::FromText(serialized);
                                toast.Show("已从 " + std::to_string(exported.environments.size()) + " 个当前环境导出蓝图");
                            },
                            true,
                            icons::CopyIcon(),
                            true
                        ).Key("luban-stack-export-envs-btn"),
                        ActionButton(
                            "格式化",
                            ButtonTone::Secondary,
                            [fmt, draft_input, parsed]() mutable {
                                if (!parsed.ok) return;
                                draft_input = TextEditingValue::FromText(
                                    fmt == "json" ? SerializeLubanStackJson(parsed.spec)
                                                  : SerializeLubanStackToml(parsed.spec)
                                );
                            },
                            parsed.ok,
                            icons::FileCodeIcon(),
                            true
                        ).Key("luban-stack-format-btn"),
                        ActionButton(
                            "保存到本地",
                            ButtonTone::Secondary,
                            [store, tasks, toast, parsed, raw_text, fmt] {
                                if (!parsed.ok) return;
                                (void)store->SaveLubanStackDraft(tasks, toast, parsed.spec.name, raw_text, fmt);
                            },
                            parsed.ok,
                            icons::CheckIcon(),
                            true
                        ).Key("luban-stack-save-btn"),
                        ActionButton(
                            "复制清单",
                            ButtonTone::Ghost,
                            [clipboard, toast, raw_text] {
                                const bool ok = clipboard && clipboard->SupportsSynchronousText() &&
                                                clipboard->WriteText(raw_text);
                                toast.Show(ok ? "已复制蓝图清单" : "复制到剪贴板失败");
                            },
                            !raw_text.empty(),
                            icons::CopyIcon(),
                            true
                        ),
                    }.With(Spacing(8.0F)),
                    TextField(draft_input)
                        .Label(std::string("声明式编排清单 (") + (fmt == "toml" ? "TOML" : "JSON") + ")")
                        .Placeholder("输入 hxlings.luban-stack/v1 清单...")
                        .Variant(TextFieldVariant::Outlined)
                        .LineLimits(TextFieldLineLimits::MultiLine(8, 14))
                        .OnChanged([draft_input](const TextEditingValue& v) mutable { draft_input = v; })
                        .Key("luban-stack-editor"),
                    std::move(validation_box),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader(
                        "实时差异计划 (Plan Diff) 与执行",
                        parsed.ok ? ("栈名称: " + parsed.spec.name + " · 目标环境 " +
                                         std::to_string(parsed.spec.environments.size()) + " 个")
                                  : std::string("修正清单语法后将自动对比本机现有环境并生成执行计划")
                    ),
                    parsed.ok
                        ? Flow {
                              StatusBadge("+ 新建 " + std::to_string(plan.create_count), sp.success, sp.success_bg),
                              StatusBadge("~ 更新配置 " + std::to_string(plan.configure_count), sp.warning, sp.warning_bg),
                              StatusBadge("= 已就绪 " + std::to_string(plan.unchanged_count), sp.info, sp.info_bg),
                          }.With(Spacing(8.0F))
                        : View{},
                    plan_rows.empty()
                        ? View{}
                        : View(Column(std::move(plan_rows)).With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                    Flow {
                        ActionButton(
                            "一键应用编排 (Apply)",
                            ButtonTone::Primary,
                            [dialog, store, tasks, toast, parsed, plan] {
                                if (!parsed.ok) return;
                                const std::string detail =
                                    "栈 `" + parsed.spec.name + "` 将执行: 新建 " +
                                    std::to_string(plan.create_count) + " 个 / 更新配置 " +
                                    std::to_string(plan.configure_count) + " 个 / 跳过已就绪 " +
                                    std::to_string(plan.unchanged_count) + " 个。";
                                dialog.Show(
                                    "应用多环境蓝图",
                                    detail,
                                    "开始执行",
                                    "取消",
                                    [store, tasks, toast, spec = parsed.spec] {
                                        store->ApplyLubanStackAsync(tasks, toast, spec, false);
                                    },
                                    {}
                                );
                            },
                            parsed.ok && luban_avail && !task_busy,
                            icons::PlayIcon(),
                            false
                        ).Key("luban-stack-apply-btn"),
                        ActionButton(
                            "仅预演计划 (Dry-Run)",
                            ButtonTone::Secondary,
                            [store, tasks, toast, parsed] {
                                if (!parsed.ok) return;
                                store->ApplyLubanStackAsync(tasks, toast, parsed.spec, true);
                            },
                            parsed.ok && luban_avail && !task_busy,
                            icons::CheckIcon(),
                            true
                        ),
                        ActionButton(
                            "同步实时配置",
                            ButtonTone::Secondary,
                            [store, tasks, toast, parsed, envs] {
                                if (!parsed.ok) return;
                                std::vector<std::string> targets;
                                for (const auto& item : parsed.spec.environments) {
                                    const bool exists = std::ranges::any_of(
                                        envs, [&](const LubanEnvironment& e) { return e.name == item.name; });
                                    if (exists) targets.push_back(item.name);
                                }
                                if (targets.empty()) {
                                    toast.Show("本机尚未存在本蓝图中的任何环境");
                                    return;
                                }
                                store->SyncLubanConfigsForStackAsync(tasks, toast, std::move(targets));
                            },
                            parsed.ok && luban_avail,
                            icons::RefreshIcon(),
                            true
                        ),
                        ActionButton(
                            "停止栈内环境",
                            ButtonTone::Danger,
                            [dialog, store, tasks, toast, parsed] {
                                if (!parsed.ok) return;
                                dialog.Show(
                                    "停止蓝图内所有运行环境",
                                    "将依次停止 `" + parsed.spec.name + "` 中正在运行的 Luban 环境。",
                                    "全部停止",
                                    "取消",
                                    [store, tasks, toast, spec = parsed.spec] {
                                        store->StopLubanStackAsync(tasks, toast, spec);
                                    },
                                    {}
                                );
                            },
                            parsed.ok && luban_avail && !task_busy,
                            icons::TrashIcon(),
                            true
                        ),
                    }.With(Spacing(8.0F)),
                    CodeBox(
                        "# 容器互操作：导出 Luban 环境为 OCI rootfs tar 并导入 Podman / Docker\n"
                        "luban export <env> ./<env>.tar --format tar && podman import ./<env>.tar luban-<env>:latest",
                        56.0F,
                        84.0F
                    ),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
        }.With(Padding(18.0F), Spacing(14.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Grow(1.0F), ScrollBar{})
        .Key("luban-stack-scroll");
}

[[huxerui::composable]]
View LubanPage() {
    const auto store = UseService<XlingsStore>();
    const TaskScope tasks = UseApplicationTaskScope();
    const TaskScope ui_tasks = UseTaskScope();
    const ToastHandle toast = UseToast();
    const DialogHandle dialog = UseDialog();
    const ThemeSpec& theme = UseTheme();

    const ViewHandle filter_handle = UseViewHandle();

    const std::size_t tab_idx = store->luban_tab.Get();
    const bool luban_avail = store->luban_available.Get();
    const bool luban_os_avail = store->luban_os_available.Get();
    const bool loading_luban = store->loading_luban.Get();

    Lifecycle([store, tasks] {
        if (!store->skip_initial_refresh && store->active_nav.Get() == 4) {
            store->RefreshLuban(tasks, false);
        }
    }, store->active_nav.Get());

    Lifecycle([store, ui_tasks, filter_handle, tab_idx] {
        if (store->active_nav.Get() == 4 && tab_idx == 0 && store->ConsumeFocusSearchNonce()) {
            ui_tasks.Post([filter_handle] {
                (void)filter_handle.RequestFocus(true);
            });
        }
    }, store->focus_search_nonce.Get(), tab_idx);

    View header = Row {
        Column {
            Text("LubanOS 隔离系统", TextRole::Title)
                .Style(TextStyle{
                    .font = Font::System(18.0F).WithWeight(FontWeight::Bold),
                    .foreground = theme.colors.on_surface,
                }),
            Text("多环境沙箱、发行版与桌面目录、声明式蓝图编排", TextRole::Label)
                .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
        }.With(Spacing(2.0F), Grow(1.0F)),
        Flow {
            luban_os_avail
                ? ActionButton(
                      "打开 LubanOS",
                      ButtonTone::Primary,
                      [store, toast] { store->LaunchLubanOsApp(toast); },
                      true,
                      icons::ExternalLinkIcon(),
                      true
                  )
                : ActionButton(
                      "安装 LubanOS",
                      ButtonTone::Secondary,
                      [store, tasks, toast] { store->InstallLubanComponentAsync(tasks, toast, "xim:luban-os"); },
                      true,
                      icons::DownloadIcon(),
                      true
                  ),
            RefreshButton(
                "刷新 Luban",
                [store, tasks] { store->RefreshLuban(tasks, true); },
                loading_luban
            ).Key("luban-refresh-btn"),
        }.With(Spacing(8.0F)),
    }.With(
        Padding(EdgeInsets::Symmetric(20.0F, 10.0F)),
        Spacing(12.0F),
        Background(theme.colors.surface),
        CrossAlign(CrossAxisAlignment::Center)
    );

    View tab_bar = Row {
        Tabs({"环境与机器", "发行版与桌面目录", "蓝图编排"}, tab_idx)
            .OnChanged([store](std::size_t idx) { store->luban_tab = idx; }),
        Spacer{}.With(Grow(1.0F)),
    }.With(
        Padding(EdgeInsets::Symmetric(20.0F, 6.0F)),
        Background(theme.colors.surface),
        CrossAlign(CrossAxisAlignment::Center)
    );

    View content = View{};
    if (!luban_avail && !loading_luban && tab_idx < 2) {
        content = LubanNotInstalledView(store, tasks, toast);
    } else if (tab_idx == 0) {
        content = LubanEnvironmentsTabView(store, tasks, toast, dialog, filter_handle);
    } else if (tab_idx == 1) {
        content = LubanCatalogTabView(store, tasks, toast, dialog);
    } else {
        content = LubanStacksStudioTabView(store, tasks, toast, dialog);
    }

    return Column {
        std::move(header),
        Divider(),
        std::move(tab_bar),
        Divider(),
        std::move(content),
    }.With(Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch));
}

} // namespace hxlings

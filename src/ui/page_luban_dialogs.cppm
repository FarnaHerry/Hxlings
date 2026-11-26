export module hxlings.page.luban.dialogs;

import std;
import huxerui;
import hxlings.theme;
import hxlings.icons;
import hxlings.store;
import hxlings.components;
import hxlings.luban.types;
import hxlings.luban.stack;

using namespace huxerui;

export namespace hxlings::luban_detail {


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

} // namespace hxlings::luban_detail


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
        const std::string name = luban_detail::TrimLubanText(name_input.Get().text);
        if (!luban_detail::IsValidLubanEnvName(name)) {
            err_msg = std::string("环境名称不能为空，且仅支持字母、数字、_ 或 -");
            return;
        }
        LubanCreateOptions opts{
            .name = name,
            .edition = edition_opt.Get(),
            .desktop = desktop_opt.Get(),
            .mode = mode_opt.Get(),
            .persist = persist_opt.Get(),
            .policy = luban_detail::TrimLubanText(policy_input.Get().text),
            .proxy = luban_detail::TrimLubanText(proxy_input.Get().text),
            .no_password = no_password_opt.Get(),
            .dry_run = dry_run_opt.Get(),
            .xpkgs = luban_detail::SplitCsvItems(xpkgs_input.Get().text),
            .allows = luban_detail::SplitCsvItems(allows_input.Get().text),
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
        const std::string archive = luban_detail::TrimLubanText(path_input.Get().text);
        if (archive.empty()) {
            err_msg = std::string("请输入要导入的归档文件或镜像路径");
            return;
        }
        const std::string cname = luban_detail::TrimLubanText(name_input.Get().text);
        if (!cname.empty() && !luban_detail::IsValidLubanEnvName(cname)) {
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
        const std::string out_path = luban_detail::TrimLubanText(path_input.Get().text);
        if (out_path.empty()) {
            err_msg = std::string("请输入导出文件路径");
            return;
        }
        LubanExportOptions opts{
            .name = env_name,
            .file_path = out_path,
            .format = format_opt.Get(),
            .size = luban_detail::TrimLubanText(size_input.Get().text),
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
                                  "podman import " + luban_detail::DisplayVal(path_input.Get().text, "./" + env_name + ".tar") +
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
            tasks, toast, env_name, luban_detail::TrimLubanText(to_input.Get().text), dry_run_opt.Get());
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
        store->RollbackLubanEnvAsync(tasks, toast, env_name, luban_detail::TrimLubanText(gen_input.Get().text));
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

} // namespace hxlings

export module hxlings.page.subos.tabs;

import std;
import huxerui;
import xlings.types;
import xlings.parser;
import hxlings.theme;
import hxlings.icons;
import hxlings.store;
import hxlings.components;

import hxlings.page.subos.dialogs;

using namespace huxerui;


export namespace hxlings {

[[huxerui::composable]]
View SubosPolicyTabView(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    DialogHandle dialog,
    std::string cur_name
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const auto& st = store->selected_subos_status.Get();
    const auto& req_pol = store->selected_subos_policy.Get();
    const auto& requests = store->selected_subos_requests.Get();
    const bool loading = store->loading_subos_governance.Get();

    const SubosPolicySnapshot& base_pol = !req_pol.source.empty() || !req_pol.net.empty() || !req_pol.extends.empty()
                                              ? req_pol
                                              : st.requested;

    auto sandbox_edit = UseState(base_pol.extends);
    auto net_edit = UseState(base_pol.net);
    auto proxy_edit = UseState(TextEditingValue::FromText(base_pol.proxy));
    auto fetch_edit = UseState(base_pol.fetch_default);
    auto idx_edit = UseState(base_pol.index_update);
    auto observe_edit = UseState(base_pol.observe_level);
    auto allow_edit = UseState(TextEditingValue::FromText(""));
    auto disallow_edit = UseState(TextEditingValue::FromText(""));
    auto env_pass_edit = UseState(TextEditingValue::FromText(""));
    auto mount_edit = UseState(TextEditingValue::FromText(""));
    auto unmount_edit = UseState(TextEditingValue::FromText(""));
    auto no_degrade_edit = UseState(base_pol.no_degrade);

    const std::size_t pol_hash = std::hash<std::string>{}(
        cur_name + "|" + base_pol.source + "|" + base_pol.extends + "|" + base_pol.net + "|" +
        base_pol.proxy + "|" + base_pol.fetch_default + "|" + base_pol.index_update + "|" +
        base_pol.observe_level + (base_pol.no_degrade ? "|1" : "|0")
    );

    Lifecycle([store, tasks, cur_name] {
        if (!store->skip_initial_refresh) {
            store->LoadSubosGovernance(tasks, cur_name, false);
        }
    }, std::hash<std::string>{}(cur_name));

    Lifecycle([base_pol, sandbox_edit, net_edit, proxy_edit, fetch_edit,
               idx_edit, observe_edit, no_degrade_edit]() mutable {
        sandbox_edit = base_pol.extends;
        net_edit = base_pol.net;
        proxy_edit = TextEditingValue::FromText(base_pol.proxy);
        fetch_edit = base_pol.fetch_default;
        idx_edit = base_pol.index_update;
        observe_edit = base_pol.observe_level;
        no_degrade_edit = base_pol.no_degrade;
    }, pol_hash);

    std::vector<View> gate_rows;
    for (std::size_t i = 0; i < st.gates.size(); ++i) {
        const auto& g = st.gates[i];
        gate_rows.push_back(
            Row {
                Column {
                    Row {
                        Text(g.gate, TextRole::Body)
                            .Style(TextStyle{.font = Font::Monospace(12.5F).WithWeight(FontWeight::SemiBold)}),
                        StatusBadge(
                            g.enforced ? "已实施" : "未实施",
                            g.enforced ? sp.success : sp.warning,
                            g.enforced ? sp.success_bg : sp.warning_bg
                        ),
                        StatusBadge(
                            g.supported ? "支持" : "不支持",
                            g.supported ? sp.info : sp.danger,
                            g.supported ? sp.info_bg : sp.danger_bg
                        ),
                    }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                    (g.route.empty() && g.reason.empty())
                        ? View{}
                        : Text(
                              (g.route.empty() ? "" : ("路径: " + g.route)) +
                                  (!g.route.empty() && !g.reason.empty() ? " · " : "") +
                                  g.reason,
                              TextRole::Label
                          ).Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                             .With(Grow(1.0F)),
                }.With(Spacing(4.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                Background(theme.colors.surface_container_low),
                CornerRadius(8.0F)
            ).Key("subos-gate-" + std::to_string(i))
        );
    }

    std::vector<View> req_rows;
    for (std::size_t i = 0; i < requests.size(); ++i) {
        const auto& r = requests[i];
        req_rows.push_back(
            Row {
                Column {
                    Row {
                        StatusBadge(r.kind.empty() ? std::string("req") : r.kind, sp.warning, sp.warning_bg),
                        Text("#" + r.id + " " + r.resource, TextRole::Body)
                            .Style(TextStyle{.font = Font::Monospace(12.5F).WithWeight(FontWeight::SemiBold)})
                            .With(Grow(1.0F)),
                    }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                    (r.detail.empty() && r.requested_at.empty())
                        ? View{}
                        : Text(
                              (r.requested_at.empty() ? std::string{} : FormatLocalTimestamp(r.requested_at)) +
                                  (!r.requested_at.empty() && !r.detail.empty() ? " · " : "") +
                                  r.detail,
                              TextRole::Label
                          ).Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                             .With(Grow(1.0F)),
                }.With(Spacing(4.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                Flow {
                    ActionButton(
                        "批准",
                        ButtonTone::Primary,
                        [store, tasks, toast, cur_name, id = r.id] {
                            store->ApproveSubosRequestAsync(tasks, toast, cur_name, id);
                        },
                        !r.id.empty(),
                        icons::CheckIcon(),
                        true
                    ),
                    ActionButton(
                        "拒绝",
                        ButtonTone::Danger,
                        [store, tasks, toast, cur_name, id = r.id] {
                            store->DenySubosRequestAsync(tasks, toast, cur_name, id);
                        },
                        !r.id.empty(),
                        icons::TrashIcon(),
                        true
                    ),
                }.With(Spacing(6.0F)),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                Spacing(8.0F),
                CrossAlign(CrossAxisAlignment::Center),
                Background(theme.colors.surface_container_low),
                CornerRadius(8.0F)
            ).Key("subos-req-" + std::to_string(i))
        );
    }

    const std::vector<std::pair<std::string, std::string>> sb_opts{
        {"dev", "dev"}, {"private", "private"}, {"locked", "locked"}
    };
    const std::vector<std::pair<std::string, std::string>> net_opts{
        {"host", "host"}, {"nat", "nat"}, {"none", "none"}, {"proxy", "proxy"}
    };
    const std::vector<std::pair<std::string, std::string>> action_opts{
        {"auto", "auto"}, {"ask", "ask"}, {"deny", "deny"}
    };
    const std::vector<std::pair<std::string, std::string>> obs_opts{
        {"off", "off"}, {"basic", "basic"}, {"standard", "standard"}, {"full", "full"}
    };

    return ScrollView(
        Column {
            Card(
                Column {
                    SectionHeader(
                        "隔离状态与内核门控 (subos status)",
                        "后端: " + (st.backend.empty() ? std::string("未加载") : st.backend) +
                            " · 策略来源: " + (st.policy_source.empty() ? std::string("默认") : st.policy_source),
                        RefreshButton(
                            "刷新策略",
                            [store, tasks, cur_name] { store->LoadSubosGovernance(tasks, cur_name, true); },
                            loading
                        ).Key("subos-refresh-policy-btn")
                    ),
                    Flow {
                        StatusBadge(
                            st.enters ? "允许进入 (enters: true)" : "进入受阻 / 未检查",
                            st.enters ? sp.success : sp.warning,
                            st.enters ? sp.success_bg : sp.warning_bg
                        ),
                        base_pol.extends.empty()
                            ? View{}
                            : StatusBadge("继承: " + base_pol.extends, sp.info, sp.info_bg),
                        base_pol.net.empty()
                            ? View{}
                            : StatusBadge("网络: " + base_pol.net, sp.info, sp.info_bg),
                        base_pol.observe_level.empty()
                            ? View{}
                            : StatusBadge("观测: " + base_pol.observe_level, sp.info, sp.info_bg),
                    }.With(Spacing(6.0F)),
                    gate_rows.empty()
                        ? Text("暂无门控详情（点击右上角刷新策略查询实时状态）", TextRole::Body)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        : View(Column(std::move(gate_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader(
                        "沙箱策略编辑器 (subos config)",
                        "修改当前 SubOS 的网络、权限、观测、环境变量透传与目录挂载策略"
                    ),
                    Text("沙箱预设 (--sandbox)", TextRole::Label),
                    SubosOptionChips(
                        sb_opts, sandbox_edit.Get(), [sandbox_edit](std::string v) mutable { sandbox_edit = std::move(v); }, "pol-sb"),
                    Text("网络模式 (--net)", TextRole::Label),
                    SubosOptionChips(
                        net_opts, net_edit.Get(), [net_edit](std::string v) mutable { net_edit = std::move(v); }, "pol-net"),
                    TextField(proxy_edit)
                        .Label("代理端点 (--proxy)")
                        .Placeholder("例如: socks5h://127.0.0.1:1080")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([proxy_edit](const TextEditingValue& v) mutable { proxy_edit = v; }),
                    Text("缺失包自动获取 (--fetch)", TextRole::Label),
                    SubosOptionChips(
                        action_opts, fetch_edit.Get(), [fetch_edit](std::string v) mutable { fetch_edit = std::move(v); }, "pol-fetch"),
                    Text("沙箱内索引更新 (--index-update)", TextRole::Label),
                    SubosOptionChips(
                        action_opts, idx_edit.Get(), [idx_edit](std::string v) mutable { idx_edit = std::move(v); }, "pol-idx"),
                    Text("审计观测级别 (--observe)", TextRole::Label),
                    SubosOptionChips(
                        obs_opts, observe_edit.Get(), [observe_edit](std::string v) mutable { observe_edit = std::move(v); }, "pol-obs"),
                    TextField(allow_edit)
                        .Label("授予能力 (--allow: display/audio/camera/gpu/ssh-agent)")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([allow_edit](const TextEditingValue& v) mutable { allow_edit = v; }),
                    TextField(disallow_edit)
                        .Label("撤销能力 (--disallow)")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([disallow_edit](const TextEditingValue& v) mutable { disallow_edit = v; }),
                    TextField(env_pass_edit)
                        .Label("放行宿主环境变量 (--env-pass，逗号分隔)")
                        .Placeholder("例如: HTTP_PROXY,HTTPS_PROXY,TERM")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([env_pass_edit](const TextEditingValue& v) mutable { env_pass_edit = v; }),
                    TextField(mount_edit)
                        .Label("持久挂载宿主路径 (--mount HOST[:INSIDE][:ro|rw])")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([mount_edit](const TextEditingValue& v) mutable { mount_edit = v; }),
                    TextField(unmount_edit)
                        .Label("移除挂载路径 (--unmount PATH)")
                        .Variant(TextFieldVariant::Outlined)
                        .OnChanged([unmount_edit](const TextEditingValue& v) mutable { unmount_edit = v; }),
                    Checkbox("缺失任意强约束门控时禁止降级进入 (--no-degrade)", no_degrade_edit.Get())
                        .OnChanged([no_degrade_edit](bool v) mutable { no_degrade_edit = v; }),
                    Flow {
                        ActionButton(
                            "应用策略更改",
                            ButtonTone::Primary,
                            [store, tasks, toast, cur_name, sandbox_edit, net_edit, proxy_edit, fetch_edit,
                             idx_edit, observe_edit, allow_edit, disallow_edit, env_pass_edit,
                             mount_edit, unmount_edit, no_degrade_edit] {
                                SubosPolicyPatch patch{
                                    .sandbox = sandbox_edit.Get(),
                                    .net = net_edit.Get(),
                                    .proxy = proxy_edit.Get().text,
                                    .fetch_default = fetch_edit.Get(),
                                    .index_update = idx_edit.Get(),
                                    .observe_level = observe_edit.Get(),
                                    .allow_cap = allow_edit.Get().text,
                                    .disallow_cap = disallow_edit.Get().text,
                                    .env_pass_csv = env_pass_edit.Get().text,
                                    .mount_spec = mount_edit.Get().text,
                                    .unmount_spec = unmount_edit.Get().text,
                                    .no_degrade = no_degrade_edit.Get(),
                                };
                                store->UpdateSubosConfigAsync(tasks, toast, cur_name, std::move(patch));
                            },
                            true,
                            icons::CheckIcon(),
                            true
                        ),
                        ActionButton(
                            "升级策略包 (--policy-upgrade)",
                            ButtonTone::Secondary,
                            [store, tasks, toast, cur_name] {
                                SubosPolicyPatch patch;
                                patch.policy_upgrade = true;
                                store->UpdateSubosConfigAsync(tasks, toast, cur_name, std::move(patch));
                            },
                            true,
                            icons::RefreshIcon(),
                            true
                        ),
                        ActionButton(
                            "重置策略 (--reset)",
                            ButtonTone::Danger,
                            [dialog, store, tasks, toast, cur_name] {
                                dialog.Show(
                                    "重置 SubOS 策略",
                                    "确定删除 `" + cur_name + "` 的自定义策略文件并恢复默认策略吗？",
                                    "确认重置",
                                    "取消",
                                    [store, tasks, toast, cur_name] {
                                        SubosPolicyPatch patch;
                                        patch.reset = true;
                                        store->UpdateSubosConfigAsync(tasks, toast, cur_name, std::move(patch));
                                    },
                                    {}
                                );
                            },
                            true,
                            icons::TrashIcon(),
                            true
                        ),
                    }.With(Spacing(8.0F)),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader(
                        "待审批沙箱请求 (" + std::to_string(requests.size()) + ")",
                        "来自分离沙箱内部的操作申请 (`subos requests` / `approve` / `deny`)"
                    ),
                    req_rows.empty()
                        ? Text("当前没有等待审批的沙箱请求", TextRole::Body)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        : View(Column(std::move(req_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Grow(1.0F), ScrollBar{});
}

[[huxerui::composable]]
View SubosSessionsTabView(
    std::shared_ptr<XlingsStore> store,
    TaskScope tasks,
    ToastHandle toast,
    std::string cur_name
) {
    const ThemeSpec& theme = UseTheme();
    const StatusPalette sp = ResolveStatusPalette(theme);
    const auto& sessions = store->selected_subos_sessions.Get();
    const auto& doctor = store->subos_doctor_snapshot.Get();
    const auto& report = store->selected_subos_report.Get();
    const std::string& info_text = store->selected_subos_info.Get();
    const bool loading = store->loading_subos_governance.Get();

    Lifecycle([store, tasks, cur_name] {
        if (!store->skip_initial_refresh) {
            store->LoadSubosGovernance(tasks, cur_name, false);
        }
    }, std::hash<std::string>{}(cur_name));

    std::vector<View> ps_rows;
    for (std::size_t i = 0; i < sessions.size(); ++i) {
        const auto& s = sessions[i];
        ps_rows.push_back(
            Row {
                Column {
                    Row {
                        Text(s.id.empty() ? std::string("session") : s.id, TextRole::Body)
                            .Style(TextStyle{.font = Font::Monospace(12.5F).WithWeight(FontWeight::SemiBold)}),
                        s.backend.empty() ? View{} : StatusBadge(s.backend, sp.info, sp.info_bg),
                        s.state.empty() ? View{} : StatusBadge(s.state, sp.success, sp.success_bg),
                    }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Text(
                        "PID: " + std::to_string(s.pid) +
                            (s.started.empty() ? "" : (" · 启动: " + FormatLocalTimestamp(s.started))) +
                            (s.command.empty() ? "" : (" · " + s.command)),
                        TextRole::Label
                    ).Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        .With(Grow(1.0F)),
                }.With(Spacing(4.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                Background(theme.colors.surface_container_low),
                CornerRadius(8.0F)
            ).Key("subos-ps-" + std::to_string(i))
        );
    }

    std::vector<View> finding_rows;
    for (std::size_t i = 0; i < doctor.findings.size(); ++i) {
        const auto& f = doctor.findings[i];
        const bool is_err = (f.level == "error" || f.level == "fatal");
        finding_rows.push_back(
            Row {
                Column {
                    Row {
                        StatusBadge(
                            f.level.empty() ? std::string("info") : f.level,
                            is_err ? sp.danger : sp.warning,
                            is_err ? sp.danger_bg : sp.warning_bg
                        ),
                        Text((f.instance.empty() ? "" : (f.instance + " · ")) + f.check, TextRole::Body)
                            .Style(TextStyle{.font = Font::System(13.0F).WithWeight(FontWeight::SemiBold)})
                            .With(Grow(1.0F)),
                    }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                    f.detail.empty()
                        ? View{}
                        : Text(f.detail, TextRole::Label)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                              .With(Grow(1.0F)),
                    f.fix.empty()
                        ? View{}
                        : Text("建议修复: " + f.fix, TextRole::Label)
                              .Style(TextStyle{.foreground = theme.colors.primary})
                              .With(Grow(1.0F)),
                }.With(Spacing(4.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                Background(theme.colors.surface_container_low),
                CornerRadius(8.0F)
            ).Key("subos-doc-" + std::to_string(i))
        );
    }

    std::vector<View> report_rows;
    for (std::size_t i = 0; i < report.sessions.size(); ++i) {
        const auto& rs = report.sessions[i];
        report_rows.push_back(
            Row {
                Column {
                    Row {
                        Text(rs.id.empty() ? std::string("session") : rs.id, TextRole::Body)
                            .Style(TextStyle{.font = Font::Monospace(12.5F).WithWeight(FontWeight::SemiBold)}),
                        StatusBadge("exit " + std::to_string(rs.exit_code),
                                    rs.exit_code == 0 ? sp.success : sp.danger,
                                    rs.exit_code == 0 ? sp.success_bg : sp.danger_bg),
                        StatusBadge(std::to_string(rs.duration_ms) + " ms", sp.info, sp.info_bg),
                    }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center)),
                    Text(
                        (rs.started.empty() ? std::string{} : ("开始: " + FormatLocalTimestamp(rs.started) + " · ")) +
                            "命令: " + std::to_string(rs.commands) +
                            " · 执行: " + std::to_string(rs.executions) +
                            " · 拒绝: " + std::to_string(rs.denied.size()) +
                            " · 变更文件: " + std::to_string(rs.changed.size()),
                        TextRole::Label
                    ).Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        .With(Grow(1.0F)),
                }.With(Spacing(4.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
            }.With(
                Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
                Background(theme.colors.surface_container_low),
                CornerRadius(8.0F)
            ).Key("subos-rep-" + std::to_string(i))
        );
    }

    return ScrollView(
        Column {
            Card(
                Column {
                    SectionHeader(
                        "运行中会话 (subos ps)",
                        "共 " + std::to_string(sessions.size()) + " 个活跃后台会话",
                        RefreshButton(
                            "刷新数据",
                            [store, tasks, cur_name] { store->LoadSubosGovernance(tasks, cur_name, true); },
                            loading
                        ).Key("subos-refresh-sessions-btn")
                    ),
                    ps_rows.empty()
                        ? Text("当前没有运行中的 SubOS 会话", TextRole::Body)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        : View(Column(std::move(ps_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader(
                        "SubOS 诊断 (subos doctor)",
                        "错误数: " + std::to_string(doctor.errors) + " · 检测项: " + std::to_string(doctor.findings.size()),
                        ActionButton(
                            "修复策略包 (--fix)",
                            ButtonTone::Primary,
                            [store, tasks, toast, cur_name] { store->RunSubosDoctorFixAsync(tasks, toast, cur_name); },
                            true,
                            icons::WrenchIcon(),
                            true
                        )
                    ),
                    finding_rows.empty()
                        ? Text("诊断未报告异常发现", TextRole::Body)
                              .Style(TextStyle{.foreground = sp.success})
                        : View(Column(std::move(finding_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader("SubOS 环境详情面板", "汇总命令数、已安装包数、GL 分发与异常告警 (来自 subos_info)"),
                    CodeBox(info_text, 90.0F, 180.0F),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
            Card(
                Column {
                    SectionHeader(
                        "会话与变更报告 (subos report)",
                        "策略变更 " + std::to_string(report.policy_changes.size()) +
                            " 次 · 历史会话 " + std::to_string(report.sessions.size()) + " 条"
                    ),
                    report_rows.empty()
                        ? Text("暂无历史会话报告记录", TextRole::Body)
                              .Style(TextStyle{.foreground = theme.colors.on_surface_variant})
                        : View(Column(std::move(report_rows)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))),
                }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch))
            ),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
    ).ScrollAxis(Axis::Vertical)
        .With(Grow(1.0F), ScrollBar{});
}

} // namespace hxlings
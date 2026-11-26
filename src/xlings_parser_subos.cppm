export module xlings.parser.subos;

import std;
import nlohmann.json;
import xlings.types;
import xlings.parser.common;

namespace hxlings::subos_parser_detail {

using namespace hxlings::parser_detail;

std::vector<SubosGateStatus> ParseGateArray(const json& doc, std::string_view key) {
    std::vector<SubosGateStatus> out;
    if (!doc.is_object()) return out;
    const auto it = doc.find(std::string(key));
    if (it == doc.end() || !it->is_array()) return out;
    for (const auto& g : *it) {
        if (!g.is_object()) continue;
        out.push_back(SubosGateStatus{
            .gate = JsonString(g, "gate"),
            .enforced = JsonBool(g, "enforced"),
            .reason = JsonString(g, "reason"),
            .route = JsonString(g, "route"),
            .supported = JsonBool(g, "supported"),
        });
    }
    return out;
}

SubosPolicySnapshot ParseSubosPolicyObj(const json& doc) {
    SubosPolicySnapshot policy;
    if (!doc.is_object()) return policy;
    policy.extends = JsonString(doc, "extends");
    policy.source = JsonString(doc, "source");

    if (doc.contains("isolation") && doc["isolation"].is_object()) {
        const auto& iso = doc["isolation"];
        policy.disable_user_ns = JsonBool(iso, "disable_userns");
        policy.identity = JsonString(iso, "identity");
        policy.net = JsonString(iso, "net");
        policy.proxy = JsonString(iso, "proxy");
        policy.no_degrade = JsonBool(iso, "no_degrade");
        policy.grants = JsonStringArray(iso, "grants");
        policy.grants_allowed = JsonStringArray(iso, "grants_allowed");
        policy.env_pass = JsonStringArray(iso, "env_pass");
    }

    if (doc.contains("mounts") && doc["mounts"].is_array()) {
        for (const auto& m : doc["mounts"]) {
            if (m.is_string()) {
                policy.mounts.push_back(m.get<std::string>());
            } else if (m.is_object()) {
                const std::string spec = JsonString(m, "spec");
                if (!spec.empty()) {
                    policy.mounts.push_back(spec);
                } else {
                    const std::string host = JsonString(m, "host");
                    const std::string inside = JsonString(m, "inside");
                    const std::string mode = JsonString(m, "mode");
                    std::string formatted = host;
                    if (!inside.empty() && inside != host) formatted += ":" + inside;
                    if (!mode.empty()) formatted += ":" + mode;
                    if (!formatted.empty()) policy.mounts.push_back(std::move(formatted));
                }
            }
        }
    }

    if (doc.contains("observe") && doc["observe"].is_object()) {
        policy.observe_level = JsonString(doc["observe"], "level");
    }

    if (doc.contains("permissions") && doc["permissions"].is_object()) {
        const auto& perms = doc["permissions"];
        if (perms.contains("fetch") && perms["fetch"].is_object()) {
            policy.fetch_default = JsonString(perms["fetch"], "default");
        }
        policy.index_update = JsonString(perms, "index_update");
    }
    return policy;
}

SubosSessionItem ParseSubosSessionObj(const json& obj, std::string_view raw_line) {
    SubosSessionItem item;
    item.id = JsonString(obj, "id");
    if (item.id.empty()) item.id = JsonString(obj, "session_id");
    item.instance = JsonString(obj, "instance");
    if (item.instance.empty()) item.instance = JsonString(obj, "subos");
    item.backend = JsonString(obj, "backend");
    item.started = JsonString(obj, "started");
    if (item.started.empty()) item.started = JsonString(obj, "started_at");
    item.state = JsonString(obj, "state");
    if (item.state.empty()) item.state = JsonString(obj, "status");
    item.pid = JsonInt(obj, "pid", 0);
    if (obj.contains("command") && obj["command"].is_array()) {
        for (const auto& c : obj["command"]) {
            if (!c.is_string()) continue;
            if (!item.command.empty()) item.command.push_back(' ');
            item.command += c.get<std::string>();
        }
    } else {
        item.command = JsonString(obj, "command");
    }
    item.raw_json = raw_line.empty() ? obj.dump() : std::string(raw_line);
    return item;
}

SubosRequestItem ParseSubosRequestObj(const json& obj, std::string_view raw_line) {
    SubosRequestItem item;
    item.id = JsonString(obj, "id");
    if (item.id.empty()) item.id = JsonString(obj, "request_id");
    item.instance = JsonString(obj, "instance");
    if (item.instance.empty()) item.instance = JsonString(obj, "subos");
    item.kind = JsonString(obj, "kind");
    if (item.kind.empty()) item.kind = JsonString(obj, "type");
    item.resource = JsonString(obj, "resource");
    if (item.resource.empty()) item.resource = JsonString(obj, "target");
    if (item.resource.empty()) item.resource = JsonString(obj, "package");
    if (item.resource.empty()) item.resource = JsonString(obj, "url");
    item.detail = JsonString(obj, "detail");
    if (item.detail.empty()) item.detail = JsonString(obj, "reason");
    item.state = JsonString(obj, "state");
    if (item.state.empty()) item.state = JsonString(obj, "status");
    item.requested_at = JsonString(obj, "requested_at");
    if (item.requested_at.empty()) item.requested_at = JsonString(obj, "created_at");
    item.raw_json = raw_line.empty() ? obj.dump() : std::string(raw_line);
    return item;
}

std::string FormatJsonArrayString(const json& doc, std::string_view key) {
    const auto arr = JsonStringArray(doc, key);
    if (!arr.empty()) {
        std::string joined;
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) joined += ", ";
            joined += arr[i];
        }
        return joined;
    }
    return JsonString(doc, key);
}

} // namespace hxlings::subos_parser_detail

export namespace hxlings {

std::vector<SubosEntry> ParseSubosListEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<SubosEntry> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "subos_list") continue;
        const auto obj = subos_parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        if (!pl.contains("entries") || !pl["entries"].is_array()) continue;
        for (const auto& e : pl["entries"]) {
            if (!e.is_object()) continue;
            out.push_back(SubosEntry{
                .name = subos_parser_detail::JsonString(e, "name"),
                .dir = subos_parser_detail::JsonString(e, "dir"),
                .abi = subos_parser_detail::JsonString(e, "abi"),
                .carrier = subos_parser_detail::JsonString(e, "carrier"),
                .kind = subos_parser_detail::JsonString(e, "kind"),
                .view = subos_parser_detail::JsonString(e, "view"),
                .active = subos_parser_detail::JsonBool(e, "active"),
                .host = subos_parser_detail::JsonBool(e, "host"),
                .packages = subos_parser_detail::JsonInt(e, "packages"),
                .commands = subos_parser_detail::JsonInt(e, "commands"),
            });
        }
    }
    return out;
}

std::vector<std::string> ParseSubosShimsEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<std::string> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "subos_shims") continue;
        const auto obj = subos_parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        out = subos_parser_detail::JsonStringArray((*obj)["payload"], "shims");
    }
    return out;
}

std::vector<SubosEnvVar> ParseSubosEnvEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<SubosEnvVar> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "subos_env") continue;
        const auto obj = subos_parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        if (!pl.contains("vars") || !pl["vars"].is_array()) continue;
        for (const auto& v : pl["vars"]) {
            if (!v.is_object()) continue;
            out.push_back(SubosEnvVar{
                .var = subos_parser_detail::JsonString(v, "var"),
                .op = subos_parser_detail::JsonString(v, "op"),
                .value = subos_parser_detail::JsonString(v, "value"),
            });
        }
    }
    return out;
}

std::vector<SubosEvent> ParseSubosEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<SubosEvent> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "subos_event") continue;
        const auto obj = subos_parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload")) continue;
        const auto& pl = (*obj)["payload"];
        SubosEvent se;
        if (pl.is_object()) {
            se.timestamp = subos_parser_detail::JsonString(pl, "timestamp");
            if (se.timestamp.empty()) se.timestamp = subos_parser_detail::JsonString(pl, "ts");
            if (se.timestamp.empty()) se.timestamp = subos_parser_detail::JsonString(pl, "time");
            se.kind = subos_parser_detail::JsonString(pl, "kind");
            se.session = subos_parser_detail::JsonString(pl, "session");
            se.summary = subos_parser_detail::JsonString(pl, "summary");
            if (se.summary.empty()) se.summary = subos_parser_detail::JsonString(pl, "command");
            if (se.summary.empty()) se.summary = subos_parser_detail::JsonString(pl, "decision");
            if (se.summary.empty()) se.summary = pl.dump();
            se.raw_json = pl.dump(2);
        } else {
            se.summary = pl.dump();
            se.raw_json = se.summary;
        }
        out.push_back(std::move(se));
    }
    return out;
}

std::vector<RepoEntry> ParseRepoListEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<RepoEntry> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "repo_list") continue;
        const auto obj = subos_parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        if (!pl.contains("repos") || !pl["repos"].is_array()) continue;
        for (const auto& r : pl["repos"]) {
            if (!r.is_object()) continue;
            out.push_back(RepoEntry{
                .name = subos_parser_detail::JsonString(r, "name"),
                .scope = subos_parser_detail::JsonString(r, "scope"),
                .url = subos_parser_detail::JsonString(r, "url"),
            });
        }
    }
    return out;
}

std::vector<IndexRepoVersions> ParseIndexVersionsEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<IndexRepoVersions> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "index_versions") continue;
        const auto obj = subos_parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_array()) continue;
        for (const auto& repo : (*obj)["payload"]) {
            if (!repo.is_object()) continue;
            IndexRepoVersions item;
            item.name = subos_parser_detail::JsonString(repo, "name");
            item.current = subos_parser_detail::JsonString(repo, "current");
            item.installed = subos_parser_detail::JsonString(repo, "installed");
            item.pinned = subos_parser_detail::JsonString(repo, "pinned");
            item.git_managed = subos_parser_detail::JsonBool(repo, "git_managed");
            if (repo.contains("snapshots") && repo["snapshots"].is_array()) {
                for (const auto& s : repo["snapshots"]) {
                    if (!s.is_object()) continue;
                    IndexSnapshot snap;
                    snap.index_version = subos_parser_detail::JsonString(s, "index_version");
                    snap.generated_at = subos_parser_detail::JsonString(s, "generated_at");
                    snap.current = subos_parser_detail::JsonBool(s, "current");
                    snap.installed = subos_parser_detail::JsonBool(s, "installed");
                    if (s.contains("artifact") && s["artifact"].is_object()) {
                        const auto& art = s["artifact"];
                        snap.artifact_name = subos_parser_detail::JsonString(art, "name");
                        snap.size_bytes = subos_parser_detail::JsonUint64(art, "size");
                    }
                    item.snapshots.push_back(std::move(snap));
                }
            }
            out.push_back(std::move(item));
        }
    }
    return out;
}

std::vector<SelfLogEntry> ParseSelfLogLines(std::string_view text) {
    std::vector<SelfLogEntry> out;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t nl = text.find('\n', start);
        const std::size_t end = (nl == std::string_view::npos) ? text.size() : nl;
        const auto obj = subos_parser_detail::TryParseJsonObject(text.substr(start, end - start));
        if (obj && obj->contains("command")) {
            out.push_back(SelfLogEntry{
                .command = subos_parser_detail::JsonString(*obj, "command"),
                .started = subos_parser_detail::JsonString(*obj, "started"),
                .trace = subos_parser_detail::JsonString(*obj, "trace"),
                .exit_code = subos_parser_detail::JsonInt(*obj, "exit"),
            });
        }
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return out;
}

ConfigSnapshot ParseConfigText(std::string_view text) {
    ConfigSnapshot cfg;
    cfg.raw_text = std::string(text);

    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t nl = text.find('\n', start);
        const std::size_t end = (nl == std::string_view::npos) ? text.size() : nl;
        const std::string line = subos_parser_detail::TrimCopy(text.substr(start, end - start));
        const std::size_t colon = line.find(':');
        if (colon != std::string::npos) {
            const std::string key = subos_parser_detail::TrimCopy(std::string_view(line).substr(0, colon));
            const std::string val = subos_parser_detail::TrimCopy(std::string_view(line).substr(colon + 1));
            if (key == "XLINGS_HOME") {
                cfg.xlings_home = val;
            } else if (key == "XLINGS_DATA") {
                cfg.xlings_data = val;
            } else if (key == "XLINGS_SUBOS") {
                cfg.xlings_subos = val;
            } else if (key == "当前子系统" || key == "active subos" || key == "subos") {
                cfg.active_subos = val;
            } else if (key == "可执行目录" || key == "bin dir" || key == "bin") {
                cfg.bin_dir = val;
            } else if (key == "镜像" || key == "mirror") {
                cfg.mirror = val;
            } else if (key == "语言" || key == "language" || key == "lang") {
                cfg.lang = val;
            } else if (key == "界面模式" || key == "ui mode" || key == "ui-mode") {
                cfg.ui_mode = val;
            } else if (key == "主题" || key == "theme") {
                cfg.theme = val;
            } else if (key == "交互模式" || key == "interactive") {
                cfg.interactive = val;
            } else if (key == "默认运行时" || key == "default runtime" || key == "default_runtime" || key == "runtime") {
                cfg.default_runtime = val;
            } else if (key == "归档目录" || key == "archive" || key == "archives" || key == "archive dirs") {
                if (val != "(none)" && val != "none" && val != "无") {
                    std::size_t pos = 0;
                    while (pos <= val.size()) {
                        const std::size_t sep = val.find_first_of(",;", pos);
                        const std::string item = subos_parser_detail::TrimCopy(
                            sep == std::string_view::npos ? std::string_view(val).substr(pos)
                                                          : std::string_view(val).substr(pos, sep - pos));
                        if (!item.empty() &&
                            std::ranges::find(cfg.archive_dirs, item) == cfg.archive_dirs.end()) {
                            cfg.archive_dirs.push_back(item);
                        }
                        if (sep == std::string_view::npos) break;
                        pos = sep + 1;
                    }
                }
            }
        }
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return cfg;
}

std::vector<LocalXpkg> ParseLocalXpkgTable(std::string_view text) {
    std::vector<LocalXpkg> out;
    std::size_t start = 0;
    bool header_seen = false;
    while (start < text.size()) {
        const std::size_t nl = text.find('\n', start);
        const std::size_t end = (nl == std::string_view::npos) ? text.size() : nl;
        const std::string line = subos_parser_detail::TrimCopy(text.substr(start, end - start));
        if (!line.empty() && !line.starts_with('[') && !line.starts_with("---")) {
            std::istringstream iss(line);
            std::string c1, c2, c3, c4;
            if (iss >> c1 >> c2 >> c3 >> c4) {
                if (!header_seen && c1 == "name" && c2 == "version") {
                    header_seen = true;
                } else {
                    out.push_back(LocalXpkg{
                        .name = c1,
                        .version = c2,
                        .status = c3,
                        .source = c4,
                    });
                }
            }
        }
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return out;
}

std::vector<ProfileGeneration> ParseProfileListText(std::string_view text) {
    std::vector<ProfileGeneration> out;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t nl = text.find('\n', start);
        const std::size_t end = (nl == std::string_view::npos) ? text.size() : nl;
        const std::string line = subos_parser_detail::TrimCopy(text.substr(start, end - start));
        if (!line.empty() &&
            line.find("no generations") == std::string::npos &&
            !line.starts_with('[') &&
            !line.starts_with("---")) {
            std::istringstream iss(line);
            std::string first;
            iss >> first;
            std::string rest;
            std::getline(iss, rest);
            rest = subos_parser_detail::TrimCopy(rest);
            if (first == "*" || first == "->") {
                std::string next_id;
                std::istringstream rest_iss(rest);
                rest_iss >> next_id;
                std::string tail;
                std::getline(rest_iss, tail);
                if (!next_id.empty()) {
                    out.push_back(ProfileGeneration{
                        .id = next_id,
                        .summary = subos_parser_detail::TrimCopy(tail) + " (active)",
                        .raw_line = line,
                    });
                }
            } else {
                out.push_back(ProfileGeneration{
                    .id = first,
                    .summary = rest,
                    .raw_line = line,
                });
            }
        }
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return out;
}

std::vector<ConfigThemeItem> ParseConfigThemeList(std::string_view text) {
    std::vector<ConfigThemeItem> out;
    std::string active_name;
    bool in_themes = false;

    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t nl = text.find('\n', start);
        const std::size_t end = (nl == std::string_view::npos) ? text.size() : nl;
        const std::string_view raw = text.substr(start, end - start);
        const std::string trimmed = subos_parser_detail::TrimCopy(raw);
        if (trimmed == "themes:" || trimmed == "主题:") {
            in_themes = true;
        } else if (trimmed.starts_with("active:") || trimmed.starts_with("当前:") || trimmed.starts_with("当前主题:")) {
            const std::size_t colon = trimmed.find(':');
            if (colon != std::string::npos) {
                active_name = subos_parser_detail::TrimCopy(std::string_view(trimmed).substr(colon + 1));
            }
            in_themes = false;
        } else if (trimmed.starts_with("to customise:") || trimmed.starts_with("to customize:")) {
            in_themes = false;
        } else if (in_themes && !trimmed.empty() && !trimmed.starts_with('[')) {
            std::istringstream iss(trimmed);
            std::string name;
            iss >> name;
            std::string src;
            std::getline(iss, src);
            src = subos_parser_detail::TrimCopy(src);
            if (!name.empty()) {
                out.push_back(ConfigThemeItem{
                    .name = std::move(name),
                    .source = std::move(src),
                    .active = false,
                });
            }
        }
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }

    if (!active_name.empty()) {
        for (auto& item : out) {
            item.active = (item.name == active_name);
        }
    }
    return out;
}

std::vector<NetworkEndpoint> ParseNetworkEndpointsEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<NetworkEndpoint> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "network_endpoints") continue;
        const auto obj = subos_parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        if (!pl.contains("endpoints") || !pl["endpoints"].is_array()) continue;
        for (const auto& ep : pl["endpoints"]) {
            if (!ep.is_object()) continue;
            const std::string url = subos_parser_detail::JsonString(ep, "url");
            if (url.empty()) continue;
            out.push_back(NetworkEndpoint{
                .region = subos_parser_detail::JsonString(ep, "region"),
                .resource = subos_parser_detail::JsonString(ep, "resource"),
                .url = url,
            });
        }
    }
    return out;
}

NetworkProbeResult ParseNetworkProbeEvents(
    const std::vector<NdjsonEvent>& events,
    std::string_view target_url = {}
) {
    NetworkProbeResult res;
    res.url = std::string(target_url);
    for (const auto& ev : events) {
        if (ev.kind == "error" && !ev.message.empty()) {
            res.error_message = ev.message;
        }
        if (ev.kind != "data" || ev.data_kind != "network_probe") continue;
        const auto obj = subos_parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        res.host = subos_parser_detail::JsonString(pl, "host");
        res.status = subos_parser_detail::JsonString(pl, "status");
        res.reachable = subos_parser_detail::JsonBool(pl, "reachable");
        res.http_status = subos_parser_detail::JsonInt(pl, "http_status", 0);
        if (pl.contains("elapsed_seconds") && pl["elapsed_seconds"].is_number()) {
            res.elapsed_seconds = pl["elapsed_seconds"].get<double>();
        }
        res.proxy = subos_parser_detail::JsonBool(pl, "proxy");
        res.ok = true;
    }
    if (!res.ok && res.status.empty() && !res.error_message.empty()) {
        res.status = "error";
    }
    return res;
}

SubosStatusSnapshot ParseSubosStatusJson(std::string_view raw) {
    SubosStatusSnapshot snap;
    snap.raw_json = subos_parser_detail::TrimCopy(raw);
    const auto obj = subos_parser_detail::TryParseJsonObject(raw);
    if (!obj) return snap;

    snap.instance = subos_parser_detail::JsonString(*obj, "instance");
    snap.policy_source = subos_parser_detail::JsonString(*obj, "policy_source");
    if (obj->contains("effective") && (*obj)["effective"].is_object()) {
        const auto& eff = (*obj)["effective"];
        snap.enters = subos_parser_detail::JsonBool(eff, "enters");
        if (eff.contains("spec") && eff["spec"].is_object()) {
            snap.backend = subos_parser_detail::JsonString(eff["spec"], "backend");
        }
        snap.gates = subos_parser_detail::ParseGateArray(eff, "gates");
    }
    if (obj->contains("requested") && (*obj)["requested"].is_object()) {
        snap.requested = subos_parser_detail::ParseSubosPolicyObj((*obj)["requested"]);
    }
    return snap;
}

SubosPolicySnapshot ParseSubosConfigJson(std::string_view raw) {
    const auto obj = subos_parser_detail::TryParseJsonObject(raw);
    if (!obj) return {};
    return subos_parser_detail::ParseSubosPolicyObj(*obj);
}

SubosDoctorSnapshot ParseSubosDoctorJson(std::string_view raw) {
    SubosDoctorSnapshot snap;
    snap.raw_json = subos_parser_detail::TrimCopy(raw);
    const auto obj = subos_parser_detail::TryParseJsonObject(raw);
    if (!obj) return snap;

    snap.errors = subos_parser_detail::JsonInt(*obj, "errors", 0);
    snap.gates = subos_parser_detail::ParseGateArray(*obj, "gates");
    if (obj->contains("instances") && (*obj)["instances"].is_array()) {
        for (const auto& inst : (*obj)["instances"]) {
            if (!inst.is_object()) continue;
            const std::string inst_name = subos_parser_detail::JsonString(inst, "instance");
            if (!inst.contains("findings") || !inst["findings"].is_array()) continue;
            for (const auto& f : inst["findings"]) {
                if (!f.is_object()) continue;
                snap.findings.push_back(SubosDoctorFinding{
                    .instance = inst_name,
                    .check = subos_parser_detail::JsonString(f, "check"),
                    .level = subos_parser_detail::JsonString(f, "level"),
                    .detail = subos_parser_detail::JsonString(f, "detail"),
                    .fix = subos_parser_detail::JsonString(f, "fix"),
                });
            }
        }
    }
    return snap;
}

std::vector<SubosSessionItem> ParseSubosPsJson(std::string_view raw) {
    std::vector<SubosSessionItem> out;
    std::size_t start = 0;
    while (start <= raw.size()) {
        const std::size_t nl = raw.find('\n', start);
        const std::size_t end = (nl == std::string_view::npos) ? raw.size() : nl;
        const std::string line = subos_parser_detail::TrimCopy(raw.substr(start, end - start));
        if (!line.empty() && line.front() == '{') {
            if (const auto obj = subos_parser_detail::TryParseJsonObject(line)) {
                out.push_back(subos_parser_detail::ParseSubosSessionObj(*obj, line));
            }
        } else if (!line.empty() && line.front() == '[') {
            try {
                auto arr = subos_parser_detail::json::parse(line);
                if (arr.is_array()) {
                    for (const auto& el : arr) {
                        if (el.is_object()) out.push_back(subos_parser_detail::ParseSubosSessionObj(el, {}));
                    }
                }
            } catch (...) {
            }
        }
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return out;
}

std::vector<SubosRequestItem> ParseSubosRequestsJson(std::string_view raw) {
    std::vector<SubosRequestItem> out;
    std::size_t start = 0;
    while (start <= raw.size()) {
        const std::size_t nl = raw.find('\n', start);
        const std::size_t end = (nl == std::string_view::npos) ? raw.size() : nl;
        const std::string line = subos_parser_detail::TrimCopy(raw.substr(start, end - start));
        if (!line.empty() && line.front() == '{') {
            if (const auto obj = subos_parser_detail::TryParseJsonObject(line)) {
                if (obj->contains("requests") && (*obj)["requests"].is_array()) {
                    for (const auto& el : (*obj)["requests"]) {
                        if (el.is_object()) out.push_back(subos_parser_detail::ParseSubosRequestObj(el, {}));
                    }
                } else {
                    out.push_back(subos_parser_detail::ParseSubosRequestObj(*obj, line));
                }
            }
        } else if (!line.empty() && line.front() == '[') {
            try {
                auto arr = subos_parser_detail::json::parse(line);
                if (arr.is_array()) {
                    for (const auto& el : arr) {
                        if (el.is_object()) out.push_back(subos_parser_detail::ParseSubosRequestObj(el, {}));
                    }
                }
            } catch (...) {
            }
        }
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return out;
}

SubosReportSnapshot ParseSubosReportJson(std::string_view raw) {
    SubosReportSnapshot snap;
    snap.raw_json = subos_parser_detail::TrimCopy(raw);
    const auto obj = subos_parser_detail::TryParseJsonObject(raw);
    if (!obj) return snap;

    snap.instance = subos_parser_detail::JsonString(*obj, "instance");
    if (obj->contains("policy_changes") && (*obj)["policy_changes"].is_array()) {
        for (const auto& c : (*obj)["policy_changes"]) {
            if (c.is_string()) {
                snap.policy_changes.push_back(c.get<std::string>());
            } else if (c.is_object()) {
                snap.policy_changes.push_back(c.dump());
            }
        }
    }
    if (obj->contains("sessions") && (*obj)["sessions"].is_array()) {
        for (const auto& s : (*obj)["sessions"]) {
            if (!s.is_object()) continue;
            SubosReportSession rs;
            rs.id = subos_parser_detail::JsonString(s, "id");
            rs.backend = subos_parser_detail::JsonString(s, "backend");
            rs.started = subos_parser_detail::JsonString(s, "started");
            rs.commands = subos_parser_detail::JsonInt(s, "commands", 0);
            rs.executions = subos_parser_detail::JsonInt(s, "executions", 0);
            rs.exit_code = subos_parser_detail::JsonInt(s, "exit", 0);
            rs.duration_ms = subos_parser_detail::JsonUint64(s, "ms", 0);
            rs.denied = subos_parser_detail::JsonStringArray(s, "denied");
            rs.changed = subos_parser_detail::JsonStringArray(s, "changed");
            rs.programs = subos_parser_detail::JsonStringArray(s, "programs");
            snap.sessions.push_back(std::move(rs));
        }
    }
    return snap;
}

std::string FormatNdjsonInfoPanel(const std::vector<NdjsonEvent>& events) {
    std::string out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "info_panel") continue;
        const auto obj = subos_parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        const std::string title = subos_parser_detail::JsonString(pl, "title");
        if (!title.empty()) {
            out += "=== " + title + " ===\n";
        }
        auto append_fields = [&](std::string_view key) {
            if (!pl.contains(std::string(key)) || !pl[std::string(key)].is_array()) return;
            for (const auto& f : pl[std::string(key)]) {
                if (!f.is_object()) continue;
                const std::string label = subos_parser_detail::JsonString(f, "label");
                const std::string value = subos_parser_detail::JsonString(f, "value");
                const bool alert = subos_parser_detail::JsonBool(f, "alert");
                if (label.empty() && value.empty()) continue;
                if (alert) out += "[!] ";
                out += label + ": " + value + "\n";
            }
        };
        append_fields("fields");
        append_fields("extra_fields");
    }
    return subos_parser_detail::TrimCopy(out);
}

std::string FormatSubosDiffEvents(const std::vector<NdjsonEvent>& events) {
    std::string out;
    bool saw_data = false;
    for (const auto& ev : events) {
        if (ev.kind == "error" && !ev.message.empty()) {
            out += "[error] " + ev.message + "\n";
            continue;
        }
        if (ev.kind != "data") continue;
        const auto obj = subos_parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload")) continue;
        saw_data = true;
        const auto& pl = (*obj)["payload"];
        if (pl.is_array()) {
            if (pl.empty()) {
                out += "两个 SubOS 的包清单完全一致（无差异）。\n";
            } else {
                for (const auto& item : pl) {
                    if (item.is_string()) {
                        out += item.get<std::string>() + "\n";
                    } else if (item.is_object()) {
                        const std::string name = subos_parser_detail::JsonString(item, "package");
                        const std::string alt_name = name.empty() ? subos_parser_detail::JsonString(item, "name") : name;
                        const std::string status = subos_parser_detail::JsonString(item, "status");
                        const std::string diff = subos_parser_detail::JsonString(item, "diff");
                        if (!alt_name.empty()) {
                            out += alt_name;
                            if (!status.empty()) out += " [" + status + "]";
                            const std::string left = subos_parser_detail::JsonString(item, "left");
                            const std::string right = subos_parser_detail::JsonString(item, "right");
                            if (!left.empty() || !right.empty()) {
                                out += ": " + (left.empty() ? "(无)" : left) + " -> " + (right.empty() ? "(无)" : right);
                            }
                            if (!diff.empty()) out += " (" + diff + ")";
                            out += "\n";
                        } else {
                            out += item.dump() + "\n";
                        }
                    }
                }
            }
        } else if (pl.is_string()) {
            out += pl.get<std::string>() + "\n";
        } else {
            out += pl.dump(2) + "\n";
        }
    }
    if (!saw_data && out.empty()) {
        for (const auto& ev : events) {
            if (!ev.message.empty()) out += ev.message + "\n";
        }
    }
    return subos_parser_detail::TrimCopy(out);
}

std::string FormatSubosRollbackListEvents(const std::vector<NdjsonEvent>& events) {
    std::string out;
    for (const auto& ev : events) {
        if (ev.kind == "error" && !ev.message.empty()) {
            out += ev.message + "\n";
            if (const auto obj = subos_parser_detail::TryParseJsonObject(ev.raw_line)) {
                const std::string hint = subos_parser_detail::JsonString(*obj, "hint");
                if (!hint.empty()) out += "提示: " + hint + "\n";
            }
        } else if (ev.kind == "data") {
            const auto obj = subos_parser_detail::TryParseJsonObject(ev.raw_line);
            if (obj && obj->contains("payload")) {
                out += (*obj)["payload"].is_string()
                           ? (*obj)["payload"].get<std::string>()
                           : (*obj)["payload"].dump(2);
                out.push_back('\n');
            }
        } else if (!ev.message.empty()) {
            out += ev.message + "\n";
        }
    }
    return subos_parser_detail::TrimCopy(out);
}

} // namespace hxlings

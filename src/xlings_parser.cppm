export module xlings.parser;

import std;
import nlohmann.json;
import xlings.types;

namespace hxlings::detail {

using json = nlohmann::json;

std::string TrimCopy(std::string_view sv) {
    std::size_t start = 0;
    while (start < sv.size() &&
           (sv[start] == ' ' || sv[start] == '\t' || sv[start] == '\r' || sv[start] == '\n')) {
        ++start;
    }
    std::size_t end = sv.size();
    while (end > start &&
           (sv[end - 1] == ' ' || sv[end - 1] == '\t' || sv[end - 1] == '\r' || sv[end - 1] == '\n')) {
        --end;
    }
    return std::string(sv.substr(start, end - start));
}

std::string JsonString(const json& obj, std::string_view key, std::string_view fallback = {}) {
    if (!obj.is_object()) return std::string(fallback);
    const auto it = obj.find(std::string(key));
    if (it == obj.end() || it->is_null()) return std::string(fallback);
    if (it->is_string()) return it->get<std::string>();
    if (it->is_boolean()) return it->get<bool>() ? "true" : "false";
    if (it->is_number_integer()) return std::to_string(it->get<std::int64_t>());
    if (it->is_number_unsigned()) return std::to_string(it->get<std::uint64_t>());
    if (it->is_number_float()) return std::to_string(it->get<double>());
    return it->dump();
}

bool JsonBool(const json& obj, std::string_view key, bool fallback = false) {
    if (!obj.is_object()) return fallback;
    const auto it = obj.find(std::string(key));
    if (it == obj.end() || !it->is_boolean()) return fallback;
    return it->get<bool>();
}

int JsonInt(const json& obj, std::string_view key, int fallback = 0) {
    if (!obj.is_object()) return fallback;
    const auto it = obj.find(std::string(key));
    if (it == obj.end() || !it->is_number_integer()) return fallback;
    return it->get<int>();
}

std::uint64_t JsonUint64(const json& obj, std::string_view key, std::uint64_t fallback = 0) {
    if (!obj.is_object()) return fallback;
    const auto it = obj.find(std::string(key));
    if (it == obj.end() || !it->is_number_unsigned()) return fallback;
    return it->get<std::uint64_t>();
}

std::vector<std::string> JsonStringArray(const json& obj, std::string_view key) {
    std::vector<std::string> out;
    if (!obj.is_object()) return out;
    const auto it = obj.find(std::string(key));
    if (it == obj.end() || !it->is_array()) return out;
    out.reserve(it->size());
    for (const auto& item : *it) {
        if (item.is_string()) out.push_back(item.get<std::string>());
    }
    return out;
}

std::optional<json> TryParseJsonObject(std::string_view raw) {
    const std::string trimmed = TrimCopy(raw);
    if (trimmed.empty() || trimmed.front() != '{') return std::nullopt;
    try {
        json parsed = json::parse(trimmed);
        if (parsed.is_object()) return parsed;
    } catch (...) {
    }
    return std::nullopt;
}

std::optional<float> ExtractProgressPercent(const json& doc) {
    auto try_num = [](const json& v) -> std::optional<float> {
        if (v.is_number()) {
            const float val = v.get<float>();
            if (std::isfinite(val)) {
                return (val > 1.0F) ? std::clamp(val / 100.0F, 0.0F, 1.0F) : std::clamp(val, 0.0F, 1.0F);
            }
        }
        return std::nullopt;
    };
    if (doc.contains("percent")) {
        if (auto p = try_num(doc["percent"])) return p;
    }
    if (doc.contains("progress")) {
        if (auto p = try_num(doc["progress"])) return p;
    }
    if (doc.contains("payload") && doc["payload"].is_object()) {
        const auto& pl = doc["payload"];
        if (pl.contains("percent")) {
            if (auto p = try_num(pl["percent"])) return p;
        }
        if (pl.contains("progress")) {
            if (auto p = try_num(pl["progress"])) return p;
        }
        if (pl.contains("current") && pl.contains("total") && pl["current"].is_number() && pl["total"].is_number()) {
            const double cur = pl["current"].get<double>();
            const double tot = pl["total"].get<double>();
            if (tot > 0.0) return std::clamp(static_cast<float>(cur / tot), 0.0F, 1.0F);
        }
    }
    if (doc.contains("current") && doc.contains("total") && doc["current"].is_number() && doc["total"].is_number()) {
        const double cur = doc["current"].get<double>();
        const double tot = doc["total"].get<double>();
        if (tot > 0.0) return std::clamp(static_cast<float>(cur / tot), 0.0F, 1.0F);
    }
    return std::nullopt;
}

} // namespace hxlings::detail

export namespace hxlings {

PackageSpecParts SplitPackageSpec(std::string_view spec) {
    PackageSpecParts parts;
    const std::string trimmed = detail::TrimCopy(spec);
    if (trimmed.empty()) return parts;

    std::string_view rest = trimmed;
    const std::size_t at_pos = rest.rfind('@');
    if (at_pos != std::string_view::npos) {
        parts.version = std::string(rest.substr(at_pos + 1));
        rest = rest.substr(0, at_pos);
    }
    const std::size_t colon_pos = rest.find(':');
    if (colon_pos != std::string_view::npos) {
        parts.ns = std::string(rest.substr(0, colon_pos));
        parts.name = std::string(rest.substr(colon_pos + 1));
    } else {
        parts.name = std::string(rest);
    }
    return parts;
}

std::string FormatBytes(std::uint64_t bytes) {
    if (bytes < 1024ULL) return std::to_string(bytes) + " B";
    const double kb = static_cast<double>(bytes) / 1024.0;
    if (kb < 1024.0) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << kb << " KB";
        return oss.str();
    }
    const double mb = kb / 1024.0;
    if (mb < 1024.0) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << mb << " MB";
        return oss.str();
    }
    const double gb = mb / 1024.0;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << gb << " GB";
    return oss.str();
}

NdjsonEvent ParseNdjsonLine(std::string_view line) {
    NdjsonEvent ev;
    ev.raw_line = detail::TrimCopy(line);
    if (ev.raw_line.empty()) {
        ev.kind = "raw";
        return ev;
    }

    const auto parsed = detail::TryParseJsonObject(ev.raw_line);
    if (!parsed) {
        ev.kind = "raw";
        ev.message = ev.raw_line;
        return ev;
    }

    const auto& doc = *parsed;
    ev.kind = detail::JsonString(doc, "kind", "raw");
    ev.data_kind = detail::JsonString(doc, "dataKind");
    ev.level = detail::JsonString(doc, "level");
    if (doc.contains("exitCode") && doc["exitCode"].is_number_integer()) {
        ev.exit_code = doc["exitCode"].get<int>();
    }
    ev.progress_percent = detail::ExtractProgressPercent(doc);

    if (ev.kind == "log") {
        ev.message = detail::JsonString(doc, "message");
        if (ev.message.empty()) ev.message = detail::JsonString(doc, "text");
        if (ev.message.empty()) ev.message = detail::JsonString(doc, "line");
        if (ev.message.empty() && doc.contains("payload") && doc["payload"].is_object()) {
            ev.message = detail::JsonString(doc["payload"], "message");
        }
    } else if (ev.kind == "error") {
        ev.message = detail::JsonString(doc, "message");
        if (ev.message.empty()) ev.message = detail::JsonString(doc, "error");
        if (ev.message.empty() && doc.contains("payload") && doc["payload"].is_object()) {
            ev.message = detail::JsonString(doc["payload"], "message");
        }
        if (ev.message.empty()) ev.message = ev.raw_line;
    } else if (ev.kind == "progress") {
        ev.message = detail::JsonString(doc, "message");
        if (ev.message.empty()) ev.message = detail::JsonString(doc, "phase");
        if (ev.message.empty() && doc.contains("payload") && doc["payload"].is_object()) {
            const auto& pl = doc["payload"];
            ev.message = detail::JsonString(pl, "message");
            if (ev.message.empty()) ev.message = detail::JsonString(pl, "phase");
            if (ev.message.empty()) ev.message = detail::JsonString(pl, "step");
        }
        if (ev.message.empty() && ev.progress_percent) {
            ev.message = std::to_string(static_cast<int>(*ev.progress_percent * 100.0F)) + "%";
        }
    } else if (ev.kind == "data" && ev.data_kind == "subos_exec_output") {
        if (doc.contains("payload") && doc["payload"].is_object()) {
            const auto& pl = doc["payload"];
            ev.message = detail::JsonString(pl, "line");
            if (ev.message.empty()) ev.message = detail::JsonString(pl, "data");
            if (ev.message.empty()) ev.message = detail::JsonString(pl, "text");
            ev.level = detail::JsonString(pl, "stream");
        }
    } else if (ev.kind == "data" && ev.data_kind == "tip") {
        if (doc.contains("payload") && doc["payload"].is_object()) {
            ev.message = detail::JsonString(doc["payload"], "message");
        }
    } else if (ev.kind == "result") {
        if (doc.contains("data") && doc["data"].is_object()) {
            ev.message = detail::JsonString(doc["data"], "message");
        }
    }
    return ev;
}

std::vector<NdjsonEvent> ParseNdjsonLines(std::string_view output) {
    std::vector<NdjsonEvent> events;
    std::size_t start = 0;
    while (start < output.size()) {
        const std::size_t nl = output.find('\n', start);
        const std::size_t end = (nl == std::string_view::npos) ? output.size() : nl;
        const std::string trimmed = detail::TrimCopy(output.substr(start, end - start));
        if (!trimmed.empty()) {
            events.push_back(ParseNdjsonLine(trimmed));
        }
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return events;
}

XlingsVersionInfo ParseVersionJson(std::string_view text) {
    XlingsVersionInfo info;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t nl = text.find('\n', start);
        const std::size_t end = (nl == std::string_view::npos) ? text.size() : nl;
        const auto obj = detail::TryParseJsonObject(text.substr(start, end - start));
        if (obj && obj->contains("version")) {
            info.version = detail::JsonString(*obj, "version");
            info.protocol_version = detail::JsonString(*obj, "protocol_version");
            info.capabilities = detail::JsonStringArray(*obj, "capabilities");
            info.features = detail::JsonStringArray(*obj, "features");
            break;
        }
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return info;
}

XlingsEnv ParseEnvEvents(const std::vector<NdjsonEvent>& events) {
    XlingsEnv env;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "env") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        env.xlings_home = detail::JsonString(pl, "xlingsHome");
        env.data_dir = detail::JsonString(pl, "dataDir");
        env.subos_dir = detail::JsonString(pl, "subosDir");
        env.bin_dir = detail::JsonString(pl, "binDir");
        env.lib_dir = detail::JsonString(pl, "libDir");
        env.active_subos = detail::JsonString(pl, "activeSubos");
        env.mirror = detail::JsonString(pl, "mirror");
        env.lang = detail::JsonString(pl, "lang");
    }
    return env;
}

SystemStatus ParseSystemStatusEvents(const std::vector<NdjsonEvent>& events) {
    SystemStatus status;
    for (const auto& ev : events) {
        if (ev.kind != "data") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];

        if (ev.data_kind == "system_info") {
            status.title = detail::JsonString(pl, "title");
            if (pl.contains("fields") && pl["fields"].is_array()) {
                for (const auto& f : pl["fields"]) {
                    if (!f.is_object()) continue;
                    status.fields.push_back(KeyValueField{
                        .label = detail::JsonString(f, "label"),
                        .value = detail::JsonString(f, "value"),
                        .highlight = detail::JsonBool(f, "highlight"),
                    });
                }
            }
        } else if (ev.data_kind == "binding_state") {
            if (pl.contains("findings") && pl["findings"].is_array()) {
                for (const auto& f : pl["findings"]) {
                    if (!f.is_object()) continue;
                    status.findings.push_back(BindingFinding{
                        .code = detail::JsonString(f, "code"),
                        .severity = detail::JsonString(f, "severity"),
                        .subos = detail::JsonString(f, "subos"),
                        .target = detail::JsonString(f, "target"),
                        .version = detail::JsonString(f, "version"),
                        .summary = detail::JsonString(f, "summary"),
                        .hint = detail::JsonString(f, "hint"),
                    });
                }
            }
        }
    }
    return status;
}

std::vector<InstalledPkg> ParseInstalledPackagesEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<InstalledPkg> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "styled_list") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        if (!pl.contains("items") || !pl["items"].is_array()) continue;

        for (const auto& row : pl["items"]) {
            if (!row.is_array() || row.empty()) continue;
            const std::string spec = row[0].is_string() ? row[0].get<std::string>() : std::string{};
            const std::string desc = (row.size() > 1 && row[1].is_string()) ? row[1].get<std::string>() : std::string{};
            const std::string st = (row.size() > 2 && row[2].is_string()) ? row[2].get<std::string>() : std::string{};
            const PackageSpecParts parts = SplitPackageSpec(spec);

            InstalledPkg pkg;
            pkg.raw_spec = spec;
            pkg.ns = parts.ns;
            pkg.name = parts.name;
            pkg.version = parts.version;
            pkg.description = desc;
            pkg.status = st;
            pkg.is_degraded = st.find("degraded") != std::string::npos || st.find("broken") != std::string::npos;
            pkg.is_active = st.empty();
            out.push_back(std::move(pkg));
        }
    }
    return out;
}

std::vector<PkgGroup> GroupInstalledPackages(const std::vector<InstalledPkg>& items) {
    std::map<std::string, PkgGroup> by_key;
    for (const auto& item : items) {
        const std::string key = item.ns.empty() ? item.name : (item.ns + ":" + item.name);
        auto& grp = by_key[key];
        if (grp.key.empty()) {
            grp.key = key;
            grp.ns = item.ns;
            grp.name = item.name;
        }
        if (grp.description.empty() && !item.description.empty()) {
            grp.description = item.description;
        }
        if (item.is_active) {
            grp.active_version = item.version;
            if (!item.description.empty()) grp.description = item.description;
        }
        if (item.is_degraded) {
            grp.has_degraded = true;
        }
        grp.versions.push_back(item);
    }

    std::vector<PkgGroup> groups;
    groups.reserve(by_key.size());
    for (auto& [k, grp] : by_key) {
        std::ranges::stable_sort(grp.versions, [](const InstalledPkg& a, const InstalledPkg& b) {
            if (a.is_active != b.is_active) return a.is_active > b.is_active;
            if (a.is_degraded != b.is_degraded) return a.is_degraded > b.is_degraded;
            return a.version > b.version;
        });
        groups.push_back(std::move(grp));
    }
    std::ranges::sort(groups, [](const PkgGroup& a, const PkgGroup& b) {
        if (a.name != b.name) return a.name < b.name;
        return a.ns < b.ns;
    });
    return groups;
}

std::vector<InstalledVersionItem> ParseInstalledVersionsEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<InstalledVersionItem> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "info_panel") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        if (!pl.contains("fields") || !pl["fields"].is_array()) continue;
        for (const auto& f : pl["fields"]) {
            if (!f.is_object()) continue;
            out.push_back(InstalledVersionItem{
                .version = detail::JsonString(f, "label"),
                .path = detail::JsonString(f, "value"),
                .active = detail::JsonBool(f, "highlight"),
            });
        }
    }
    return out;
}

std::vector<SearchPkg> ParseSearchPackagesEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<SearchPkg> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "packages") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_array()) continue;
        for (const auto& item : (*obj)["payload"]) {
            if (!item.is_object()) continue;
            SearchPkg pkg;
            pkg.package = detail::JsonString(item, "package");
            pkg.ns = detail::JsonString(item, "namespace");
            pkg.name = detail::JsonString(item, "name");
            pkg.version = detail::JsonString(item, "version");
            pkg.description = detail::JsonString(item, "description");
            pkg.status = detail::JsonString(item, "status");
            pkg.index = detail::JsonString(item, "index");
            pkg.categories = detail::JsonStringArray(item, "categories");
            pkg.keywords = detail::JsonStringArray(item, "keywords");
            pkg.archs = detail::JsonStringArray(item, "archs");
            out.push_back(std::move(pkg));
        }
    }
    return out;
}

PackageDetail ParsePackageInfoEvents(const std::vector<NdjsonEvent>& events) {
    PackageDetail detail;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "info_panel") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        detail.title = detail::JsonString(pl, "title");

        if (pl.contains("package") && pl["package"].is_object()) {
            const auto& p = pl["package"];
            detail.package = detail::JsonString(p, "package");
            detail.ns = detail::JsonString(p, "namespace");
            detail.name = detail::JsonString(p, "name");
            detail.description = detail::JsonString(p, "description");
            detail.status = detail::JsonString(p, "status");
            detail.selected_version = detail::JsonString(p, "version");
            detail.available_versions = detail::JsonStringArray(p, "versions");
            detail.categories = detail::JsonStringArray(p, "categories");
            detail.archs = detail::JsonStringArray(p, "archs");
        }
        if (pl.contains("fields") && pl["fields"].is_array()) {
            for (const auto& f : pl["fields"]) {
                if (!f.is_object()) continue;
                detail.fields.push_back(KeyValueField{
                    .label = detail::JsonString(f, "label"),
                    .value = detail::JsonString(f, "value"),
                    .highlight = detail::JsonBool(f, "highlight"),
                });
            }
        }
    }
    return detail;
}

std::vector<InstallPlanItem> ParseInstallPlanEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<InstallPlanItem> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "install_plan") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        if (!pl.contains("packages") || !pl["packages"].is_array()) continue;
        for (const auto& row : pl["packages"]) {
            if (!row.is_array() || row.empty()) continue;
            InstallPlanItem item;
            if (row[0].is_string()) item.spec = row[0].get<std::string>();
            if (row.size() > 1 && row[1].is_string()) item.note = row[1].get<std::string>();
            if (row.size() > 2 && row[2].is_number_integer()) item.action_code = row[2].get<int>();
            out.push_back(std::move(item));
        }
    }
    return out;
}

std::vector<SubosEntry> ParseSubosListEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<SubosEntry> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "subos_list") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        if (!pl.contains("entries") || !pl["entries"].is_array()) continue;
        for (const auto& e : pl["entries"]) {
            if (!e.is_object()) continue;
            out.push_back(SubosEntry{
                .name = detail::JsonString(e, "name"),
                .dir = detail::JsonString(e, "dir"),
                .abi = detail::JsonString(e, "abi"),
                .carrier = detail::JsonString(e, "carrier"),
                .kind = detail::JsonString(e, "kind"),
                .view = detail::JsonString(e, "view"),
                .active = detail::JsonBool(e, "active"),
                .host = detail::JsonBool(e, "host"),
                .packages = detail::JsonInt(e, "packages"),
                .commands = detail::JsonInt(e, "commands"),
            });
        }
    }
    return out;
}

std::vector<std::string> ParseSubosShimsEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<std::string> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "subos_shims") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        out = detail::JsonStringArray((*obj)["payload"], "shims");
    }
    return out;
}

std::vector<SubosEnvVar> ParseSubosEnvEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<SubosEnvVar> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "subos_env") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        if (!pl.contains("vars") || !pl["vars"].is_array()) continue;
        for (const auto& v : pl["vars"]) {
            if (!v.is_object()) continue;
            out.push_back(SubosEnvVar{
                .var = detail::JsonString(v, "var"),
                .op = detail::JsonString(v, "op"),
                .value = detail::JsonString(v, "value"),
            });
        }
    }
    return out;
}

std::vector<SubosEvent> ParseSubosEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<SubosEvent> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "subos_event") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload")) continue;
        const auto& pl = (*obj)["payload"];
        SubosEvent se;
        if (pl.is_object()) {
            se.timestamp = detail::JsonString(pl, "timestamp");
            if (se.timestamp.empty()) se.timestamp = detail::JsonString(pl, "ts");
            if (se.timestamp.empty()) se.timestamp = detail::JsonString(pl, "time");
            se.kind = detail::JsonString(pl, "kind");
            se.session = detail::JsonString(pl, "session");
            se.summary = detail::JsonString(pl, "summary");
            if (se.summary.empty()) se.summary = detail::JsonString(pl, "command");
            if (se.summary.empty()) se.summary = detail::JsonString(pl, "decision");
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
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        if (!pl.contains("repos") || !pl["repos"].is_array()) continue;
        for (const auto& r : pl["repos"]) {
            if (!r.is_object()) continue;
            out.push_back(RepoEntry{
                .name = detail::JsonString(r, "name"),
                .scope = detail::JsonString(r, "scope"),
                .url = detail::JsonString(r, "url"),
            });
        }
    }
    return out;
}

std::vector<IndexRepoVersions> ParseIndexVersionsEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<IndexRepoVersions> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "index_versions") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_array()) continue;
        for (const auto& repo : (*obj)["payload"]) {
            if (!repo.is_object()) continue;
            IndexRepoVersions item;
            item.name = detail::JsonString(repo, "name");
            item.current = detail::JsonString(repo, "current");
            item.installed = detail::JsonString(repo, "installed");
            item.pinned = detail::JsonString(repo, "pinned");
            item.git_managed = detail::JsonBool(repo, "git_managed");
            if (repo.contains("snapshots") && repo["snapshots"].is_array()) {
                for (const auto& s : repo["snapshots"]) {
                    if (!s.is_object()) continue;
                    IndexSnapshot snap;
                    snap.index_version = detail::JsonString(s, "index_version");
                    snap.generated_at = detail::JsonString(s, "generated_at");
                    snap.current = detail::JsonBool(s, "current");
                    snap.installed = detail::JsonBool(s, "installed");
                    if (s.contains("artifact") && s["artifact"].is_object()) {
                        const auto& art = s["artifact"];
                        snap.artifact_name = detail::JsonString(art, "name");
                        snap.size_bytes = detail::JsonUint64(art, "size");
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
        const auto obj = detail::TryParseJsonObject(text.substr(start, end - start));
        if (obj && obj->contains("command")) {
            out.push_back(SelfLogEntry{
                .command = detail::JsonString(*obj, "command"),
                .started = detail::JsonString(*obj, "started"),
                .trace = detail::JsonString(*obj, "trace"),
                .exit_code = detail::JsonInt(*obj, "exit"),
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
        const std::string line = detail::TrimCopy(text.substr(start, end - start));
        const std::size_t colon = line.find(':');
        if (colon != std::string::npos) {
            const std::string key = detail::TrimCopy(std::string_view(line).substr(0, colon));
            const std::string val = detail::TrimCopy(std::string_view(line).substr(colon + 1));
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
        const std::string line = detail::TrimCopy(text.substr(start, end - start));
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
        const std::string line = detail::TrimCopy(text.substr(start, end - start));
        if (!line.empty() &&
            line.find("no generations") == std::string::npos &&
            !line.starts_with('[') &&
            !line.starts_with("---")) {
            std::istringstream iss(line);
            std::string first;
            iss >> first;
            std::string rest;
            std::getline(iss, rest);
            rest = detail::TrimCopy(rest);
            if (first == "*" || first == "->") {
                std::string next_id;
                std::istringstream rest_iss(rest);
                rest_iss >> next_id;
                std::string tail;
                std::getline(rest_iss, tail);
                if (!next_id.empty()) {
                    out.push_back(ProfileGeneration{
                        .id = next_id,
                        .summary = detail::TrimCopy(tail) + " (active)",
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

} // namespace hxlings

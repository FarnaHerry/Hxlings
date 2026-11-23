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

std::string NormalizeDescription(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    bool in_ws = true;
    for (const char ch : sv) {
        const bool is_ws = ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
        if (is_ws) {
            if (!in_ws) {
                out.push_back(' ');
                in_ws = true;
            }
        } else {
            out.push_back(ch);
            in_ws = false;
        }
    }
    if (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
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

bool IsVersionSeparator(char ch) {
    return ch == '.' || ch == '-' || ch == '_' || ch == '+';
}

std::string_view NextVersionSegment(std::string_view sv, std::size_t& pos) {
    while (pos < sv.size() && IsVersionSeparator(sv[pos])) ++pos;
    const std::size_t start = pos;
    while (pos < sv.size() && !IsVersionSeparator(sv[pos])) ++pos;
    return sv.substr(start, pos - start);
}

bool IsAllAsciiDigits(std::string_view sv) {
    if (sv.empty()) return false;
    for (const unsigned char ch : sv) {
        if (ch < '0' || ch > '9') return false;
    }
    return true;
}

std::string_view StripLeadingZeros(std::string_view sv) {
    std::size_t idx = 0;
    while (idx < sv.size() && sv[idx] == '0') ++idx;
    return sv.substr(idx);
}

} // namespace hxlings::detail

export namespace hxlings {

std::string TrimWhitespace(std::string_view sv) {
    return detail::TrimCopy(sv);
}

int CompareVersionStrings(std::string_view a, std::string_view b) {
    std::size_t pa = 0;
    std::size_t pb = 0;
    while (pa < a.size() || pb < b.size()) {
        const std::string_view sa = detail::NextVersionSegment(a, pa);
        const std::string_view sb = detail::NextVersionSegment(b, pb);
        if (sa.empty() && sb.empty()) break;
        if (sa.empty()) return -1;
        if (sb.empty()) return 1;

        if (detail::IsAllAsciiDigits(sa) && detail::IsAllAsciiDigits(sb)) {
            const std::string_view ta = detail::StripLeadingZeros(sa);
            const std::string_view tb = detail::StripLeadingZeros(sb);
            if (ta.size() != tb.size()) return (ta.size() < tb.size()) ? -1 : 1;
            if (ta != tb) return (ta < tb) ? -1 : 1;
        } else if (sa != sb) {
            return (sa < sb) ? -1 : 1;
        }
    }
    return 0;
}

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
            if (grp.active_version.empty() || CompareVersionStrings(item.version, grp.active_version) > 0) {
                grp.active_version = item.version;
                if (!item.description.empty()) grp.description = item.description;
            } else if (grp.description.empty() && !item.description.empty()) {
                grp.description = item.description;
            }
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
            const int cmp = CompareVersionStrings(a.version, b.version);
            if (cmp != 0) return cmp > 0;
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
            pkg.description = detail::NormalizeDescription(detail::JsonString(item, "description"));
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
            detail.description = detail::NormalizeDescription(detail::JsonString(p, "description"));
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

std::optional<RemovePlan> ParseRemovePlanEvents(const std::vector<NdjsonEvent>& events) {
    std::optional<RemovePlan> latest;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "remove_plan") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];

        RemovePlan plan;
        plan.name = detail::JsonString(pl, "name");
        plan.older_than = detail::JsonString(pl, "older_than");
        plan.dry_run = detail::JsonBool(pl, "dry_run", true);
        plan.total_bytes = detail::JsonUint64(pl, "bytes");

        if (pl.contains("remove") && pl["remove"].is_array()) {
            for (const auto& r : pl["remove"]) {
                if (!r.is_object()) continue;
                plan.remove.push_back(RemovePlanItem{
                    .version = detail::JsonString(r, "version"),
                    .bytes = detail::JsonUint64(r, "bytes"),
                });
            }
        }
        if (pl.contains("keep") && pl["keep"].is_array()) {
            for (const auto& k : pl["keep"]) {
                if (!k.is_object()) continue;
                plan.keep.push_back(RemovePlanKept{
                    .version = detail::JsonString(k, "version"),
                    .reason = detail::JsonString(k, "reason"),
                });
            }
        }
        latest = std::move(plan);
    }
    return latest;
}

std::vector<PayloadStatusItem> ParsePayloadStatusEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<PayloadStatusItem> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "payload_status") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        if (!pl.contains("targets") || !pl["targets"].is_array()) continue;

        out.clear();
        for (const auto& t : pl["targets"]) {
            if (!t.is_object()) continue;
            PayloadStatusItem item;
            item.request = detail::JsonString(t, "request");
            item.ns = detail::JsonString(t, "namespace");
            item.name = detail::JsonString(t, "name");
            item.version = detail::JsonString(t, "version");
            item.origin = detail::JsonString(t, "origin");
            item.payload_dir = detail::JsonString(t, "payload_dir");
            item.error = detail::JsonString(t, "error");
            item.revision = detail::JsonInt(t, "revision");
            if (t.contains("installed_revision") && t["installed_revision"].is_number_integer()) {
                item.installed_revision = t["installed_revision"].get<int>();
            }
            item.complete = detail::JsonBool(t, "complete");
            item.current = detail::JsonBool(t, "current");
            item.usable = detail::JsonBool(t, "usable");
            out.push_back(std::move(item));
        }
    }
    return out;
}

std::vector<UpdatablePackagePlan> BuildUpdatablePlans(
    const std::vector<PkgGroup>& groups,
    const std::vector<PayloadStatusItem>& statuses
) {
    auto find_status = [&](const PkgGroup& g) -> const PayloadStatusItem* {
        for (const auto& ps : statuses) {
            if (!ps.request.empty() && ps.request == g.key) return &ps;
        }
        for (const auto& ps : statuses) {
            const std::string fq = ps.ns.empty() ? ps.name : (ps.ns + ":" + ps.name);
            if ((!fq.empty() && fq == g.key) || (!ps.name.empty() && ps.name == g.key)) {
                return &ps;
            }
        }
        return nullptr;
    };

    std::vector<UpdatablePackagePlan> plans;
    for (const auto& g : groups) {
        const PayloadStatusItem* ps = find_status(g);
        if (ps == nullptr) continue;

        const std::string highest_installed = g.versions.empty() ? std::string{} : g.versions.front().version;
        const std::string current_ver = !g.active_version.empty() ? g.active_version : highest_installed;

        if (!ps->error.empty()) {
            const std::size_t nl = ps->error.find('\n');
            const std::string first_line = detail::TrimCopy(
                nl == std::string::npos ? std::string_view(ps->error)
                                        : std::string_view(ps->error).substr(0, nl)
            );
            plans.push_back(UpdatablePackagePlan{
                .key = g.key,
                .ns = g.ns,
                .name = g.name,
                .current_version = current_ver,
                .active_version = g.active_version,
                .is_skipped = true,
                .skip_reason = first_line,
            });
            continue;
        }

        if (ps->current || ps->version.empty()) continue;

        const bool latest_installed = std::ranges::any_of(
            g.versions, [&](const InstalledPkg& iv) { return iv.version == ps->version; });
        const bool rev_only = latest_installed && ps->origin == "local" && ps->complete;

        plans.push_back(UpdatablePackagePlan{
            .key = g.key,
            .ns = g.ns,
            .name = g.name,
            .current_version = rev_only ? ps->version : current_ver,
            .active_version = g.active_version,
            .latest_version = ps->version,
            .installed_revision = ps->installed_revision.value_or(0),
            .latest_revision = ps->revision,
            .is_revision_only = rev_only,
            .is_skipped = false,
        });
    }

    std::ranges::sort(plans, [](const UpdatablePackagePlan& a, const UpdatablePackagePlan& b) {
        if (a.is_skipped != b.is_skipped) return !a.is_skipped;
        if (a.is_revision_only != b.is_revision_only) return !a.is_revision_only;
        return a.key < b.key;
    });
    return plans;
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
            } else if (key == "默认运行时" || key == "default runtime" || key == "default_runtime" || key == "runtime") {
                cfg.default_runtime = val;
            } else if (key == "归档目录" || key == "archive" || key == "archives" || key == "archive dirs") {
                if (val != "(none)" && val != "none" && val != "无") {
                    std::size_t pos = 0;
                    while (pos <= val.size()) {
                        const std::size_t sep = val.find_first_of(",;", pos);
                        const std::string item = detail::TrimCopy(
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

namespace hxlings::detail {

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

} // namespace hxlings::detail

export namespace hxlings {

std::vector<ConfigThemeItem> ParseConfigThemeList(std::string_view text) {
    std::vector<ConfigThemeItem> out;
    std::string active_name;
    bool in_themes = false;

    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t nl = text.find('\n', start);
        const std::size_t end = (nl == std::string_view::npos) ? text.size() : nl;
        const std::string_view raw = text.substr(start, end - start);
        const std::string trimmed = detail::TrimCopy(raw);
        if (trimmed == "themes:" || trimmed == "主题:") {
            in_themes = true;
        } else if (trimmed.starts_with("active:") || trimmed.starts_with("当前:") || trimmed.starts_with("当前主题:")) {
            const std::size_t colon = trimmed.find(':');
            if (colon != std::string::npos) {
                active_name = detail::TrimCopy(std::string_view(trimmed).substr(colon + 1));
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
            src = detail::TrimCopy(src);
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
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        if (!pl.contains("endpoints") || !pl["endpoints"].is_array()) continue;
        for (const auto& ep : pl["endpoints"]) {
            if (!ep.is_object()) continue;
            const std::string url = detail::JsonString(ep, "url");
            if (url.empty()) continue;
            out.push_back(NetworkEndpoint{
                .region = detail::JsonString(ep, "region"),
                .resource = detail::JsonString(ep, "resource"),
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
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        res.host = detail::JsonString(pl, "host");
        res.status = detail::JsonString(pl, "status");
        res.reachable = detail::JsonBool(pl, "reachable");
        res.http_status = detail::JsonInt(pl, "http_status", 0);
        if (pl.contains("elapsed_seconds") && pl["elapsed_seconds"].is_number()) {
            res.elapsed_seconds = pl["elapsed_seconds"].get<double>();
        }
        res.proxy = detail::JsonBool(pl, "proxy");
        res.ok = true;
    }
    if (!res.ok && res.status.empty() && !res.error_message.empty()) {
        res.status = "error";
    }
    return res;
}

SubosStatusSnapshot ParseSubosStatusJson(std::string_view raw) {
    SubosStatusSnapshot snap;
    snap.raw_json = detail::TrimCopy(raw);
    const auto obj = detail::TryParseJsonObject(raw);
    if (!obj) return snap;

    snap.instance = detail::JsonString(*obj, "instance");
    snap.policy_source = detail::JsonString(*obj, "policy_source");
    if (obj->contains("effective") && (*obj)["effective"].is_object()) {
        const auto& eff = (*obj)["effective"];
        snap.enters = detail::JsonBool(eff, "enters");
        if (eff.contains("spec") && eff["spec"].is_object()) {
            snap.backend = detail::JsonString(eff["spec"], "backend");
        }
        snap.gates = detail::ParseGateArray(eff, "gates");
    }
    if (obj->contains("requested") && (*obj)["requested"].is_object()) {
        snap.requested = detail::ParseSubosPolicyObj((*obj)["requested"]);
    }
    return snap;
}

SubosPolicySnapshot ParseSubosConfigJson(std::string_view raw) {
    const auto obj = detail::TryParseJsonObject(raw);
    if (!obj) return {};
    return detail::ParseSubosPolicyObj(*obj);
}

SubosDoctorSnapshot ParseSubosDoctorJson(std::string_view raw) {
    SubosDoctorSnapshot snap;
    snap.raw_json = detail::TrimCopy(raw);
    const auto obj = detail::TryParseJsonObject(raw);
    if (!obj) return snap;

    snap.errors = detail::JsonInt(*obj, "errors", 0);
    snap.gates = detail::ParseGateArray(*obj, "gates");
    if (obj->contains("instances") && (*obj)["instances"].is_array()) {
        for (const auto& inst : (*obj)["instances"]) {
            if (!inst.is_object()) continue;
            const std::string inst_name = detail::JsonString(inst, "instance");
            if (!inst.contains("findings") || !inst["findings"].is_array()) continue;
            for (const auto& f : inst["findings"]) {
                if (!f.is_object()) continue;
                snap.findings.push_back(SubosDoctorFinding{
                    .instance = inst_name,
                    .check = detail::JsonString(f, "check"),
                    .level = detail::JsonString(f, "level"),
                    .detail = detail::JsonString(f, "detail"),
                    .fix = detail::JsonString(f, "fix"),
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
        const std::string line = detail::TrimCopy(raw.substr(start, end - start));
        if (!line.empty() && line.front() == '{') {
            if (const auto obj = detail::TryParseJsonObject(line)) {
                out.push_back(detail::ParseSubosSessionObj(*obj, line));
            }
        } else if (!line.empty() && line.front() == '[') {
            try {
                auto arr = detail::json::parse(line);
                if (arr.is_array()) {
                    for (const auto& el : arr) {
                        if (el.is_object()) out.push_back(detail::ParseSubosSessionObj(el, {}));
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
        const std::string line = detail::TrimCopy(raw.substr(start, end - start));
        if (!line.empty() && line.front() == '{') {
            if (const auto obj = detail::TryParseJsonObject(line)) {
                if (obj->contains("requests") && (*obj)["requests"].is_array()) {
                    for (const auto& el : (*obj)["requests"]) {
                        if (el.is_object()) out.push_back(detail::ParseSubosRequestObj(el, {}));
                    }
                } else {
                    out.push_back(detail::ParseSubosRequestObj(*obj, line));
                }
            }
        } else if (!line.empty() && line.front() == '[') {
            try {
                auto arr = detail::json::parse(line);
                if (arr.is_array()) {
                    for (const auto& el : arr) {
                        if (el.is_object()) out.push_back(detail::ParseSubosRequestObj(el, {}));
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
    snap.raw_json = detail::TrimCopy(raw);
    const auto obj = detail::TryParseJsonObject(raw);
    if (!obj) return snap;

    snap.instance = detail::JsonString(*obj, "instance");
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
            rs.id = detail::JsonString(s, "id");
            rs.backend = detail::JsonString(s, "backend");
            rs.started = detail::JsonString(s, "started");
            rs.commands = detail::JsonInt(s, "commands", 0);
            rs.executions = detail::JsonInt(s, "executions", 0);
            rs.exit_code = detail::JsonInt(s, "exit", 0);
            rs.duration_ms = detail::JsonUint64(s, "ms", 0);
            rs.denied = detail::JsonStringArray(s, "denied");
            rs.changed = detail::JsonStringArray(s, "changed");
            rs.programs = detail::JsonStringArray(s, "programs");
            snap.sessions.push_back(std::move(rs));
        }
    }
    return snap;
}

std::string FormatNdjsonInfoPanel(const std::vector<NdjsonEvent>& events) {
    std::string out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "info_panel") continue;
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        const std::string title = detail::JsonString(pl, "title");
        if (!title.empty()) {
            out += "=== " + title + " ===\n";
        }
        auto append_fields = [&](std::string_view key) {
            if (!pl.contains(std::string(key)) || !pl[std::string(key)].is_array()) return;
            for (const auto& f : pl[std::string(key)]) {
                if (!f.is_object()) continue;
                const std::string label = detail::JsonString(f, "label");
                const std::string value = detail::JsonString(f, "value");
                const bool alert = detail::JsonBool(f, "alert");
                if (label.empty() && value.empty()) continue;
                if (alert) out += "[!] ";
                out += label + ": " + value + "\n";
            }
        };
        append_fields("fields");
        append_fields("extra_fields");
    }
    return detail::TrimCopy(out);
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
        const auto obj = detail::TryParseJsonObject(ev.raw_line);
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
                        const std::string name = detail::JsonString(item, "package");
                        const std::string alt_name = name.empty() ? detail::JsonString(item, "name") : name;
                        const std::string status = detail::JsonString(item, "status");
                        const std::string diff = detail::JsonString(item, "diff");
                        if (!alt_name.empty()) {
                            out += alt_name;
                            if (!status.empty()) out += " [" + status + "]";
                            const std::string left = detail::JsonString(item, "left");
                            const std::string right = detail::JsonString(item, "right");
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
    return detail::TrimCopy(out);
}

std::string FormatSubosRollbackListEvents(const std::vector<NdjsonEvent>& events) {
    std::string out;
    for (const auto& ev : events) {
        if (ev.kind == "error" && !ev.message.empty()) {
            out += ev.message + "\n";
            if (const auto obj = detail::TryParseJsonObject(ev.raw_line)) {
                const std::string hint = detail::JsonString(*obj, "hint");
                if (!hint.empty()) out += "提示: " + hint + "\n";
            }
        } else if (ev.kind == "data") {
            const auto obj = detail::TryParseJsonObject(ev.raw_line);
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
    return detail::TrimCopy(out);
}

} // namespace hxlings

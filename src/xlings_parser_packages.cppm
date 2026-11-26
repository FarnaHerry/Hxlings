export module xlings.parser.packages;

import std;
import nlohmann.json;
import xlings.types;
import xlings.parser.common;


export namespace hxlings {

XlingsVersionInfo ParseVersionJson(std::string_view text) {
    XlingsVersionInfo info;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t nl = text.find('\n', start);
        const std::size_t end = (nl == std::string_view::npos) ? text.size() : nl;
        const auto obj = parser_detail::TryParseJsonObject(text.substr(start, end - start));
        if (obj && obj->contains("version")) {
            info.version = parser_detail::JsonString(*obj, "version");
            info.protocol_version = parser_detail::JsonString(*obj, "protocol_version");
            info.capabilities = parser_detail::JsonStringArray(*obj, "capabilities");
            info.features = parser_detail::JsonStringArray(*obj, "features");
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
        const auto obj = parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        env.xlings_home = parser_detail::JsonString(pl, "xlingsHome");
        env.data_dir = parser_detail::JsonString(pl, "dataDir");
        env.subos_dir = parser_detail::JsonString(pl, "subosDir");
        env.bin_dir = parser_detail::JsonString(pl, "binDir");
        env.lib_dir = parser_detail::JsonString(pl, "libDir");
        env.active_subos = parser_detail::JsonString(pl, "activeSubos");
        env.mirror = parser_detail::JsonString(pl, "mirror");
        env.lang = parser_detail::JsonString(pl, "lang");
    }
    return env;
}

SystemStatus ParseSystemStatusEvents(const std::vector<NdjsonEvent>& events) {
    SystemStatus status;
    for (const auto& ev : events) {
        if (ev.kind != "data") continue;
        const auto obj = parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];

        if (ev.data_kind == "system_info") {
            status.title = parser_detail::JsonString(pl, "title");
            if (pl.contains("fields") && pl["fields"].is_array()) {
                for (const auto& f : pl["fields"]) {
                    if (!f.is_object()) continue;
                    status.fields.push_back(KeyValueField{
                        .label = parser_detail::JsonString(f, "label"),
                        .value = parser_detail::JsonString(f, "value"),
                        .highlight = parser_detail::JsonBool(f, "highlight"),
                    });
                }
            }
        } else if (ev.data_kind == "binding_state") {
            if (pl.contains("findings") && pl["findings"].is_array()) {
                for (const auto& f : pl["findings"]) {
                    if (!f.is_object()) continue;
                    status.findings.push_back(BindingFinding{
                        .code = parser_detail::JsonString(f, "code"),
                        .severity = parser_detail::JsonString(f, "severity"),
                        .subos = parser_detail::JsonString(f, "subos"),
                        .target = parser_detail::JsonString(f, "target"),
                        .version = parser_detail::JsonString(f, "version"),
                        .summary = parser_detail::JsonString(f, "summary"),
                        .hint = parser_detail::JsonString(f, "hint"),
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
        const auto obj = parser_detail::TryParseJsonObject(ev.raw_line);
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
        const auto obj = parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        if (!pl.contains("fields") || !pl["fields"].is_array()) continue;
        for (const auto& f : pl["fields"]) {
            if (!f.is_object()) continue;
            out.push_back(InstalledVersionItem{
                .version = parser_detail::JsonString(f, "label"),
                .path = parser_detail::JsonString(f, "value"),
                .active = parser_detail::JsonBool(f, "highlight"),
            });
        }
    }
    return out;
}

std::vector<SearchPkg> ParseSearchPackagesEvents(const std::vector<NdjsonEvent>& events) {
    std::vector<SearchPkg> out;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "packages") continue;
        const auto obj = parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_array()) continue;
        for (const auto& item : (*obj)["payload"]) {
            if (!item.is_object()) continue;
            SearchPkg pkg;
            pkg.package = parser_detail::JsonString(item, "package");
            pkg.ns = parser_detail::JsonString(item, "namespace");
            pkg.name = parser_detail::JsonString(item, "name");
            pkg.version = parser_detail::JsonString(item, "version");
            pkg.description = parser_detail::NormalizeDescription(parser_detail::JsonString(item, "description"));
            pkg.status = parser_detail::JsonString(item, "status");
            pkg.index = parser_detail::JsonString(item, "index");
            pkg.categories = parser_detail::JsonStringArray(item, "categories");
            pkg.keywords = parser_detail::JsonStringArray(item, "keywords");
            pkg.archs = parser_detail::JsonStringArray(item, "archs");
            out.push_back(std::move(pkg));
        }
    }
    return out;
}

PackageDetail ParsePackageInfoEvents(const std::vector<NdjsonEvent>& events) {
    PackageDetail detail;
    for (const auto& ev : events) {
        if (ev.kind != "data" || ev.data_kind != "info_panel") continue;
        const auto obj = parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        detail.title = parser_detail::JsonString(pl, "title");

        if (pl.contains("package") && pl["package"].is_object()) {
            const auto& p = pl["package"];
            detail.package = parser_detail::JsonString(p, "package");
            detail.ns = parser_detail::JsonString(p, "namespace");
            detail.name = parser_detail::JsonString(p, "name");
            detail.description = parser_detail::NormalizeDescription(parser_detail::JsonString(p, "description"));
            detail.status = parser_detail::JsonString(p, "status");
            detail.selected_version = parser_detail::JsonString(p, "version");
            detail.available_versions = parser_detail::JsonStringArray(p, "versions");
            detail.categories = parser_detail::JsonStringArray(p, "categories");
            detail.archs = parser_detail::JsonStringArray(p, "archs");
        }
        if (pl.contains("fields") && pl["fields"].is_array()) {
            for (const auto& f : pl["fields"]) {
                if (!f.is_object()) continue;
                detail.fields.push_back(KeyValueField{
                    .label = parser_detail::JsonString(f, "label"),
                    .value = parser_detail::JsonString(f, "value"),
                    .highlight = parser_detail::JsonBool(f, "highlight"),
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
        const auto obj = parser_detail::TryParseJsonObject(ev.raw_line);
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
        const auto obj = parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];

        RemovePlan plan;
        plan.name = parser_detail::JsonString(pl, "name");
        plan.older_than = parser_detail::JsonString(pl, "older_than");
        plan.dry_run = parser_detail::JsonBool(pl, "dry_run", true);
        plan.total_bytes = parser_detail::JsonUint64(pl, "bytes");

        if (pl.contains("remove") && pl["remove"].is_array()) {
            for (const auto& r : pl["remove"]) {
                if (!r.is_object()) continue;
                plan.remove.push_back(RemovePlanItem{
                    .version = parser_detail::JsonString(r, "version"),
                    .bytes = parser_detail::JsonUint64(r, "bytes"),
                });
            }
        }
        if (pl.contains("keep") && pl["keep"].is_array()) {
            for (const auto& k : pl["keep"]) {
                if (!k.is_object()) continue;
                plan.keep.push_back(RemovePlanKept{
                    .version = parser_detail::JsonString(k, "version"),
                    .reason = parser_detail::JsonString(k, "reason"),
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
        const auto obj = parser_detail::TryParseJsonObject(ev.raw_line);
        if (!obj || !obj->contains("payload") || !(*obj)["payload"].is_object()) continue;
        const auto& pl = (*obj)["payload"];
        if (!pl.contains("targets") || !pl["targets"].is_array()) continue;

        out.clear();
        for (const auto& t : pl["targets"]) {
            if (!t.is_object()) continue;
            PayloadStatusItem item;
            item.request = parser_detail::JsonString(t, "request");
            item.ns = parser_detail::JsonString(t, "namespace");
            item.name = parser_detail::JsonString(t, "name");
            item.version = parser_detail::JsonString(t, "version");
            item.origin = parser_detail::JsonString(t, "origin");
            item.payload_dir = parser_detail::JsonString(t, "payload_dir");
            item.error = parser_detail::JsonString(t, "error");
            item.revision = parser_detail::JsonInt(t, "revision");
            if (t.contains("installed_revision") && t["installed_revision"].is_number_integer()) {
                item.installed_revision = t["installed_revision"].get<int>();
            }
            item.complete = parser_detail::JsonBool(t, "complete");
            item.current = parser_detail::JsonBool(t, "current");
            item.usable = parser_detail::JsonBool(t, "usable");
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
            const std::string first_line = parser_detail::TrimCopy(
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

} // namespace hxlings

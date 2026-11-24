export module hxlings.luban.client;

import std;
import mcpplibs.os;
import nlohmann.json;
import xlings.types;
import xlings.process;
import hxlings.luban.types;

namespace hxlings::detail {

namespace os = mcpplibs::os;

inline bool ContainsRegistrySegment(std::string_view value) {
    return value.find("/.mcpp/registry") != std::string_view::npos ||
           value.find("\\.mcpp\\registry") != std::string_view::npos;
}

inline bool IsRunnableFile(const std::filesystem::path& path) {
    if (path.empty()) return false;
    std::error_code ec;
    const std::filesystem::file_status st = std::filesystem::status(path, ec);
    if (ec || !std::filesystem::is_regular_file(st)) return false;
    if constexpr (os::is_posix) {
        constexpr auto exec_bits = std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::group_exec |
                                   std::filesystem::perms::others_exec;
        if ((st.permissions() & exec_bits) == std::filesystem::perms::none) {
            return false;
        }
    }
    return true;
}

inline std::string TrimWhitespace(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

inline std::string ResolveNamedExecutable(std::string_view bin_stem, std::string_view env_override) {
    const std::string bin_name = std::string(bin_stem) + std::string(os::conventions().exe_suffix);
    const std::string bundle_dir = os::env::get("MCPP_BUNDLE_DIR").value_or(std::string{});

    if (!env_override.empty()) {
        if (const auto over = os::env::get(env_override); over.has_value() && !over->empty()) {
            const std::filesystem::path p = os::fs::from_utf8(*over);
            if (IsRunnableFile(p)) {
                return os::fs::native(p);
            }
        }
    }

    for (const auto& dir : os::env::search_path()) {
        if (dir.empty() || PathMatchesPrefixOrRegistry(os::fs::utf8(dir), bundle_dir)) continue;
        const std::filesystem::path candidate = dir / bin_name;
        if (IsRunnableFile(candidate)) {
            return os::fs::native(candidate);
        }
    }

    if (const auto xlings_home = os::env::get("XLINGS_HOME");
        xlings_home.has_value() && !xlings_home->empty() && !ContainsRegistrySegment(*xlings_home)) {
        const std::filesystem::path xh = os::fs::from_utf8(*xlings_home);
        for (const auto& candidate : {xh / "subos" / "current" / "bin" / bin_name, xh / "bin" / bin_name}) {
            if (IsRunnableFile(candidate)) {
                return os::fs::native(candidate);
            }
        }
    }

    if (const auto home = os::dirs::home(); home.has_value() && !home->empty()) {
        for (const auto& candidate : {*home / ".xlings" / "subos" / "current" / "bin" / bin_name,
                                      *home / ".xlings" / "bin" / bin_name}) {
            if (IsRunnableFile(candidate)) {
                return os::fs::native(candidate);
            }
        }
    }

    if constexpr (os::is_macos) {
        for (const char* brew_dir : {"/opt/homebrew/bin", "/usr/local/bin"}) {
            const std::filesystem::path candidate = std::filesystem::path(brew_dir) / bin_name;
            if (IsRunnableFile(candidate)) {
                return os::fs::native(candidate);
            }
        }
    }

    return {};
}

inline std::string JsonString(const nlohmann::json& obj, std::string_view key, std::string fallback = {}) {
    if (!obj.is_object()) return fallback;
    const auto it = obj.find(std::string(key));
    if (it == obj.end() || it->is_null()) return fallback;
    if (it->is_string()) return it->get<std::string>();
    if (it->is_boolean()) return it->get<bool>() ? "true" : "false";
    if (it->is_number_integer()) return std::to_string(it->get<std::int64_t>());
    return fallback;
}

inline int JsonInt(const nlohmann::json& obj, std::string_view key, int fallback = 0) {
    if (!obj.is_object()) return fallback;
    const auto it = obj.find(std::string(key));
    if (it != obj.end() && it->is_number_integer()) return it->get<int>();
    return fallback;
}

inline bool JsonBool(const nlohmann::json& obj, std::string_view key, bool fallback = false) {
    if (!obj.is_object()) return fallback;
    const auto it = obj.find(std::string(key));
    if (it != obj.end() && it->is_boolean()) return it->get<bool>();
    return fallback;
}

inline std::vector<std::string> JsonStringArray(const nlohmann::json& obj, std::string_view key) {
    std::vector<std::string> out;
    if (!obj.is_object()) return out;
    const auto it = obj.find(std::string(key));
    if (it == obj.end() || !it->is_array()) return out;
    for (const auto& item : *it) {
        if (item.is_string()) {
            out.push_back(item.get<std::string>());
        }
    }
    return out;
}

inline const nlohmann::json& UnwrapLubanResult(const nlohmann::json& root) {
    if (root.is_object()) {
        const auto it = root.find("result");
        if (it != root.end() && it->is_object()) {
            return *it;
        }
    }
    return root;
}

inline std::optional<nlohmann::json> ExtractJsonDocument(std::string_view raw) {
    const std::string trimmed = TrimWhitespace(raw);
    if (trimmed.empty()) return std::nullopt;

    if (nlohmann::json direct = nlohmann::json::parse(trimmed, nullptr, false); !direct.is_discarded()) {
        return direct;
    }

    for (std::size_t pos = 0; pos < trimmed.size(); ++pos) {
        if (pos > 0 && trimmed[pos - 1] != '\n' && trimmed[pos - 1] != '\r') continue;
        std::size_t i = pos;
        while (i < trimmed.size() && (trimmed[i] == ' ' || trimmed[i] == '\t')) ++i;
        if (i < trimmed.size() && (trimmed[i] == '{' || trimmed[i] == '[')) {
            if (nlohmann::json parsed = nlohmann::json::parse(trimmed.substr(i), nullptr, false);
                !parsed.is_discarded()) {
                return parsed;
            }
        }
    }

    std::optional<nlohmann::json> last_line_doc;
    std::size_t line_start = 0;
    while (line_start <= trimmed.size()) {
        const std::size_t nl = trimmed.find('\n', line_start);
        const std::string line = TrimWhitespace(
            nl == std::string::npos
                ? std::string_view(trimmed).substr(line_start)
                : std::string_view(trimmed).substr(line_start, nl - line_start)
        );
        if (!line.empty() && (line.front() == '{' || line.front() == '[')) {
            if (nlohmann::json parsed = nlohmann::json::parse(line, nullptr, false); !parsed.is_discarded()) {
                if (parsed.is_object() && parsed.contains("result")) {
                    return parsed;
                }
                last_line_doc = std::move(parsed);
            }
        }
        if (nl == std::string::npos) break;
        line_start = nl + 1;
    }
    return last_line_doc;
}

} // namespace hxlings::detail

export namespace hxlings {

inline std::string ResolveLubanBinary() {
    return detail::ResolveNamedExecutable("luban", "HXLINGS_LUBAN_BIN");
}

inline std::string ResolveLubanOsBinary() {
    return detail::ResolveNamedExecutable("luban-os", "HXLINGS_LUBAN_OS_BIN");
}

inline LubanHostStatus ParseLubanStatusJson(std::string_view raw) {
    LubanHostStatus out;
    const auto doc_opt = detail::ExtractJsonDocument(raw);
    if (!doc_opt.has_value() || !doc_opt->is_object()) {
        out.raw_error = detail::TrimWhitespace(raw);
        return out;
    }
    const nlohmann::json& doc = detail::UnwrapLubanResult(*doc_opt);

    if (doc.contains("event") && detail::JsonString(doc, "event") == "error") {
        out.raw_error = detail::JsonString(doc, "text");
        return out;
    }

    out.ready = detail::JsonBool(doc, "ready", false);
    out.os = detail::JsonString(doc, "os");
    out.arch = detail::JsonString(doc, "arch");

    if (const auto iso_it = doc.find("isolation"); iso_it != doc.end() && iso_it->is_object()) {
        const nlohmann::json& iso = *iso_it;
        if (const auto b_it = iso.find("backend"); b_it != iso.end() && b_it->is_object()) {
            out.backend_name = detail::JsonString(*b_it, "name");
            out.backend_path = detail::JsonString(*b_it, "path");
            out.backend_source = detail::JsonString(*b_it, "source");
        }
        if (const auto g_it = iso.find("gates"); g_it != iso.end() && g_it->is_array()) {
            for (const auto& g : *g_it) {
                if (!g.is_object()) continue;
                out.gates.push_back(LubanGateStatus{
                    .gate = detail::JsonString(g, "gate"),
                    .enforced = detail::JsonString(g, "enforced"),
                    .reason = detail::JsonString(g, "reason"),
                    .supported = detail::JsonBool(g, "supported", false),
                });
            }
        }
        if (const auto t_it = iso.find("tools"); t_it != iso.end() && t_it->is_array()) {
            for (const auto& t : *t_it) {
                if (!t.is_object()) continue;
                const std::string path = detail::JsonString(t, "path");
                const std::string install_cmd = detail::JsonString(t, "install");
                if (path.empty() && !install_cmd.empty()) {
                    out.missing_tools.push_back(LubanToolStatus{
                        .name = detail::JsonString(t, "name"),
                        .package = detail::JsonString(t, "package"),
                        .install_cmd = install_cmd,
                        .path = path,
                        .available = false,
                    });
                }
            }
        }
    }

    if (const auto m_it = doc.find("modes"); m_it != doc.end() && m_it->is_array()) {
        for (const auto& m : *m_it) {
            if (!m.is_object()) continue;
            LubanModeStatus mode{
                .name = detail::JsonString(m, "name"),
                .how = detail::JsonString(m, "how"),
                .reason = detail::JsonString(m, "reason"),
                .route = detail::JsonString(m, "route"),
                .ready = detail::JsonBool(m, "ready", false),
            };
            if (mode.name == "sandbox") {
                out.sandbox_ready = mode.ready;
            } else if (mode.name == "machine") {
                out.machine_ready = mode.ready;
            } else if (mode.name == "desktop") {
                out.desktop_ready = mode.ready;
                out.desktop_reason = mode.reason;
                out.desktop_route = mode.route;
            }
            out.modes.push_back(std::move(mode));
        }
    }

    if (const auto v_it = doc.find("versions"); v_it != doc.end() && v_it->is_object()) {
        const nlohmann::json& v = *v_it;
        out.xlings_version = detail::JsonString(v, "xlings");
        out.luban_version = detail::JsonString(v, "luban");
        out.luban_os_version = detail::JsonString(v, "luban-os");
        out.protocol_version = detail::JsonString(v, "protocol");
        out.index_commit = detail::JsonString(v, "index");
        out.running_envs = detail::JsonStringArray(v, "running");
    }

    return out;
}

inline std::vector<LubanEnvironment> ParseLubanLsJson(std::string_view raw) {
    std::vector<LubanEnvironment> out;
    const auto doc_opt = detail::ExtractJsonDocument(raw);
    if (!doc_opt.has_value() || !doc_opt->is_object()) return out;

    const nlohmann::json& doc = detail::UnwrapLubanResult(*doc_opt);
    const auto envs_it = doc.find("environments");
    if (envs_it == doc.end() || !envs_it->is_array()) return out;

    for (const auto& item : *envs_it) {
        if (!item.is_object()) continue;
        LubanEnvironment env{
            .name = detail::JsonString(item, "name"),
            .kind = detail::JsonString(item, "kind"),
            .edition = detail::JsonString(item, "edition"),
            .carrier = detail::JsonString(item, "carrier"),
            .shell = detail::JsonString(item, "shell"),
            .desktops = detail::JsonStringArray(item, "desktops"),
            .packages = detail::JsonInt(item, "packages", 0),
            .running = detail::JsonBool(item, "running", false),
        };
        if (!env.name.empty()) {
            out.push_back(std::move(env));
        }
    }
    return out;
}

inline LubanCatalog ParseLubanSearchJson(std::string_view raw) {
    LubanCatalog out;
    const auto doc_opt = detail::ExtractJsonDocument(raw);
    if (!doc_opt.has_value() || !doc_opt->is_object()) return out;

    const nlohmann::json& doc = detail::UnwrapLubanResult(*doc_opt);
    const auto grp_it = doc.find("groups");
    if (grp_it == doc.end() || !grp_it->is_array()) return out;

    for (const auto& grp : *grp_it) {
        if (!grp.is_object()) continue;
        const std::string group_name = detail::JsonString(grp, "group");
        const auto items_it = grp.find("items");
        if (items_it == grp.end() || !items_it->is_array()) continue;

        for (const auto& item : *items_it) {
            if (!item.is_object()) continue;
            LubanCatalogItem ci{
                .group = group_name,
                .name = detail::JsonString(item, "name"),
                .ref = detail::JsonString(item, "ref"),
                .version = detail::JsonString(item, "version"),
                .status = detail::JsonString(item, "status"),
                .source = detail::JsonString(item, "source"),
                .description = detail::JsonString(item, "description"),
                .gpu_hint = detail::JsonString(item, "gpu"),
                .highlights = detail::JsonStringArray(item, "highlights"),
                .tier = detail::JsonInt(item, "tier", 0),
                .available = detail::JsonBool(item, "available", true),
            };
            if (const auto t_it = item.find("title"); t_it != item.end() && t_it->is_object()) {
                ci.title_zh = detail::JsonString(*t_it, "zh");
                if (ci.title_zh.empty()) ci.title_zh = detail::JsonString(*t_it, "en");
            }
            if (const auto s_it = item.find("summary"); s_it != item.end() && s_it->is_object()) {
                ci.summary_zh = detail::JsonString(*s_it, "zh");
                if (ci.summary_zh.empty()) ci.summary_zh = detail::JsonString(*s_it, "en");
            }
            if (group_name == "distributions") {
                out.distributions.push_back(std::move(ci));
            } else if (group_name == "desktops") {
                out.desktops.push_back(std::move(ci));
            }
        }
    }
    return out;
}

inline std::optional<LubanEnvStatusDetail> ParseLubanEnvStatusJson(std::string_view raw) {
    const auto doc_opt = detail::ExtractJsonDocument(raw);
    if (!doc_opt.has_value() || !doc_opt->is_object()) return std::nullopt;
    const nlohmann::json& doc = detail::UnwrapLubanResult(*doc_opt);
    if (doc.contains("event") && detail::JsonString(doc, "event") == "error") {
        return std::nullopt;
    }

    LubanEnvStatusDetail out{
        .name = detail::JsonString(doc, "name"),
        .kind = detail::JsonString(doc, "kind"),
        .edition = detail::JsonString(doc, "edition"),
        .carrier = detail::JsonString(doc, "carrier"),
        .mode = detail::JsonString(doc, "mode"),
        .persist = detail::JsonString(doc, "persist"),
        .enter = detail::JsonString(doc, "enter"),
        .network = detail::JsonString(doc, "network"),
        .shell = detail::JsonString(doc, "shell"),
        .desktop = detail::JsonString(doc, "desktop"),
        .desktops = detail::JsonStringArray(doc, "desktops"),
        .needs = detail::JsonStringArray(doc, "needs"),
        .notes = detail::JsonStringArray(doc, "notes"),
        .unmet = detail::JsonStringArray(doc, "unmet"),
        .packages = detail::JsonInt(doc, "packages", 0),
        .running = detail::JsonBool(doc, "running", false),
    };
    if (out.name.empty()) return std::nullopt;
    return out;
}

inline std::optional<LubanEnvConfigDetail> ParseLubanEnvConfigJson(std::string_view raw) {
    const auto doc_opt = detail::ExtractJsonDocument(raw);
    if (!doc_opt.has_value() || !doc_opt->is_object()) return std::nullopt;
    const nlohmann::json& doc = detail::UnwrapLubanResult(*doc_opt);
    if (doc.contains("event") && detail::JsonString(doc, "event") == "error") {
        return std::nullopt;
    }

    LubanEnvConfigDetail out{
        .name = detail::JsonString(doc, "name"),
        .mode = detail::JsonString(doc, "mode"),
        .persist = detail::JsonString(doc, "persist"),
        .shell = detail::JsonString(doc, "shell"),
        .desktop = detail::JsonString(doc, "desktop"),
        .on_close = detail::JsonString(doc, "on_close"),
        .files = detail::JsonString(doc, "files"),
        .kernel = detail::JsonString(doc, "kernel"),
    };

    if (const auto iso_it = doc.find("isolation"); iso_it != doc.end() && iso_it->is_object()) {
        const nlohmann::json& iso = *iso_it;
        out.policy_extends = detail::JsonString(iso, "extends");
        out.policy_source = detail::JsonString(iso, "source");
        out.mounts = detail::JsonStringArray(iso, "mounts");

        if (const auto obs_it = iso.find("observe"); obs_it != iso.end() && obs_it->is_object()) {
            out.observe_level = detail::JsonString(*obs_it, "level");
        }
        if (const auto perm_it = iso.find("permissions"); perm_it != iso.end() && perm_it->is_object()) {
            out.index_update_policy = detail::JsonString(*perm_it, "index_update");
            if (const auto f_it = perm_it->find("fetch"); f_it != perm_it->end() && f_it->is_object()) {
                out.fetch_policy = detail::JsonString(*f_it, "default");
            }
        }
        if (const auto inner_it = iso.find("isolation"); inner_it != iso.end() && inner_it->is_object()) {
            const nlohmann::json& inner = *inner_it;
            out.net = detail::JsonString(inner, "net");
            out.proxy = detail::JsonString(inner, "proxy");
            out.no_degrade = detail::JsonBool(inner, "no_degrade", false);
            out.grants = detail::JsonStringArray(inner, "grants");
            out.grants_allowed = detail::JsonStringArray(inner, "grants_allowed");
            out.env_pass = detail::JsonStringArray(inner, "env_pass");
        }
    }

    if (out.name.empty()) return std::nullopt;
    return out;
}

inline std::string FormatLubanLogsText(std::string_view raw) {
    const std::string trimmed = detail::TrimWhitespace(raw);
    if (trimmed.empty() || trimmed == "[]") {
        return "暂无日志记录";
    }

    auto format_obj = [](const nlohmann::json& item) -> std::string {
        if (!item.is_object()) return item.dump();
        const std::string ts = !detail::JsonString(item, "timestamp").empty()
                                   ? detail::JsonString(item, "timestamp")
                                   : (!detail::JsonString(item, "time").empty()
                                          ? detail::JsonString(item, "time")
                                          : detail::JsonString(item, "ts"));
        const std::string src = !detail::JsonString(item, "source").empty()
                                    ? detail::JsonString(item, "source")
                                    : detail::JsonString(item, "app");
        const std::string msg = !detail::JsonString(item, "text").empty()
                                    ? detail::JsonString(item, "text")
                                    : detail::JsonString(item, "message");
        std::string line;
        if (!ts.empty()) line += "[" + ts + "] ";
        if (!src.empty()) line += "[" + src + "] ";
        line += msg.empty() ? item.dump() : msg;
        return line;
    };

    if (const auto doc_opt = detail::ExtractJsonDocument(trimmed); doc_opt.has_value()) {
        if (doc_opt->is_array()) {
            if (doc_opt->empty()) return "暂无日志记录";
            std::string out;
            for (const auto& entry : *doc_opt) {
                if (!out.empty()) out.push_back('\n');
                out += format_obj(entry);
            }
            return out;
        }
        if (doc_opt->is_object()) {
            if (const auto it = doc_opt->find("logs"); it != doc_opt->end() && it->is_array()) {
                if (it->empty()) return "暂无日志记录";
                std::string out;
                for (const auto& entry : *it) {
                    if (!out.empty()) out.push_back('\n');
                    out += format_obj(entry);
                }
                return out;
            }
            return format_obj(*doc_opt);
        }
    }
    return trimmed;
}

inline std::string FormatLubanHistoryText(std::string_view raw) {
    const std::string trimmed = detail::TrimWhitespace(raw);
    if (trimmed.empty() || trimmed == "[]") {
        return "暂无历史代记录";
    }

    if (const auto doc_opt = detail::ExtractJsonDocument(trimmed); doc_opt.has_value()) {
        if (doc_opt->is_object()) {
            if (detail::JsonString(*doc_opt, "event") == "error") {
                const std::string txt = detail::JsonString(*doc_opt, "text");
                const std::string hint = detail::JsonString(*doc_opt, "hint");
                return hint.empty() ? txt : (txt + "\n提示: " + hint);
            }
            const nlohmann::json& inner = detail::UnwrapLubanResult(*doc_opt);
            if (const auto gen_it = inner.find("generations");
                gen_it != inner.end() && gen_it->is_array()) {
                if (gen_it->empty()) return "暂无历史代记录";
                std::string out;
                for (const auto& g : *gen_it) {
                    if (!out.empty()) out.push_back('\n');
                    out += g.is_string() ? g.get<std::string>() : g.dump();
                }
                return out;
            }
            return inner.dump(2);
        }
        if (doc_opt->is_array()) {
            if (doc_opt->empty()) return "暂无历史代记录";
            std::string out;
            for (const auto& g : *doc_opt) {
                if (!out.empty()) out.push_back('\n');
                out += g.is_string() ? g.get<std::string>() : g.dump();
            }
            return out;
        }
    }
    return trimmed;
}

inline std::vector<std::string> BuildLubanNewArgs(const LubanCreateOptions& opts) {
    std::vector<std::string> args{"new", opts.name};
    auto push_opt = [&](std::string_view flag, std::string_view val) {
        if (!val.empty()) {
            args.emplace_back(flag);
            args.emplace_back(val);
        }
    };
    push_opt("--edition", opts.edition);
    for (const auto& xp : opts.xpkgs) {
        push_opt("--xpkg", xp);
    }
    push_opt("--desktop", opts.desktop);
    push_opt("--enter", opts.enter_mode);
    push_opt("--mode", opts.mode);
    push_opt("--persist", opts.persist);
    push_opt("--shell", opts.shell);
    push_opt("--login", opts.login);
    push_opt("--on-close", opts.on_close);
    push_opt("--policy", opts.policy);
    push_opt("--tz", opts.tz);
    for (const auto& grant : opts.allows) {
        push_opt("--allow", grant);
    }
    push_opt("--runtime", opts.runtime);
    push_opt("--user", opts.user);
    if (opts.no_password) args.emplace_back("--no-password");
    push_opt("--proxy", opts.proxy);
    push_opt("--carrier", opts.carrier);
    if (opts.dry_run) args.emplace_back("--dry-run");
    args.emplace_back("--agent");
    args.emplace_back("--yes");
    return args;
}

inline std::vector<std::string> BuildLubanConfigArgs(std::string_view name, const LubanConfigPatch& patch) {
    std::vector<std::string> args{"config", std::string(name)};
    auto push_opt = [&](std::string_view flag, const std::optional<std::string>& val) {
        if (val.has_value() && !val->empty()) {
            args.emplace_back(flag);
            args.push_back(*val);
        }
    };
    push_opt("--mode", patch.mode);
    push_opt("--persist", patch.persist);
    push_opt("--shell", patch.shell);
    push_opt("--login", patch.login);
    push_opt("--runtime", patch.runtime);
    push_opt("--desktop", patch.desktop);
    push_opt("--on-close", patch.on_close);
    push_opt("--enter", patch.enter_mode);
    push_opt("--files", patch.files);
    push_opt("--kernel", patch.kernel);
    push_opt("--policy", patch.policy);
    push_opt("--proxy", patch.proxy);
    push_opt("--tz", patch.tz);
    push_opt("--fetch", patch.fetch);
    push_opt("--observe", patch.observe);
    for (const auto& g : patch.allow) {
        if (!g.empty()) {
            args.emplace_back("--allow");
            args.push_back(g);
        }
    }
    for (const auto& g : patch.disallow) {
        if (!g.empty()) {
            args.emplace_back("--disallow");
            args.push_back(g);
        }
    }
    for (const auto& m : patch.mounts) {
        if (!m.empty()) {
            args.emplace_back("--mount");
            args.push_back(m);
        }
    }
    for (const auto& m : patch.unmounts) {
        if (!m.empty()) {
            args.emplace_back("--unmount");
            args.push_back(m);
        }
    }
    args.emplace_back("--agent");
    args.emplace_back("--yes");
    return args;
}

inline std::vector<std::string> BuildLubanExportArgs(const LubanExportOptions& opts) {
    std::vector<std::string> args{"export", opts.name, opts.file_path};
    if (!opts.format.empty()) {
        args.emplace_back("--format");
        args.push_back(opts.format);
    }
    if (!opts.size.empty()) {
        args.emplace_back("--size");
        args.push_back(opts.size);
    }
    if (!opts.boot.empty()) {
        args.emplace_back("--boot");
        args.push_back(opts.boot);
    }
    args.emplace_back("--agent");
    args.emplace_back("--yes");
    return args;
}

using LubanEventCallback = std::function<void(const NdjsonEvent&)>;

class LubanClient {
public:
    explicit LubanClient(
        std::string binary_path = ResolveLubanBinary(),
        std::string luban_os_path = ResolveLubanOsBinary()
    )
        : binary_path_(std::move(binary_path)),
          luban_os_path_(std::move(luban_os_path)) {}

    [[nodiscard]] bool IsAvailable() const noexcept {
        return !binary_path_.empty();
    }

    [[nodiscard]] bool IsLubanOsInstalled() const noexcept {
        return !luban_os_path_.empty();
    }

    [[nodiscard]] const std::string& LubanBinaryPath() const noexcept {
        return binary_path_;
    }

    [[nodiscard]] const std::string& LubanOsBinaryPath() const noexcept {
        return luban_os_path_;
    }

    void RefreshDiscoveredPaths() {
        binary_path_ = ResolveLubanBinary();
        luban_os_path_ = ResolveLubanOsBinary();
    }

    CommandOutcome RunCli(
        const std::vector<std::string>& sub_args,
        LubanEventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        CommandOutcome outcome;
        if (binary_path_.empty()) {
            outcome.ok = false;
            outcome.exit_code = 127;
            outcome.error_message = "未检测到 `luban` 可执行文件，请先安装 luban";
            return outcome;
        }

        std::vector<std::string> cmd;
        cmd.reserve(1 + sub_args.size());
        cmd.push_back(binary_path_);
        cmd.insert(cmd.end(), sub_args.begin(), sub_args.end());

        const ProcessResult proc = RunProcessStreaming(
            cmd,
            [&](const std::string& line) {
                NdjsonEvent ev;
                ev.kind = "raw";
                ev.message = line;
                ev.raw_line = line;
                if (on_event) on_event(ev);
                outcome.events.push_back(std::move(ev));
            },
            std::move(control)
        );

        outcome.raw_output = proc.raw_output;
        outcome.exit_code = proc.exit_code;
        outcome.error_message = proc.error_message;
        outcome.ok = !proc.cancelled && proc.exit_code == 0;
        if (!outcome.ok && outcome.error_message.empty()) {
            outcome.error_message = proc.cancelled
                                        ? "Cancelled"
                                        : ("Command exited with code " + std::to_string(outcome.exit_code));
        }
        return outcome;
    }

    [[nodiscard]] LubanHostStatus QueryHostStatus() const {
        if (binary_path_.empty()) {
            LubanHostStatus unavailable;
            unavailable.raw_error = "未检测到 luban 命令";
            return unavailable;
        }
        const CommandOutcome out = RunCli({"status", "--json"});
        return ParseLubanStatusJson(out.raw_output);
    }

    [[nodiscard]] std::vector<LubanEnvironment> ListEnvironments() const {
        if (binary_path_.empty()) return {};
        const CommandOutcome out = RunCli({"ls", "--json"});
        return ParseLubanLsJson(out.raw_output);
    }

    [[nodiscard]] LubanCatalog SearchCatalog() const {
        if (binary_path_.empty()) return {};
        const CommandOutcome out = RunCli({"search", "--json"});
        return ParseLubanSearchJson(out.raw_output);
    }

    [[nodiscard]] std::optional<LubanEnvStatusDetail> QueryEnvStatus(std::string_view name) const {
        if (binary_path_.empty() || name.empty()) return std::nullopt;
        const CommandOutcome out = RunCli({"status", std::string(name), "--json"});
        return ParseLubanEnvStatusJson(out.raw_output);
    }

    [[nodiscard]] std::optional<LubanEnvConfigDetail> QueryEnvConfig(std::string_view name) const {
        if (binary_path_.empty() || name.empty()) return std::nullopt;
        const CommandOutcome out = RunCli({"config", std::string(name), "--json"});
        return ParseLubanEnvConfigJson(out.raw_output);
    }

    [[nodiscard]] std::string QueryEnvHistory(std::string_view name) const {
        if (binary_path_.empty() || name.empty()) return "暂无历史代记录";
        const CommandOutcome out = RunCli({"history", std::string(name), "--json"});
        return FormatLubanHistoryText(out.raw_output);
    }

    [[nodiscard]] std::string QueryEnvLogs(std::string_view name, std::string_view since = {}) const {
        if (binary_path_.empty() || name.empty()) return "暂无日志记录";
        std::vector<std::string> args{"logs", std::string(name), "--json"};
        if (!since.empty()) {
            args.emplace_back("--since");
            args.emplace_back(since);
        }
        const CommandOutcome out = RunCli(args);
        return FormatLubanLogsText(out.raw_output);
    }

    CommandOutcome CreateEnvironment(
        const LubanCreateOptions& opts,
        LubanEventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli(BuildLubanNewArgs(opts), std::move(on_event), std::move(control));
    }

    CommandOutcome ConfigureEnvironment(
        std::string_view name,
        const LubanConfigPatch& patch,
        LubanEventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli(BuildLubanConfigArgs(name, patch), std::move(on_event), std::move(control));
    }

    CommandOutcome RunCommand(
        std::string_view name,
        const std::vector<std::string>& command_argv,
        LubanEventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> args{"run", std::string(name), "--agent"};
        args.insert(args.end(), command_argv.begin(), command_argv.end());
        return RunCli(args, std::move(on_event), std::move(control));
    }

    CommandOutcome StopEnvironment(
        std::string_view name,
        LubanEventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli({"stop", std::string(name), "--agent", "--yes"}, std::move(on_event), std::move(control));
    }

    CommandOutcome UpgradeEnvironment(
        std::string_view name,
        std::string_view target_version = {},
        bool dry_run = false,
        LubanEventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> args{"upgrade", std::string(name)};
        if (!target_version.empty()) {
            args.emplace_back("--to");
            args.emplace_back(target_version);
        }
        if (dry_run) args.emplace_back("--dry-run");
        args.emplace_back("--agent");
        args.emplace_back("--yes");
        return RunCli(args, std::move(on_event), std::move(control));
    }

    CommandOutcome RollbackEnvironment(
        std::string_view name,
        std::string_view generation = {},
        LubanEventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> args{"rollback", std::string(name)};
        if (!generation.empty()) {
            args.emplace_back("--to");
            args.emplace_back(generation);
        }
        args.emplace_back("--agent");
        args.emplace_back("--yes");
        return RunCli(args, std::move(on_event), std::move(control));
    }

    CommandOutcome ExportEnvironment(
        const LubanExportOptions& opts,
        LubanEventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli(BuildLubanExportArgs(opts), std::move(on_event), std::move(control));
    }

    CommandOutcome ImportEnvironment(
        std::string_view archive_path,
        std::string_view custom_name = {},
        LubanEventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        std::vector<std::string> args{"import", std::string(archive_path)};
        if (!custom_name.empty()) {
            args.emplace_back("--name");
            args.emplace_back(custom_name);
        }
        args.emplace_back("--agent");
        args.emplace_back("--yes");
        return RunCli(args, std::move(on_event), std::move(control));
    }

    CommandOutcome RemoveEnvironment(
        std::string_view name,
        LubanEventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli({"rm", std::string(name), "--agent", "--yes"}, std::move(on_event), std::move(control));
    }

    CommandOutcome SetupHost(
        LubanEventCallback on_event = {},
        std::shared_ptr<ActiveProcessControl> control = nullptr
    ) const {
        return RunCli({"setup", "--agent", "--yes"}, std::move(on_event), std::move(control));
    }

    [[nodiscard]] bool LaunchLubanOsGui() const {
        if (luban_os_path_.empty()) return false;
        return LaunchDetachedExecutable(mcpplibs::os::fs::from_utf8(luban_os_path_), {});
    }

private:
    std::string binary_path_;
    std::string luban_os_path_;
};

} // namespace hxlings

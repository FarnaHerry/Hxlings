export module hxlings.luban.stack;

import std;
import mcpplibs.os;
import nlohmann.json;
import hxlings.luban.types;

namespace hxlings::detail {

inline std::string TrimAscii(std::string_view s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])) != 0) ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])) != 0) --e;
    return std::string(s.substr(b, e - b));
}

inline std::string_view StripUtf8Bom(std::string_view s) {
    if (s.size() >= 3 &&
        static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF) {
        return s.substr(3);
    }
    return s;
}

inline bool IsValidStackEnvName(std::string_view name) {
    if (name.empty()) return false;
    for (const char ch : name) {
        const bool ok = (ch >= 'a' && ch <= 'z') ||
                        (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') ||
                        ch == '-' || ch == '_';
        if (!ok) return false;
    }
    return true;
}

inline std::string NormalizePolicyComparable(std::string_view raw) {
    if (raw.starts_with("xim:")) {
        raw.remove_prefix(4);
    }
    if (const std::size_t at = raw.find('@'); at != std::string_view::npos) {
        raw = raw.substr(0, at);
    }
    return std::string(raw);
}

inline bool ContainsString(const std::vector<std::string>& list, std::string_view value) {
    return std::ranges::any_of(list, [value](const std::string& item) { return item == value; });
}

inline std::string StripTomlComment(std::string_view line) {
    bool in_double = false;
    bool in_single = false;
    bool escaped = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (in_double) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_double = false;
            }
        } else if (in_single) {
            if (ch == '\'') {
                in_single = false;
            }
        } else {
            if (ch == '"') {
                in_double = true;
            } else if (ch == '\'') {
                in_single = true;
            } else if (ch == '#') {
                return std::string(line.substr(0, i));
            }
        }
    }
    return std::string(line);
}

inline std::optional<std::string> ParseTomlString(std::string_view raw, std::string& err) {
    const std::string s = TrimAscii(raw);
    if (s.size() < 2) {
        err = "字符串缺少引号定界符";
        return std::nullopt;
    }
    if (s.front() == '\'' && s.back() == '\'') {
        return s.substr(1, s.size() - 2);
    }
    if (s.front() != '"' || s.back() != '"') {
        err = "字符串格式无效";
        return std::nullopt;
    }
    std::string out;
    out.reserve(s.size() - 2);
    for (std::size_t i = 1; i + 1 < s.size(); ++i) {
        const char ch = s[i];
        if (ch != '\\') {
            out.push_back(ch);
            continue;
        }
        if (i + 2 >= s.size()) {
            err = "字符串结尾转义不完整";
            return std::nullopt;
        }
        const char next = s[++i];
        switch (next) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            default:
                err = std::string("不支持的 TOML 转义 \\") + next;
                return std::nullopt;
        }
    }
    return out;
}

inline std::optional<std::vector<std::string>> ParseTomlStringArray(std::string_view raw, std::string& err) {
    const std::string s = TrimAscii(raw);
    if (s.size() < 2 || s.front() != '[' || s.back() != ']') {
        err = "数组缺少方括号 [ ... ]";
        return std::nullopt;
    }
    const std::string_view inner(s.data() + 1, s.size() - 2);
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < inner.size()) {
        while (i < inner.size() && (std::isspace(static_cast<unsigned char>(inner[i])) != 0 || inner[i] == ',')) {
            ++i;
        }
        if (i >= inner.size()) break;
        const char quote = inner[i];
        if (quote != '"' && quote != '\'') {
            err = "数组元素必须为字符串";
            return std::nullopt;
        }
        const std::size_t start = i++;
        bool closed = false;
        bool escaped = false;
        while (i < inner.size()) {
            const char ch = inner[i++];
            if (quote == '"') {
                if (escaped) {
                    escaped = false;
                } else if (ch == '\\') {
                    escaped = true;
                } else if (ch == '"') {
                    closed = true;
                    break;
                }
            } else if (ch == '\'') {
                closed = true;
                break;
            }
        }
        if (!closed) {
            err = "数组内字符串缺少闭合引号";
            return std::nullopt;
        }
        auto parsed = ParseTomlString(inner.substr(start, i - start), err);
        if (!parsed.has_value()) return std::nullopt;
        out.push_back(std::move(*parsed));
    }
    return out;
}

inline int CountUnquotedBracketDelta(std::string_view s) {
    int delta = 0;
    bool in_double = false;
    bool in_single = false;
    bool escaped = false;
    for (const char ch : s) {
        if (in_double) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') in_double = false;
        } else if (in_single) {
            if (ch == '\'') in_single = false;
        } else {
            if (ch == '"') in_double = true;
            else if (ch == '\'') in_single = true;
            else if (ch == '[') ++delta;
            else if (ch == ']') --delta;
        }
    }
    return delta;
}

inline std::string EscapeTomlString(std::string_view s) {
    std::string out = "\"";
    for (const char ch : s) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    out.push_back('"');
    return out;
}

inline std::string EscapeTomlArray(const std::vector<std::string>& items) {
    std::string out = "[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) out += ", ";
        out += EscapeTomlString(items[i]);
    }
    out.push_back(']');
    return out;
}

inline std::string JsonStrField(const nlohmann::json& obj, std::string_view k) {
    const auto it = obj.find(std::string(k));
    if (it != obj.end() && it->is_string()) return it->get<std::string>();
    return {};
}

inline std::vector<std::string> JsonStrListField(const nlohmann::json& obj, std::string_view k) {
    std::vector<std::string> out;
    const auto it = obj.find(std::string(k));
    if (it != obj.end() && it->is_array()) {
        for (const auto& elem : *it) {
            if (elem.is_string()) out.push_back(elem.get<std::string>());
        }
    }
    return out;
}

inline std::string SanitizeStackFileStem(std::string_view raw) {
    std::string out;
    for (const char ch : raw) {
        const bool ok = (ch >= 'a' && ch <= 'z') ||
                        (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') ||
                        ch == '-' || ch == '_';
        if (ok) {
            out.push_back(ch);
        } else if (ch == ' ' || ch == '.' || ch == '/') {
            if (!out.empty() && out.back() != '-') out.push_back('-');
        }
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    if (out.empty()) return "luban-stack";
    return out;
}

} // namespace hxlings::detail

export namespace hxlings {

inline std::vector<std::string> ValidateLubanStackSpec(const LubanStackSpec& spec) {
    std::vector<std::string> errors;
    if (spec.environments.empty()) {
        errors.push_back("清单至少需要声明 1 个 Luban 环境（[[environments]]）");
        return errors;
    }

    auto one_of = [](std::string_view v, std::initializer_list<std::string_view> allowed) {
        for (const auto a : allowed) {
            if (v == a) return true;
        }
        return false;
    };

    std::set<std::string> seen_names;
    for (std::size_t i = 0; i < spec.environments.size(); ++i) {
        const auto& env = spec.environments[i];
        const std::string tag = "环境 #" + std::to_string(i + 1) +
                                (env.name.empty() ? std::string{} : (" (" + env.name + ")"));
        if (!detail::IsValidStackEnvName(env.name)) {
            errors.push_back(tag + ": 名称不能为空且仅允许 ASCII 字母、数字、'-' 或 '_'");
        } else {
            if (env.name == "default" || env.name == "current") {
                errors.push_back(tag + ": '" + env.name + "' 为保留名称，禁止在编排清单中覆盖");
            }
            if (!seen_names.insert(env.name).second) {
                errors.push_back(tag + ": 环境名称 '" + env.name + "' 重复");
            }
        }

        if (!env.mode.empty() && !one_of(env.mode, {"sandbox", "machine"})) {
            errors.push_back(tag + ": mode 仅支持 'sandbox' 或 'machine'");
        }
        if (!env.persist.empty() && !one_of(env.persist, {"all", "home", "none"})) {
            errors.push_back(tag + ": persist 仅支持 'all'、'home' 或 'none'");
        }
        if (!env.enter_mode.empty() && !one_of(env.enter_mode, {"console", "desktop"})) {
            errors.push_back(tag + ": enter_mode 仅支持 'console' 或 'desktop'");
        }
        if (!env.fetch.empty() && !one_of(env.fetch, {"auto", "ask", "deny"})) {
            errors.push_back(tag + ": fetch 仅支持 'auto'、'ask' 或 'deny'");
        }
        if (!env.observe.empty() && !one_of(env.observe, {"off", "basic", "standard", "full"})) {
            errors.push_back(tag + ": observe 仅支持 'off'、'basic'、'standard' 或 'full'");
        }

        const std::string norm_pol = detail::NormalizePolicyComparable(env.policy);
        const std::string norm_ed = detail::NormalizePolicyComparable(env.edition);
        const bool needs_proxy = (norm_pol == "agent-private" || norm_ed == "agent-workspace" ||
                                  norm_ed == "luban-agent-workspace");
        if (needs_proxy && env.proxy.empty()) {
            errors.push_back(tag + ": private/agent-workspace 策略要求提供 proxy（例如 \"socks5h://127.0.0.1:7897\"）");
        }
    }

    return errors;
}

inline LubanStackParseResult ParseLubanStackJson(std::string_view text) {
    LubanStackParseResult res;
    res.detected_format = "json";
    const std::string_view clean = detail::StripUtf8Bom(text);

    nlohmann::json root = nlohmann::json::parse(clean, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        res.errors.push_back("JSON 解析失败：根节点必须是有效的 JSON 对象");
        return res;
    }

    if (const std::string sc = detail::JsonStrField(root, "schema"); !sc.empty()) {
        res.spec.schema = sc;
    }
    res.spec.name = detail::JsonStrField(root, "name");
    res.spec.description = detail::JsonStrField(root, "description");

    const nlohmann::json* envs_arr = nullptr;
    for (const char* key : {"environments", "suboses", "subos"}) {
        if (const auto it = root.find(key); it != root.end() && it->is_array()) {
            envs_arr = &(*it);
            break;
        }
    }
    if (envs_arr == nullptr) {
        res.errors.push_back("JSON 清单缺少 'environments' 数组");
        return res;
    }

    for (const auto& item : *envs_arr) {
        if (!item.is_object()) {
            res.errors.push_back("environments 数组内每项都必须是 JSON 对象");
            return res;
        }
        LubanStackEnvSpec env;
        env.name = detail::JsonStrField(item, "name");
        if (const std::string ed = detail::JsonStrField(item, "edition"); !ed.empty()) {
            env.edition = ed;
        }
        if (const std::string md = detail::JsonStrField(item, "mode"); !md.empty()) {
            env.mode = md;
        }
        env.desktop = detail::JsonStrField(item, "desktop");
        env.enter_mode = !detail::JsonStrField(item, "enter_mode").empty()
                             ? detail::JsonStrField(item, "enter_mode")
                             : detail::JsonStrField(item, "enter");
        env.persist = detail::JsonStrField(item, "persist");
        env.policy = detail::JsonStrField(item, "policy");
        env.proxy = detail::JsonStrField(item, "proxy");
        env.tz = detail::JsonStrField(item, "tz");
        env.runtime = detail::JsonStrField(item, "runtime");
        env.shell = detail::JsonStrField(item, "shell");
        env.user = detail::JsonStrField(item, "user");
        if (const auto np_it = item.find("no_password"); np_it != item.end() && np_it->is_boolean()) {
            env.no_password = np_it->get<bool>();
        }
        env.fetch = detail::JsonStrField(item, "fetch");
        env.observe = detail::JsonStrField(item, "observe");
        env.carrier = detail::JsonStrField(item, "carrier");
        env.xpkgs = detail::JsonStrListField(item, "xpkgs");
        env.allows = !detail::JsonStrListField(item, "allows").empty()
                         ? detail::JsonStrListField(item, "allows")
                         : detail::JsonStrListField(item, "allow");
        env.mounts = !detail::JsonStrListField(item, "mounts").empty()
                         ? detail::JsonStrListField(item, "mounts")
                         : detail::JsonStrListField(item, "mount");
        env.description = detail::JsonStrField(item, "description");
        res.spec.environments.push_back(std::move(env));
    }

    res.errors = ValidateLubanStackSpec(res.spec);
    res.ok = res.errors.empty();
    return res;
}

inline LubanStackParseResult ParseLubanStackToml(std::string_view text) {
    LubanStackParseResult res;
    res.detected_format = "toml";
    const std::string_view clean = detail::StripUtf8Bom(text);

    bool in_env_section = false;
    std::string pending_key;
    std::string pending_val;
    int bracket_balance = 0;
    std::size_t pending_start_line = 0;

    auto apply_kv = [&](std::size_t line_no, std::string_view key, std::string_view raw_val) {
        const std::string val = detail::TrimAscii(raw_val);
        std::string parse_err;

        if (!in_env_section) {
            auto str_val = detail::ParseTomlString(val, parse_err);
            if (!str_val.has_value()) {
                res.errors.push_back("第 " + std::to_string(line_no) + " 行顶层字段 '" + std::string(key) + "': " + parse_err);
                return;
            }
            if (key == "schema") res.spec.schema = *str_val;
            else if (key == "name") res.spec.name = *str_val;
            else if (key == "description") res.spec.description = *str_val;
            else {
                res.errors.push_back("第 " + std::to_string(line_no) + " 行：未知顶层字段 '" + std::string(key) + "'");
            }
            return;
        }

        if (res.spec.environments.empty()) {
            res.errors.push_back("第 " + std::to_string(line_no) + " 行：内部错误（缺少当前环境段落）");
            return;
        }
        LubanStackEnvSpec& cur = res.spec.environments.back();

        if (key == "no_password") {
            if (val == "true") cur.no_password = true;
            else if (val == "false") cur.no_password = false;
            else res.errors.push_back("第 " + std::to_string(line_no) + " 行 no_password 必须为 true 或 false");
            return;
        }
        if (key == "xpkgs" || key == "allows" || key == "allow" || key == "mounts" || key == "mount") {
            auto arr = detail::ParseTomlStringArray(val, parse_err);
            if (!arr.has_value()) {
                res.errors.push_back("第 " + std::to_string(line_no) + " 行数组字段 '" + std::string(key) + "': " + parse_err);
                return;
            }
            if (key == "xpkgs") cur.xpkgs = std::move(*arr);
            else if (key == "allows" || key == "allow") cur.allows = std::move(*arr);
            else cur.mounts = std::move(*arr);
            return;
        }

        auto str_val = detail::ParseTomlString(val, parse_err);
        if (!str_val.has_value()) {
            res.errors.push_back("第 " + std::to_string(line_no) + " 行字段 '" + std::string(key) + "': " + parse_err);
            return;
        }
        if (key == "name") cur.name = std::move(*str_val);
        else if (key == "edition") cur.edition = std::move(*str_val);
        else if (key == "mode") cur.mode = std::move(*str_val);
        else if (key == "desktop") cur.desktop = std::move(*str_val);
        else if (key == "enter_mode" || key == "enter") cur.enter_mode = std::move(*str_val);
        else if (key == "persist") cur.persist = std::move(*str_val);
        else if (key == "policy") cur.policy = std::move(*str_val);
        else if (key == "proxy") cur.proxy = std::move(*str_val);
        else if (key == "tz") cur.tz = std::move(*str_val);
        else if (key == "runtime") cur.runtime = std::move(*str_val);
        else if (key == "shell") cur.shell = std::move(*str_val);
        else if (key == "user") cur.user = std::move(*str_val);
        else if (key == "fetch") cur.fetch = std::move(*str_val);
        else if (key == "observe") cur.observe = std::move(*str_val);
        else if (key == "carrier") cur.carrier = std::move(*str_val);
        else if (key == "description") cur.description = std::move(*str_val);
        else {
            res.errors.push_back("第 " + std::to_string(line_no) + " 行：不支持的环境字段 '" + std::string(key) + "'");
        }
    };

    std::size_t line_start = 0;
    std::size_t line_no = 0;
    while (line_start <= clean.size()) {
        ++line_no;
        const std::size_t nl = clean.find('\n', line_start);
        std::string_view raw_line = (nl == std::string_view::npos)
                                        ? clean.substr(line_start)
                                        : clean.substr(line_start, nl - line_start);
        if (!raw_line.empty() && raw_line.back() == '\r') {
            raw_line.remove_suffix(1);
        }
        const std::string line = detail::TrimAscii(detail::StripTomlComment(raw_line));
        line_start = (nl == std::string_view::npos) ? (clean.size() + 1) : (nl + 1);

        if (line.empty()) continue;

        if (!pending_key.empty()) {
            pending_val.push_back(' ');
            pending_val += line;
            bracket_balance += detail::CountUnquotedBracketDelta(line);
            if (bracket_balance <= 0) {
                apply_kv(pending_start_line, pending_key, pending_val);
                pending_key.clear();
                pending_val.clear();
                bracket_balance = 0;
            }
            continue;
        }

        if (line.size() >= 4 && line.starts_with("[[") && line.ends_with("]]")) {
            const std::string table_name = detail::TrimAscii(std::string_view(line.data() + 2, line.size() - 4));
            if (table_name == "environments" || table_name == "subos" || table_name == "suboses") {
                in_env_section = true;
                res.spec.environments.emplace_back();
            } else {
                res.errors.push_back("第 " + std::to_string(line_no) + " 行：未知数组表头 [[" + table_name + "]]");
            }
            continue;
        }
        if (line.front() == '[') {
            res.errors.push_back("第 " + std::to_string(line_no) + " 行：不支持的段落表头 " + line + "（请使用 [[environments]]）");
            continue;
        }

        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            res.errors.push_back("第 " + std::to_string(line_no) + " 行缺少 '=' 赋值符");
            continue;
        }
        const std::string key = detail::TrimAscii(std::string_view(line).substr(0, eq));
        const std::string val = detail::TrimAscii(std::string_view(line).substr(eq + 1));
        if (key.empty()) {
            res.errors.push_back("第 " + std::to_string(line_no) + " 行缺少字段名");
            continue;
        }
        const int delta = detail::CountUnquotedBracketDelta(val);
        if (delta > 0) {
            pending_key = key;
            pending_val = val;
            bracket_balance = delta;
            pending_start_line = line_no;
            continue;
        }
        apply_kv(line_no, key, val);
    }

    if (!pending_key.empty()) {
        res.errors.push_back("第 " + std::to_string(pending_start_line) + " 行数组未闭合");
    }
    if (!res.errors.empty()) {
        return res;
    }

    res.errors = ValidateLubanStackSpec(res.spec);
    res.ok = res.errors.empty();
    return res;
}

inline LubanStackParseResult ParseLubanStackAuto(std::string_view text) {
    const std::string trimmed = detail::TrimAscii(detail::StripUtf8Bom(text));
    if (!trimmed.empty() && trimmed.front() == '{') {
        return ParseLubanStackJson(trimmed);
    }
    return ParseLubanStackToml(trimmed);
}

inline std::string SerializeLubanStackJson(const LubanStackSpec& spec) {
    nlohmann::json root;
    root["schema"] = spec.schema.empty() ? std::string("hxlings.luban-stack/v1") : spec.schema;
    root["name"] = spec.name;
    if (!spec.description.empty()) root["description"] = spec.description;

    nlohmann::json envs = nlohmann::json::array();
    for (const auto& env : spec.environments) {
        nlohmann::json obj;
        obj["name"] = env.name;
        if (!env.edition.empty()) obj["edition"] = env.edition;
        if (!env.mode.empty()) obj["mode"] = env.mode;
        if (!env.desktop.empty()) obj["desktop"] = env.desktop;
        if (!env.enter_mode.empty()) obj["enter_mode"] = env.enter_mode;
        if (!env.persist.empty()) obj["persist"] = env.persist;
        if (!env.policy.empty()) obj["policy"] = env.policy;
        if (!env.proxy.empty()) obj["proxy"] = env.proxy;
        if (!env.tz.empty()) obj["tz"] = env.tz;
        if (!env.runtime.empty()) obj["runtime"] = env.runtime;
        if (!env.shell.empty()) obj["shell"] = env.shell;
        if (!env.user.empty()) obj["user"] = env.user;
        if (env.no_password) obj["no_password"] = true;
        if (!env.fetch.empty()) obj["fetch"] = env.fetch;
        if (!env.observe.empty()) obj["observe"] = env.observe;
        if (!env.carrier.empty()) obj["carrier"] = env.carrier;
        if (!env.xpkgs.empty()) obj["xpkgs"] = env.xpkgs;
        if (!env.allows.empty()) obj["allows"] = env.allows;
        if (!env.mounts.empty()) obj["mounts"] = env.mounts;
        if (!env.description.empty()) obj["description"] = env.description;
        envs.push_back(std::move(obj));
    }
    root["environments"] = std::move(envs);
    return root.dump(2) + "\n";
}

inline std::string SerializeLubanStackToml(const LubanStackSpec& spec) {
    std::string out;
    out += "schema = " + detail::EscapeTomlString(spec.schema.empty() ? "hxlings.luban-stack/v1" : spec.schema) + "\n";
    out += "name = " + detail::EscapeTomlString(spec.name) + "\n";
    if (!spec.description.empty()) {
        out += "description = " + detail::EscapeTomlString(spec.description) + "\n";
    }
    for (const auto& env : spec.environments) {
        out += "\n[[environments]]\n";
        out += "name = " + detail::EscapeTomlString(env.name) + "\n";
        if (!env.edition.empty()) out += "edition = " + detail::EscapeTomlString(env.edition) + "\n";
        if (!env.mode.empty()) out += "mode = " + detail::EscapeTomlString(env.mode) + "\n";
        if (!env.desktop.empty()) out += "desktop = " + detail::EscapeTomlString(env.desktop) + "\n";
        if (!env.enter_mode.empty()) out += "enter_mode = " + detail::EscapeTomlString(env.enter_mode) + "\n";
        if (!env.persist.empty()) out += "persist = " + detail::EscapeTomlString(env.persist) + "\n";
        if (!env.policy.empty()) out += "policy = " + detail::EscapeTomlString(env.policy) + "\n";
        if (!env.proxy.empty()) out += "proxy = " + detail::EscapeTomlString(env.proxy) + "\n";
        if (!env.tz.empty()) out += "tz = " + detail::EscapeTomlString(env.tz) + "\n";
        if (!env.runtime.empty()) out += "runtime = " + detail::EscapeTomlString(env.runtime) + "\n";
        if (!env.shell.empty()) out += "shell = " + detail::EscapeTomlString(env.shell) + "\n";
        if (!env.user.empty()) out += "user = " + detail::EscapeTomlString(env.user) + "\n";
        if (env.no_password) out += "no_password = true\n";
        if (!env.fetch.empty()) out += "fetch = " + detail::EscapeTomlString(env.fetch) + "\n";
        if (!env.observe.empty()) out += "observe = " + detail::EscapeTomlString(env.observe) + "\n";
        if (!env.carrier.empty()) out += "carrier = " + detail::EscapeTomlString(env.carrier) + "\n";
        if (!env.xpkgs.empty()) out += "xpkgs = " + detail::EscapeTomlArray(env.xpkgs) + "\n";
        if (!env.allows.empty()) out += "allows = " + detail::EscapeTomlArray(env.allows) + "\n";
        if (!env.mounts.empty()) out += "mounts = " + detail::EscapeTomlArray(env.mounts) + "\n";
        if (!env.description.empty()) out += "description = " + detail::EscapeTomlString(env.description) + "\n";
    }
    return out;
}

inline LubanStackPlan BuildLubanStackPlan(
    const LubanStackSpec& spec,
    const std::vector<LubanEnvironment>& existing,
    const std::map<std::string, LubanEnvConfigDetail>& existing_configs = {}
) {
    LubanStackPlan plan;
    plan.stack_name = spec.name;

    auto find_env = [&](std::string_view name) -> const LubanEnvironment* {
        for (const auto& item : existing) {
            if (item.name == name) return &item;
        }
        return nullptr;
    };

    for (const auto& desired : spec.environments) {
        const LubanEnvironment* cur_env = find_env(desired.name);
        if (cur_env == nullptr) {
            std::string desc = "新建 " + desired.name + " (edition=" +
                               (desired.edition.empty() ? std::string("core") : desired.edition) +
                               ", mode=" + (desired.mode.empty() ? std::string("sandbox") : desired.mode) + ")";
            if (!desired.desktop.empty()) desc += " + desktop:" + desired.desktop;
            if (!desired.xpkgs.empty()) {
                desc += " + " + std::to_string(desired.xpkgs.size()) + " 个附加包";
            }
            plan.actions.push_back(LubanStackAction{
                .kind = LubanStackActionKind::Create,
                .env_name = desired.name,
                .summary = std::move(desc),
                .spec = desired,
            });
            ++plan.create_count;
            continue;
        }

        LubanConfigPatch patch;
        std::vector<std::string> diffs;
        const auto cfg_it = existing_configs.find(desired.name);
        if (cfg_it != existing_configs.end()) {
            const LubanEnvConfigDetail& cfg = cfg_it->second;
            if (!desired.mode.empty() && desired.mode != cfg.mode) {
                patch.mode = desired.mode;
                diffs.push_back("mode=" + desired.mode);
            }
            if (!desired.persist.empty() && desired.persist != cfg.persist) {
                patch.persist = desired.persist;
                diffs.push_back("persist=" + desired.persist);
            }
            if (!desired.desktop.empty() && desired.desktop != cfg.desktop) {
                patch.desktop = desired.desktop;
                diffs.push_back("desktop=" + desired.desktop);
            }
            if (!desired.shell.empty() && desired.shell != cfg.shell) {
                patch.shell = desired.shell;
                diffs.push_back("shell=" + desired.shell);
            }
            if (!desired.policy.empty() &&
                detail::NormalizePolicyComparable(desired.policy) !=
                    detail::NormalizePolicyComparable(cfg.policy_extends)) {
                patch.policy = desired.policy;
                diffs.push_back("policy=" + desired.policy);
            }
            if (!desired.proxy.empty() && desired.proxy != cfg.proxy) {
                patch.proxy = desired.proxy;
                diffs.push_back("proxy=" + desired.proxy);
            }
            if (!desired.fetch.empty() && desired.fetch != cfg.fetch_policy) {
                patch.fetch = desired.fetch;
                diffs.push_back("fetch=" + desired.fetch);
            }
            if (!desired.observe.empty() && desired.observe != cfg.observe_level) {
                patch.observe = desired.observe;
                diffs.push_back("observe=" + desired.observe);
            }
            if (!desired.enter_mode.empty()) {
                patch.enter_mode = desired.enter_mode;
                diffs.push_back("enter=" + desired.enter_mode);
            }
            if (!desired.tz.empty()) {
                patch.tz = desired.tz;
                diffs.push_back("tz=" + desired.tz);
            }
            if (!desired.runtime.empty()) {
                patch.runtime = desired.runtime;
                diffs.push_back("runtime=" + desired.runtime);
            }
            for (const auto& grant : desired.allows) {
                if (!grant.empty() && !detail::ContainsString(cfg.grants, grant)) {
                    patch.allow.push_back(grant);
                    diffs.push_back("+allow:" + grant);
                }
            }
            for (const auto& m : desired.mounts) {
                if (!m.empty() && !detail::ContainsString(cfg.mounts, m)) {
                    patch.mounts.push_back(m);
                    diffs.push_back("+mount:" + m);
                }
            }
        } else {
            if (!desired.mode.empty()) {
                patch.mode = desired.mode;
                diffs.push_back("mode=" + desired.mode);
            }
            if (!desired.persist.empty()) {
                patch.persist = desired.persist;
                diffs.push_back("persist=" + desired.persist);
            }
            if (!desired.desktop.empty()) {
                patch.desktop = desired.desktop;
                diffs.push_back("desktop=" + desired.desktop);
            }
            if (!desired.policy.empty()) {
                patch.policy = desired.policy;
                diffs.push_back("policy=" + desired.policy);
            }
            if (!desired.proxy.empty()) {
                patch.proxy = desired.proxy;
                diffs.push_back("proxy=" + desired.proxy);
            }
            if (!desired.fetch.empty()) {
                patch.fetch = desired.fetch;
                diffs.push_back("fetch=" + desired.fetch);
            }
            if (!desired.observe.empty()) {
                patch.observe = desired.observe;
                diffs.push_back("observe=" + desired.observe);
            }
            if (!desired.enter_mode.empty()) {
                patch.enter_mode = desired.enter_mode;
                diffs.push_back("enter=" + desired.enter_mode);
            }
            if (!desired.tz.empty()) {
                patch.tz = desired.tz;
                diffs.push_back("tz=" + desired.tz);
            }
            if (!desired.runtime.empty()) {
                patch.runtime = desired.runtime;
                diffs.push_back("runtime=" + desired.runtime);
            }
            if (!desired.shell.empty()) {
                patch.shell = desired.shell;
                diffs.push_back("shell=" + desired.shell);
            }
            for (const auto& g : desired.allows) {
                if (!g.empty()) {
                    patch.allow.push_back(g);
                    diffs.push_back("+allow:" + g);
                }
            }
            for (const auto& m : desired.mounts) {
                if (!m.empty()) {
                    patch.mounts.push_back(m);
                    diffs.push_back("+mount:" + m);
                }
            }
        }

        if (patch.Empty()) {
            plan.actions.push_back(LubanStackAction{
                .kind = LubanStackActionKind::UpToDate,
                .env_name = desired.name,
                .summary = "已就绪且配置一致，无需变更",
                .spec = desired,
            });
            ++plan.unchanged_count;
        } else {
            std::string sum = "更新配置 ";
            for (std::size_t idx = 0; idx < diffs.size(); ++idx) {
                if (idx > 0) sum += ", ";
                sum += diffs[idx];
            }
            plan.actions.push_back(LubanStackAction{
                .kind = LubanStackActionKind::Configure,
                .env_name = desired.name,
                .summary = std::move(sum),
                .spec = desired,
                .config_patch = std::move(patch),
            });
            ++plan.configure_count;
        }
    }

    return plan;
}

inline LubanStackSpec ExportEnvironmentsToStack(
    std::string stack_name,
    const std::vector<LubanEnvironment>& envs,
    const std::map<std::string, LubanEnvConfigDetail>& configs = {}
) {
    LubanStackSpec spec;
    spec.name = stack_name.empty() ? std::string("exported-luban-stack") : std::move(stack_name);
    spec.description = "从本机 Luban 环境快照导出的声明式编排清单";

    for (const auto& e : envs) {
        if (e.name.empty() || e.name == "default" || e.name == "current") continue;
        LubanStackEnvSpec item;
        item.name = e.name;
        item.edition = e.edition.empty() ? std::string("core") : e.edition;
        item.shell = e.shell;
        item.carrier = e.carrier;
        if (!e.desktops.empty()) item.desktop = e.desktops.front();

        if (const auto cfg_it = configs.find(e.name); cfg_it != configs.end()) {
            const LubanEnvConfigDetail& c = cfg_it->second;
            if (!c.mode.empty()) item.mode = c.mode;
            if (!c.persist.empty()) item.persist = c.persist;
            if (!c.desktop.empty()) item.desktop = c.desktop;
            if (!c.shell.empty()) item.shell = c.shell;
            if (!c.policy_extends.empty() && c.policy_extends != "legacy") {
                item.policy = c.policy_extends;
            }
            item.proxy = c.proxy;
            item.fetch = c.fetch_policy;
            item.observe = c.observe_level;
            item.allows = c.grants;
            item.mounts = c.mounts;
        }
        spec.environments.push_back(std::move(item));
    }
    return spec;
}

inline std::vector<LubanStackSpec> BuiltinLubanStacks() {
    return {
        LubanStackSpec{
            .name = "cpp23-dev-matrix",
            .description = "C++23 工具链开发盒 + 轻量一次性测试沙箱",
            .environments = {
                LubanStackEnvSpec{
                    .name = "cpp23-core",
                    .edition = "core",
                    .mode = "sandbox",
                    .persist = "all",
                    .policy = "dev",
                    .xpkgs = {"xim:llvm", "xim:cmake"},
                    .description = "主 C++23 编译与调试环境",
                },
                LubanStackEnvSpec{
                    .name = "tiny-test",
                    .edition = "tiny-musl",
                    .mode = "sandbox",
                    .persist = "none",
                    .description = "无状态 musl 验证容器（退出即清理）",
                },
            },
        },
        LubanStackSpec{
            .name = "agent-isolated-workspace",
            .description = "日常开发核心 + 强代理 AI Agent 隔离工作区",
            .environments = {
                LubanStackEnvSpec{
                    .name = "dev-core",
                    .edition = "core",
                    .mode = "sandbox",
                    .policy = "dev",
                    .description = "日常命令行开发盒",
                },
                LubanStackEnvSpec{
                    .name = "agent-box",
                    .edition = "agent-workspace",
                    .mode = "sandbox",
                    .policy = "xim:agent-private",
                    .proxy = "socks5h://127.0.0.1:7897",
                    .fetch = "ask",
                    .observe = "standard",
                    .description = "代理隔离且独立身份的 AI Agent 沙箱",
                },
            },
        },
        LubanStackSpec{
            .name = "desktop-plus-sandbox",
            .description = "xdesk 图形桌面系统 + 轻量命令行开发盒",
            .environments = {
                LubanStackEnvSpec{
                    .name = "luban-desk",
                    .edition = "desktop",
                    .mode = "sandbox",
                    .desktop = "xdesk",
                    .enter_mode = "desktop",
                    .persist = "all",
                    .no_password = true,
                    .allows = {"gpu", "clipboard", "audio"},
                    .description = "xdesk 桌面工作环境",
                },
                LubanStackEnvSpec{
                    .name = "cli-lab",
                    .edition = "core",
                    .mode = "sandbox",
                    .enter_mode = "console",
                    .description = "配套终端实验环境",
                },
            },
        },
    };
}

inline std::filesystem::path ResolveLubanStacksDir() {
    namespace os = mcpplibs::os;
    if (const auto cfg = os::dirs::config(); cfg.has_value() && !cfg->empty()) {
        return *cfg / "hxlings" / "luban-stacks";
    }
    return os::dirs::temp() / "hxlings-luban-stacks";
}

inline std::vector<SavedLubanStackInfo> ListSavedLubanStacks(const std::filesystem::path& dir = ResolveLubanStacksDir()) {
    namespace os = mcpplibs::os;
    std::vector<SavedLubanStackInfo> out;
    std::error_code ec;
    if (dir.empty() || !std::filesystem::is_directory(dir, ec)) return out;

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const std::string ext = entry.path().extension().string();
        if (ext != ".json" && ext != ".toml") continue;

        auto text_res = os::fs::read(entry.path());
        if (!text_res.has_value()) continue;
        const LubanStackParseResult parsed = ParseLubanStackAuto(*text_res);

        SavedLubanStackInfo info{
            .id = entry.path().stem().string(),
            .filename = entry.path().filename().string(),
            .format = (ext == ".toml") ? "toml" : "json",
            .path = entry.path(),
            .display_name = parsed.ok && !parsed.spec.name.empty()
                                ? parsed.spec.name
                                : entry.path().stem().string(),
            .description = parsed.ok ? parsed.spec.description : std::string("格式待修正"),
            .env_count = parsed.spec.environments.size(),
        };
        out.push_back(std::move(info));
    }

    std::ranges::sort(out, [](const SavedLubanStackInfo& a, const SavedLubanStackInfo& b) {
        return a.filename < b.filename;
    });
    return out;
}

inline std::optional<std::string> LoadLubanStackFile(const std::filesystem::path& path) {
    auto res = mcpplibs::os::fs::read(path);
    if (!res.has_value()) return std::nullopt;
    return *res;
}

inline std::optional<std::filesystem::path> SaveLubanStackFile(
    std::string_view name,
    std::string_view content,
    std::string_view format = "json",
    const std::filesystem::path& dir = ResolveLubanStacksDir()
) {
    namespace os = mcpplibs::os;
    if (dir.empty()) return std::nullopt;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec && !std::filesystem::is_directory(dir, ec)) return std::nullopt;

    const std::string stem = detail::SanitizeStackFileStem(name);
    const std::string ext = (format == "toml") ? ".toml" : ".json";
    const std::filesystem::path target = dir / (stem + ext);
    if (auto res = os::fs::write_atomic(target, content); !res.has_value()) {
        return std::nullopt;
    }
    return target;
}

inline bool DeleteLubanStackFile(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::remove(path, ec);
}

} // namespace hxlings

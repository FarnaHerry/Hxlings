export module xlings.parser.common;

import std;
import nlohmann.json;
import xlings.types;

export namespace hxlings::parser_detail {

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

bool ParseAsciiDigits(std::string_view sv, std::size_t pos, std::size_t len, int& out) {
    if (pos + len > sv.size()) return false;
    int val = 0;
    for (std::size_t i = 0; i < len; ++i) {
        const unsigned char ch = static_cast<unsigned char>(sv[pos + i]);
        if (ch < '0' || ch > '9') return false;
        val = val * 10 + static_cast<int>(ch - '0');
    }
    out = val;
    return true;
}

std::optional<std::int64_t> TryParseIsoTimestamp(std::string_view sv, bool& has_explicit_zone) {
    has_explicit_zone = false;
    if (sv.size() < 19) return std::nullopt;
    if (sv[4] != '-' || sv[7] != '-' || (sv[10] != 'T' && sv[10] != 't' && sv[10] != ' ') ||
        sv[13] != ':' || sv[16] != ':') {
        return std::nullopt;
    }

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (!ParseAsciiDigits(sv, 0, 4, year) || !ParseAsciiDigits(sv, 5, 2, month) ||
        !ParseAsciiDigits(sv, 8, 2, day) || !ParseAsciiDigits(sv, 11, 2, hour) ||
        !ParseAsciiDigits(sv, 14, 2, minute) || !ParseAsciiDigits(sv, 17, 2, second)) {
        return std::nullopt;
    }
    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        return std::nullopt;
    }

    const std::chrono::year_month_day ymd{
        std::chrono::year{year},
        std::chrono::month{static_cast<unsigned>(month)},
        std::chrono::day{static_cast<unsigned>(day)},
    };
    if (!ymd.ok()) return std::nullopt;

    std::size_t pos = 19;
    if (pos < sv.size() && (sv[pos] == '.' || sv[pos] == ',')) {
        ++pos;
        std::size_t frac_digits = 0;
        while (pos < sv.size() && sv[pos] >= '0' && sv[pos] <= '9') {
            ++pos;
            ++frac_digits;
        }
        if (frac_digits == 0 || frac_digits > 9) return std::nullopt;
    }

    int offset_minutes = 0;
    if (pos < sv.size()) {
        const char tz = sv[pos];
        if (tz == 'Z' || tz == 'z') {
            has_explicit_zone = true;
            ++pos;
        } else if (tz == '+' || tz == '-') {
            const int sign = (tz == '+') ? 1 : -1;
            ++pos;
            int tz_h = 0, tz_m = 0;
            if (!ParseAsciiDigits(sv, pos, 2, tz_h) || tz_h > 23) return std::nullopt;
            pos += 2;
            if (pos < sv.size()) {
                if (sv[pos] == ':') ++pos;
                if (!ParseAsciiDigits(sv, pos, 2, tz_m) || tz_m > 59) return std::nullopt;
                pos += 2;
            }
            offset_minutes = sign * (tz_h * 60 + tz_m);
            has_explicit_zone = true;
        } else {
            return std::nullopt;
        }
    }
    if (pos != sv.size()) return std::nullopt;

    const std::int64_t days = std::chrono::sys_days{ymd}.time_since_epoch().count();
    const std::int64_t base_sec = days * 86400LL +
                                  static_cast<std::int64_t>(hour) * 3600LL +
                                  static_cast<std::int64_t>(minute) * 60LL +
                                  static_cast<std::int64_t>(second);
    return base_sec - static_cast<std::int64_t>(offset_minutes) * 60LL;
}

std::size_t MatchIsoTimestampPrefixEnd(std::string_view sv, std::size_t start) {
    if (start + 20 > sv.size()) return 0;
    if (sv[start + 4] != '-' || sv[start + 7] != '-' ||
        (sv[start + 10] != 'T' && sv[start + 10] != 't' && sv[start + 10] != ' ') ||
        sv[start + 13] != ':' || sv[start + 16] != ':') {
        return 0;
    }
    std::size_t pos = start + 19;
    if (pos < sv.size() && (sv[pos] == '.' || sv[pos] == ',')) {
        ++pos;
        std::size_t frac = 0;
        while (pos < sv.size() && sv[pos] >= '0' && sv[pos] <= '9') {
            ++pos;
            ++frac;
        }
        if (frac == 0 || frac > 9) return 0;
    }
    if (pos >= sv.size()) return 0;
    if (sv[pos] == 'Z' || sv[pos] == 'z') {
        ++pos;
        return pos;
    }
    if (sv[pos] == '+' || sv[pos] == '-') {
        ++pos;
        if (pos + 2 > sv.size()) return 0;
        pos += 2;
        if (pos < sv.size() && sv[pos] == ':') ++pos;
        if (pos + 2 <= sv.size() && sv[pos] >= '0' && sv[pos] <= '9' &&
            sv[pos + 1] >= '0' && sv[pos + 1] <= '9') {
            pos += 2;
        }
        return pos;
    }
    return 0;
}

} // namespace hxlings::parser_detail


export namespace hxlings {

std::string TrimWhitespace(std::string_view sv) {
    return parser_detail::TrimCopy(sv);
}

int CompareVersionStrings(std::string_view a, std::string_view b) {
    std::size_t pa = 0;
    std::size_t pb = 0;
    while (pa < a.size() || pb < b.size()) {
        const std::string_view sa = parser_detail::NextVersionSegment(a, pa);
        const std::string_view sb = parser_detail::NextVersionSegment(b, pb);
        if (sa.empty() && sb.empty()) break;
        if (sa.empty()) return -1;
        if (sb.empty()) return 1;

        if (parser_detail::IsAllAsciiDigits(sa) && parser_detail::IsAllAsciiDigits(sb)) {
            const std::string_view ta = parser_detail::StripLeadingZeros(sa);
            const std::string_view tb = parser_detail::StripLeadingZeros(sb);
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
    const std::string trimmed = parser_detail::TrimCopy(spec);
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
    ev.raw_line = parser_detail::TrimCopy(line);
    if (ev.raw_line.empty()) {
        ev.kind = "raw";
        return ev;
    }

    const auto parsed = parser_detail::TryParseJsonObject(ev.raw_line);
    if (!parsed) {
        ev.kind = "raw";
        ev.message = ev.raw_line;
        return ev;
    }

    const auto& doc = *parsed;
    ev.kind = parser_detail::JsonString(doc, "kind", "raw");
    ev.data_kind = parser_detail::JsonString(doc, "dataKind");
    ev.level = parser_detail::JsonString(doc, "level");
    if (doc.contains("exitCode") && doc["exitCode"].is_number_integer()) {
        ev.exit_code = doc["exitCode"].get<int>();
    }
    ev.progress_percent = parser_detail::ExtractProgressPercent(doc);

    if (ev.kind == "log") {
        ev.message = parser_detail::JsonString(doc, "message");
        if (ev.message.empty()) ev.message = parser_detail::JsonString(doc, "text");
        if (ev.message.empty()) ev.message = parser_detail::JsonString(doc, "line");
        if (ev.message.empty() && doc.contains("payload") && doc["payload"].is_object()) {
            ev.message = parser_detail::JsonString(doc["payload"], "message");
        }
    } else if (ev.kind == "error") {
        ev.message = parser_detail::JsonString(doc, "message");
        if (ev.message.empty()) ev.message = parser_detail::JsonString(doc, "error");
        if (ev.message.empty() && doc.contains("payload") && doc["payload"].is_object()) {
            ev.message = parser_detail::JsonString(doc["payload"], "message");
        }
        if (ev.message.empty()) ev.message = ev.raw_line;
    } else if (ev.kind == "progress") {
        ev.message = parser_detail::JsonString(doc, "message");
        if (ev.message.empty()) ev.message = parser_detail::JsonString(doc, "phase");
        if (ev.message.empty() && doc.contains("payload") && doc["payload"].is_object()) {
            const auto& pl = doc["payload"];
            ev.message = parser_detail::JsonString(pl, "message");
            if (ev.message.empty()) ev.message = parser_detail::JsonString(pl, "phase");
            if (ev.message.empty()) ev.message = parser_detail::JsonString(pl, "step");
        }
        if (ev.message.empty() && ev.progress_percent) {
            ev.message = std::to_string(static_cast<int>(*ev.progress_percent * 100.0F)) + "%";
        }
    } else if (ev.kind == "data" && ev.data_kind == "subos_exec_output") {
        if (doc.contains("payload") && doc["payload"].is_object()) {
            const auto& pl = doc["payload"];
            ev.message = parser_detail::JsonString(pl, "line");
            if (ev.message.empty()) ev.message = parser_detail::JsonString(pl, "data");
            if (ev.message.empty()) ev.message = parser_detail::JsonString(pl, "text");
            ev.level = parser_detail::JsonString(pl, "stream");
        }
    } else if (ev.kind == "data" && ev.data_kind == "tip") {
        if (doc.contains("payload") && doc["payload"].is_object()) {
            ev.message = parser_detail::JsonString(doc["payload"], "message");
        }
    } else if (ev.kind == "result") {
        if (doc.contains("data") && doc["data"].is_object()) {
            ev.message = parser_detail::JsonString(doc["data"], "message");
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
        const std::string trimmed = parser_detail::TrimCopy(output.substr(start, end - start));
        if (!trimmed.empty()) {
            events.push_back(ParseNdjsonLine(trimmed));
        }
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return events;
}

std::optional<std::int64_t> ParseTimestampToEpochSeconds(std::string_view raw) {
    const std::string trimmed = parser_detail::TrimCopy(raw);
    if (trimmed.empty()) return std::nullopt;

    bool has_zone = false;
    if (const auto iso = parser_detail::TryParseIsoTimestamp(trimmed, has_zone); iso.has_value() && has_zone) {
        return iso;
    }

    if (trimmed.size() == 10 || trimmed.size() == 13 || trimmed.size() == 16 || trimmed.size() == 19) {
        for (const unsigned char ch : trimmed) {
            if (ch < '0' || ch > '9') return std::nullopt;
        }
        std::int64_t val = 0;
        const auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), val);
        if (ec != std::errc{} || ptr != trimmed.data() + trimmed.size()) return std::nullopt;
        if (trimmed.size() == 10) return val;
        if (trimmed.size() == 13) return val / 1000LL;
        if (trimmed.size() == 16) return val / 1000000LL;
        return val / 1000000000LL;
    }
    return std::nullopt;
}

bool IsIsoTimestampWithZone(std::string_view raw) {
    const std::string trimmed = parser_detail::TrimCopy(raw);
    bool has_zone = false;
    return parser_detail::TryParseIsoTimestamp(trimmed, has_zone).has_value() && has_zone;
}

std::string FormatEpochWithOffsetMinutes(std::int64_t epoch_seconds, int offset_minutes) {
    const std::int64_t shifted = epoch_seconds + static_cast<std::int64_t>(offset_minutes) * 60LL;
    std::int64_t day_count = shifted / 86400LL;
    std::int64_t rem = shifted % 86400LL;
    if (rem < 0) {
        rem += 86400LL;
        --day_count;
    }
    const std::chrono::year_month_day ymd{std::chrono::sys_days{std::chrono::days{day_count}}};
    const int hh = static_cast<int>(rem / 3600LL);
    const int mm = static_cast<int>((rem % 3600LL) / 60LL);
    const int ss = static_cast<int>(rem % 60LL);
    return std::format(
        "{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
        static_cast<int>(ymd.year()),
        static_cast<unsigned>(ymd.month()),
        static_cast<unsigned>(ymd.day()),
        hh,
        mm,
        ss
    );
}

std::string FormatLocalTimestamp(std::string_view raw) {
    const std::string trimmed = parser_detail::TrimCopy(raw);
    const auto epoch = ParseTimestampToEpochSeconds(trimmed);
    if (!epoch.has_value()) return trimmed;

    const std::time_t tt = static_cast<std::time_t>(*epoch);
    static std::mutex localtime_mu;
    std::tm local_tm{};
    {
        std::lock_guard lock(localtime_mu);
        if (const std::tm* ptm = std::localtime(&tt)) {
            local_tm = *ptm;
        } else {
            return FormatEpochWithOffsetMinutes(*epoch, 0);
        }
    }
    return std::format(
        "{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
        local_tm.tm_year + 1900,
        local_tm.tm_mon + 1,
        local_tm.tm_mday,
        local_tm.tm_hour,
        local_tm.tm_min,
        local_tm.tm_sec
    );
}

std::string FormatTimestampsInText(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        const std::size_t end_pos = parser_detail::MatchIsoTimestampPrefixEnd(text, i);
        if (end_pos > i) {
            const std::string_view candidate = text.substr(i, end_pos - i);
            if (IsIsoTimestampWithZone(candidate)) {
                out += FormatLocalTimestamp(candidate);
                i = end_pos;
                continue;
            }
        }
        out.push_back(text[i]);
        ++i;
    }
    return out;
}

} // namespace hxlings

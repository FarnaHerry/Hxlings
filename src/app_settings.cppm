export module hxlings.settings;

import std;
import mcpplibs.os;
import nlohmann.json;

export namespace hxlings {

struct AppSettings {
    bool dark_mode = false;
    int brand_preset_index = 0;
    bool tray_enabled = true;
    bool close_to_tray = true;

    bool operator==(const AppSettings&) const = default;
};

inline std::filesystem::path DefaultAppSettingsPath() {
    namespace os = mcpplibs::os;
    if (const auto cfg = os::dirs::config(); cfg.has_value() && !cfg->empty()) {
        return *cfg / "hxlings" / "settings.json";
    }
    if (const auto home = os::dirs::home(); home.has_value() && !home->empty()) {
        return *home / ".config" / "hxlings" / "settings.json";
    }
    return os::dirs::temp() / "hxlings" / "settings.json";
}

inline AppSettings ParseAppSettingsJson(std::string_view text) {
    AppSettings out;
    const auto parsed = nlohmann::json::parse(text.begin(), text.end(), nullptr, false);
    if (!parsed.is_object()) {
        return out;
    }
    if (parsed.contains("dark_mode") && parsed["dark_mode"].is_boolean()) {
        out.dark_mode = parsed["dark_mode"].get<bool>();
    }
    if (parsed.contains("brand_preset_index") && parsed["brand_preset_index"].is_number_integer()) {
        out.brand_preset_index = std::max(0, parsed["brand_preset_index"].get<int>());
    }
    if (parsed.contains("tray_enabled") && parsed["tray_enabled"].is_boolean()) {
        out.tray_enabled = parsed["tray_enabled"].get<bool>();
    }
    if (parsed.contains("close_to_tray") && parsed["close_to_tray"].is_boolean()) {
        out.close_to_tray = parsed["close_to_tray"].get<bool>();
    }
    return out;
}

inline std::string SerializeAppSettingsJson(const AppSettings& settings) {
    nlohmann::json obj;
    obj["dark_mode"] = settings.dark_mode;
    obj["brand_preset_index"] = std::max(0, settings.brand_preset_index);
    obj["tray_enabled"] = settings.tray_enabled;
    obj["close_to_tray"] = settings.close_to_tray;
    return obj.dump(2) + "\n";
}

inline AppSettings LoadAppSettings(const std::filesystem::path& path = DefaultAppSettingsPath()) {
    const auto bytes = mcpplibs::os::fs::read(path);
    if (!bytes.has_value()) {
        return AppSettings{};
    }
    return ParseAppSettingsJson(*bytes);
}

inline bool SaveAppSettings(const AppSettings& settings,
                            const std::filesystem::path& path = DefaultAppSettingsPath()) {
    std::error_code ec;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }
    const auto res = mcpplibs::os::fs::write_atomic(path, SerializeAppSettingsJson(settings));
    return res.has_value();
}

} // namespace hxlings

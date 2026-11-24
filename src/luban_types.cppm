export module hxlings.luban.types;

import std;

export namespace hxlings {

struct LubanGateStatus {
    std::string gate;
    std::string enforced;
    std::string reason;
    bool supported = false;

    bool operator==(const LubanGateStatus&) const = default;
};

struct LubanModeStatus {
    std::string name;
    std::string how;
    std::string reason;
    std::string route;
    bool ready = false;

    bool operator==(const LubanModeStatus&) const = default;
};

struct LubanToolStatus {
    std::string name;
    std::string package;
    std::string install_cmd;
    std::string path;
    bool available = false;

    bool operator==(const LubanToolStatus&) const = default;
};

struct LubanHostStatus {
    bool ready = false;
    std::string os;
    std::string arch;
    std::string backend_name;
    std::string backend_path;
    std::string backend_source;
    std::string xlings_version;
    std::string luban_version;
    std::string luban_os_version;
    std::string protocol_version;
    std::string index_commit;
    bool sandbox_ready = false;
    bool machine_ready = false;
    bool desktop_ready = false;
    std::string desktop_reason;
    std::string desktop_route;
    std::vector<LubanGateStatus> gates;
    std::vector<LubanModeStatus> modes;
    std::vector<LubanToolStatus> missing_tools;
    std::vector<std::string> running_envs;
    std::string raw_error;

    bool operator==(const LubanHostStatus&) const = default;
};

struct LubanEnvironment {
    std::string name;
    std::string kind;
    std::string edition;
    std::string carrier;
    std::string shell;
    std::vector<std::string> desktops;
    int packages = 0;
    bool running = false;

    bool operator==(const LubanEnvironment&) const = default;
};

struct LubanEnvStatusDetail {
    std::string name;
    std::string kind;
    std::string edition;
    std::string carrier;
    std::string mode;
    std::string persist;
    std::string enter;
    std::string network;
    std::string shell;
    std::string desktop;
    std::vector<std::string> desktops;
    std::vector<std::string> needs;
    std::vector<std::string> notes;
    std::vector<std::string> unmet;
    int packages = 0;
    bool running = false;

    bool operator==(const LubanEnvStatusDetail&) const = default;
};

struct LubanEnvConfigDetail {
    std::string name;
    std::string mode;
    std::string persist;
    std::string shell;
    std::string desktop;
    std::string on_close;
    std::string files;
    std::string kernel;
    std::string policy_extends;
    std::string policy_source;
    std::string net;
    std::string proxy;
    std::string observe_level;
    std::string fetch_policy;
    std::string index_update_policy;
    bool no_degrade = false;
    std::vector<std::string> grants;
    std::vector<std::string> grants_allowed;
    std::vector<std::string> env_pass;
    std::vector<std::string> mounts;

    bool operator==(const LubanEnvConfigDetail&) const = default;
};

struct LubanCatalogItem {
    std::string group;
    std::string name;
    std::string ref;
    std::string version;
    std::string status;
    std::string source;
    std::string description;
    std::string title_zh;
    std::string summary_zh;
    std::string gpu_hint;
    std::vector<std::string> highlights;
    int tier = 0;
    bool available = true;

    bool operator==(const LubanCatalogItem&) const = default;
};

struct LubanCatalog {
    std::vector<LubanCatalogItem> distributions;
    std::vector<LubanCatalogItem> desktops;

    bool operator==(const LubanCatalog&) const = default;
};

struct LubanCreateOptions {
    std::string name;
    std::string edition;
    std::string desktop;
    std::string enter_mode;
    std::string mode;
    std::string persist;
    std::string shell;
    std::string login;
    std::string on_close;
    std::string policy;
    std::string tz;
    std::string runtime;
    std::string user;
    std::string proxy;
    std::string carrier;
    bool no_password = false;
    bool dry_run = false;
    std::vector<std::string> xpkgs;
    std::vector<std::string> allows;

    bool operator==(const LubanCreateOptions&) const = default;
};

struct LubanConfigPatch {
    std::optional<std::string> mode;
    std::optional<std::string> persist;
    std::optional<std::string> shell;
    std::optional<std::string> login;
    std::optional<std::string> runtime;
    std::optional<std::string> desktop;
    std::optional<std::string> on_close;
    std::optional<std::string> enter_mode;
    std::optional<std::string> files;
    std::optional<std::string> kernel;
    std::optional<std::string> policy;
    std::optional<std::string> proxy;
    std::optional<std::string> tz;
    std::optional<std::string> fetch;
    std::optional<std::string> observe;
    std::vector<std::string> allow;
    std::vector<std::string> disallow;
    std::vector<std::string> mounts;
    std::vector<std::string> unmounts;

    [[nodiscard]] bool Empty() const noexcept {
        return !mode.has_value() && !persist.has_value() && !shell.has_value() &&
               !login.has_value() && !runtime.has_value() && !desktop.has_value() &&
               !on_close.has_value() && !enter_mode.has_value() && !files.has_value() &&
               !kernel.has_value() && !policy.has_value() && !proxy.has_value() &&
               !tz.has_value() && !fetch.has_value() && !observe.has_value() &&
               allow.empty() && disallow.empty() && mounts.empty() && unmounts.empty();
    }

    bool operator==(const LubanConfigPatch&) const = default;
};

struct LubanExportOptions {
    std::string name;
    std::string file_path;
    std::string format;
    std::string size;
    std::string boot;

    bool operator==(const LubanExportOptions&) const = default;
};

struct LubanStackEnvSpec {
    std::string name;
    std::string edition = "core";
    std::string mode = "sandbox";
    std::string desktop;
    std::string enter_mode;
    std::string persist;
    std::string policy;
    std::string proxy;
    std::string tz;
    std::string runtime;
    std::string shell;
    std::string user;
    bool no_password = false;
    std::string fetch;
    std::string observe;
    std::string carrier;
    std::vector<std::string> xpkgs;
    std::vector<std::string> allows;
    std::vector<std::string> mounts;
    std::string description;

    bool operator==(const LubanStackEnvSpec&) const = default;
};

struct LubanStackSpec {
    std::string schema = "hxlings.luban-stack/v1";
    std::string name;
    std::string description;
    std::vector<LubanStackEnvSpec> environments;

    bool operator==(const LubanStackSpec&) const = default;
};

enum class LubanStackActionKind {
    Create,
    Configure,
    UpToDate,
};

struct LubanStackAction {
    LubanStackActionKind kind = LubanStackActionKind::Create;
    std::string env_name;
    std::string summary;
    LubanStackEnvSpec spec;
    LubanConfigPatch config_patch;

    bool operator==(const LubanStackAction&) const = default;
};

struct LubanStackPlan {
    std::string stack_name;
    std::vector<LubanStackAction> actions;
    std::size_t create_count = 0;
    std::size_t configure_count = 0;
    std::size_t unchanged_count = 0;

    bool operator==(const LubanStackPlan&) const = default;
};

struct LubanStackParseResult {
    bool ok = false;
    std::string detected_format;
    LubanStackSpec spec;
    std::vector<std::string> errors;

    bool operator==(const LubanStackParseResult&) const = default;
};

struct SavedLubanStackInfo {
    std::string id;
    std::string filename;
    std::string format;
    std::filesystem::path path;
    std::string display_name;
    std::string description;
    std::size_t env_count = 0;

    bool operator==(const SavedLubanStackInfo&) const = default;
};

} // namespace hxlings

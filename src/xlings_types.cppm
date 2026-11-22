export module xlings.types;

import std;

export namespace hxlings {

struct PackageSpecParts {
    std::string ns;
    std::string name;
    std::string version;

    bool operator==(const PackageSpecParts&) const = default;
};

struct XlingsVersionInfo {
    std::string version;
    std::string protocol_version;
    std::vector<std::string> capabilities;
    std::vector<std::string> features;

    bool operator==(const XlingsVersionInfo&) const = default;
};

struct XlingsEnv {
    std::string xlings_home;
    std::string data_dir;
    std::string subos_dir;
    std::string bin_dir;
    std::string lib_dir;
    std::string active_subos;
    std::string mirror;
    std::string lang;
    std::string binary_path;

    bool operator==(const XlingsEnv&) const = default;
};

struct KeyValueField {
    std::string label;
    std::string value;
    bool highlight = false;

    bool operator==(const KeyValueField&) const = default;
};

struct BindingFinding {
    std::string code;
    std::string severity;
    std::string subos;
    std::string target;
    std::string version;
    std::string summary;
    std::string hint;

    bool operator==(const BindingFinding&) const = default;
};

struct SystemStatus {
    std::string title;
    std::vector<KeyValueField> fields;
    std::vector<BindingFinding> findings;

    bool operator==(const SystemStatus&) const = default;
};

struct InstalledPkg {
    std::string raw_spec;
    std::string ns;
    std::string name;
    std::string version;
    std::string description;
    std::string status;
    bool is_active = false;
    bool is_degraded = false;

    bool operator==(const InstalledPkg&) const = default;
};

struct PkgGroup {
    std::string key;
    std::string ns;
    std::string name;
    std::string description;
    std::string active_version;
    bool has_degraded = false;
    std::vector<InstalledPkg> versions;

    bool operator==(const PkgGroup&) const = default;
};

struct InstalledVersionItem {
    std::string version;
    std::string path;
    bool active = false;

    bool operator==(const InstalledVersionItem&) const = default;
};

struct SearchPkg {
    std::string package;
    std::string ns;
    std::string name;
    std::string version;
    std::string description;
    std::string status;
    std::string index;
    std::vector<std::string> categories;
    std::vector<std::string> keywords;
    std::vector<std::string> archs;

    bool operator==(const SearchPkg&) const = default;
};

struct PackageDetail {
    std::string title;
    std::string package;
    std::string ns;
    std::string name;
    std::string description;
    std::string status;
    std::string selected_version;
    std::vector<std::string> available_versions;
    std::vector<std::string> categories;
    std::vector<std::string> archs;
    std::vector<KeyValueField> fields;

    bool operator==(const PackageDetail&) const = default;
};

struct InstallPlanItem {
    std::string spec;
    std::string note;
    int action_code = 0;

    bool operator==(const InstallPlanItem&) const = default;
};

struct SubosEntry {
    std::string name;
    std::string dir;
    std::string abi;
    std::string carrier;
    std::string kind;
    std::string view;
    bool active = false;
    bool host = false;
    int packages = 0;
    int commands = 0;

    bool operator==(const SubosEntry&) const = default;
};

struct SubosEnvVar {
    std::string var;
    std::string op;
    std::string value;

    bool operator==(const SubosEnvVar&) const = default;
};

struct SubosEvent {
    std::string timestamp;
    std::string kind;
    std::string session;
    std::string summary;
    std::string raw_json;

    bool operator==(const SubosEvent&) const = default;
};

struct RepoEntry {
    std::string name;
    std::string scope;
    std::string url;

    bool operator==(const RepoEntry&) const = default;
};

struct IndexSnapshot {
    std::string index_version;
    std::string generated_at;
    std::string artifact_name;
    std::uint64_t size_bytes = 0;
    bool current = false;
    bool installed = false;

    bool operator==(const IndexSnapshot&) const = default;
};

struct IndexRepoVersions {
    std::string name;
    std::string current;
    std::string installed;
    std::string pinned;
    bool git_managed = false;
    std::vector<IndexSnapshot> snapshots;

    bool operator==(const IndexRepoVersions&) const = default;
};

struct LocalXpkg {
    std::string name;
    std::string version;
    std::string status;
    std::string source;

    bool operator==(const LocalXpkg&) const = default;
};

struct ConfigSnapshot {
    std::string xlings_home;
    std::string xlings_data;
    std::string xlings_subos;
    std::string active_subos;
    std::string bin_dir;
    std::string mirror;
    std::string lang;
    std::string ui_mode;
    std::string theme;
    std::string interactive;
    std::string raw_text;

    bool operator==(const ConfigSnapshot&) const = default;
};

struct ProfileGeneration {
    std::string id;
    std::string summary;
    std::string raw_line;

    bool operator==(const ProfileGeneration&) const = default;
};

struct SelfLogEntry {
    std::string command;
    std::string started;
    std::string trace;
    int exit_code = 0;

    bool operator==(const SelfLogEntry&) const = default;
};

struct RemovePlanItem {
    std::string version;
    std::uint64_t bytes = 0;

    bool operator==(const RemovePlanItem&) const = default;
};

struct RemovePlanKept {
    std::string version;
    std::string reason;

    bool operator==(const RemovePlanKept&) const = default;
};

struct RemovePlan {
    std::string name;
    std::string older_than;
    bool dry_run = true;
    std::uint64_t total_bytes = 0;
    std::vector<RemovePlanItem> remove;
    std::vector<RemovePlanKept> keep;

    bool operator==(const RemovePlan&) const = default;
};

struct ExpiredPackagePlan {
    std::string key;
    std::string ns;
    std::string name;
    std::string cutoff_version;
    bool cutoff_is_active = false;
    std::size_t installed_count = 0;
    RemovePlan plan;

    bool operator==(const ExpiredPackagePlan&) const = default;
};

struct PayloadStatusItem {
    std::string request;
    std::string ns;
    std::string name;
    std::string version;
    std::string origin;
    std::string payload_dir;
    std::string error;
    int revision = 0;
    std::optional<int> installed_revision;
    bool complete = false;
    bool current = false;
    bool usable = false;

    bool operator==(const PayloadStatusItem&) const = default;
};

struct UpdatablePackagePlan {
    std::string key;
    std::string ns;
    std::string name;
    std::string current_version;
    std::string active_version;
    std::string latest_version;
    int installed_revision = 0;
    int latest_revision = 0;
    bool is_revision_only = false;
    bool is_skipped = false;
    std::string skip_reason;

    bool operator==(const UpdatablePackagePlan&) const = default;
};

struct NdjsonEvent {
    std::string kind;
    std::string data_kind;
    std::string message;
    std::string level;
    std::optional<float> progress_percent;
    std::optional<int> exit_code;
    std::string raw_line;

    bool operator==(const NdjsonEvent&) const = default;
};

struct CommandOutcome {
    bool ok = false;
    int exit_code = -1;
    std::string error_message;
    std::string raw_output;
    std::vector<NdjsonEvent> events;

    bool operator==(const CommandOutcome&) const = default;
};

struct SearchOutcome {
    std::vector<SearchPkg> packages;
    bool ok = true;
    std::string error_message;

    bool operator==(const SearchOutcome&) const = default;
};

} // namespace hxlings

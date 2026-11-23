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
    std::string default_runtime;
    std::vector<std::string> archive_dirs;
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

struct NetworkEndpoint {
    std::string region;
    std::string resource;
    std::string url;

    bool operator==(const NetworkEndpoint&) const = default;
};

struct NetworkProbeResult {
    std::string url;
    std::string host;
    std::string status;
    bool reachable = false;
    int http_status = 0;
    double elapsed_seconds = 0.0;
    bool proxy = false;
    bool ok = false;
    std::string error_message;

    bool operator==(const NetworkProbeResult&) const = default;
};

struct ConfigThemeItem {
    std::string name;
    std::string source;
    bool active = false;

    bool operator==(const ConfigThemeItem&) const = default;
};

struct SubosGateStatus {
    std::string gate;
    bool enforced = false;
    std::string reason;
    std::string route;
    bool supported = false;

    bool operator==(const SubosGateStatus&) const = default;
};

struct SubosPolicySnapshot {
    std::string extends;
    std::string source;
    std::string identity;
    std::string net;
    std::string proxy;
    bool disable_user_ns = false;
    bool no_degrade = false;
    std::string observe_level;
    std::string fetch_default;
    std::string index_update;
    std::vector<std::string> grants;
    std::vector<std::string> grants_allowed;
    std::vector<std::string> env_pass;
    std::vector<std::string> mounts;

    bool operator==(const SubosPolicySnapshot&) const = default;
};

struct SubosStatusSnapshot {
    std::string instance;
    std::string policy_source;
    bool enters = false;
    std::string backend;
    std::vector<SubosGateStatus> gates;
    SubosPolicySnapshot requested;
    std::string raw_json;

    bool operator==(const SubosStatusSnapshot&) const = default;
};

struct SubosDoctorFinding {
    std::string instance;
    std::string check;
    std::string level;
    std::string detail;
    std::string fix;

    bool operator==(const SubosDoctorFinding&) const = default;
};

struct SubosDoctorSnapshot {
    int errors = 0;
    std::vector<SubosGateStatus> gates;
    std::vector<SubosDoctorFinding> findings;
    std::string raw_json;

    bool operator==(const SubosDoctorSnapshot&) const = default;
};

struct SubosSessionItem {
    std::string id;
    std::string instance;
    std::string backend;
    std::string started;
    std::string state;
    int pid = 0;
    std::string command;
    std::string raw_json;

    bool operator==(const SubosSessionItem&) const = default;
};

struct SubosRequestItem {
    std::string id;
    std::string instance;
    std::string kind;
    std::string resource;
    std::string detail;
    std::string state;
    std::string requested_at;
    std::string raw_json;

    bool operator==(const SubosRequestItem&) const = default;
};

struct SubosReportSession {
    std::string id;
    std::string backend;
    std::string started;
    int commands = 0;
    int executions = 0;
    int exit_code = 0;
    std::uint64_t duration_ms = 0;
    std::vector<std::string> denied;
    std::vector<std::string> changed;
    std::vector<std::string> programs;

    bool operator==(const SubosReportSession&) const = default;
};

struct SubosReportSnapshot {
    std::string instance;
    std::vector<std::string> policy_changes;
    std::vector<SubosReportSession> sessions;
    std::string raw_json;

    bool operator==(const SubosReportSnapshot&) const = default;
};

struct SubosCreateOptions {
    std::string name;
    std::string from_subos;
    std::string sandbox;
    std::string runtime;
    std::string abi;
    std::string storage;
    std::string proxy;
    bool rootfs = false;
    bool dry_run = false;

    bool operator==(const SubosCreateOptions&) const = default;
};

struct SubosPolicyPatch {
    std::string sandbox;
    std::string net;
    std::string proxy;
    bool clear_proxy = false;
    std::string fetch_default;
    std::string index_update;
    std::string observe_level;
    std::string allow_cap;
    std::string disallow_cap;
    std::string env_pass_csv;
    std::string mount_spec;
    std::string unmount_spec;
    std::optional<bool> no_degrade;
    bool policy_upgrade = false;
    bool reset = false;

    bool operator==(const SubosPolicyPatch&) const = default;
};

} // namespace hxlings

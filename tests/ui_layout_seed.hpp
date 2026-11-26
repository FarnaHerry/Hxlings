#pragma once

namespace hxlings_test {


class DeployedResources final : public PlatformResources {
public:
    explicit DeployedResources(std::filesystem::path root) : root_(std::move(root)) {}

    [[nodiscard]] ResourceConfiguration Configuration() const override {
        return {};
    }

    [[nodiscard]] std::optional<InputStream> OpenRead(std::string_view package_path) override {
        std::error_code ec;
        const std::filesystem::path full = root_ / std::filesystem::path(std::string(package_path));
        if (!std::filesystem::exists(full, ec)) {
            return std::nullopt;
        }
        auto opened = File(std::u8string_view(full.u8string())).OpenRead();
        if (opened.Succeeded()) {
            return std::move(opened).Value();
        }
        return std::nullopt;
    }

private:
    std::filesystem::path root_;
};

std::filesystem::path ResolveResourcesRoot(const char* argv0) {
    std::error_code ec;
    const auto probe_base = [&](const std::filesystem::path& base) -> std::optional<std::filesystem::path> {
        if (base.empty()) return std::nullopt;
        for (const char* sub : {
                 "hxlings.resources",
                 "HuxerUI",
                 "hxlings.app/Contents/Resources/HuxerUI",
             }) {
            const std::filesystem::path p = base / sub;
            if (std::filesystem::exists(p / "huxerui" / "resources.bin", ec) ||
                std::filesystem::exists(p, ec)) {
                return p;
            }
        }
        return std::nullopt;
    };

    if (argv0 != nullptr && *argv0 != '\0') {
        const std::filesystem::path canon = std::filesystem::weakly_canonical(argv0, ec);
        if (!ec) {
            if (auto found = probe_base(canon.parent_path())) return *found;
        }
        if (auto found = probe_base(std::filesystem::path(argv0).parent_path())) return *found;
    }
    if (auto found = probe_base(std::filesystem::current_path(ec))) return *found;
    return std::filesystem::current_path(ec) / "hxlings.resources";
}

std::shared_ptr<XlingsStore> MakeSeededStore() {
    auto store = std::make_shared<XlingsStore>();
    store->skip_initial_refresh = true;
    store->initial_loaded = true;
    store->indexes_loaded = true;
    store->index_versions_loaded = true;
    store->xlings_config_loaded = true;
    store->profiles_loaded = true;
    store->InitAppSettings(AppSettings{});

    store->version_info = XlingsVersionInfo{
        .version = "2026.11.22.1",
        .protocol_version = "1.18",
        .capabilities = {"env", "list_packages", "remove_package", "list_subos", "sync_index"},
        .features = {"subos", "xim", "xvm"},
    };

    store->env = XlingsEnv{
        .xlings_home = "/home/farna/.xlings",
        .data_dir = "/home/farna/.xlings/data",
        .subos_dir = "/home/farna/.xlings/subos/default",
        .bin_dir = "/home/farna/.xlings/subos/default/bin",
        .lib_dir = "/home/farna/.xlings/subos/default/lib",
        .active_subos = "default",
        .mirror = "CN",
        .lang = "zh",
        .binary_path = "/home/farna/.xlings/subos/current/bin/xlings",
    };

    store->system_status = SystemStatus{
        .title = "xlings status",
        .fields = {
            {.label = "active subos", .value = "default", .highlight = true},
            {.label = "mirror", .value = "CN", .highlight = false},
        },
        .findings = {
            BindingFinding{
                .code = "xvm-active-group-incoherent",
                .severity = "broken",
                .subos = "default",
                .target = "zlib.files.3",
                .version = "1.3.1",
                .summary = "libz.so active binding points to an older payload target (zlib.files.3) that diverges from current SubOS shim anchor",
                .hint = "run `xlings use libz.so 1.3.1` or `xlings self doctor --fix`",
            },
        },
    };

    std::vector<InstalledPkg> raw_pkgs;
    auto add_pkg = [&](std::string ns, std::string name, std::string ver, std::string desc, bool active, bool degraded = false) {
        std::string status = active ? std::string{} : (degraded ? std::string("degraded: index entry unavailable") : std::string("inactive"));
        raw_pkgs.push_back(InstalledPkg{
            .raw_spec = ns + ":" + name + "@" + ver,
            .ns = std::move(ns),
            .name = std::move(name),
            .version = std::move(ver),
            .description = std::move(desc),
            .status = std::move(status),
            .is_active = active,
            .is_degraded = degraded,
        });
    };

    add_pkg("xim", "xlings", "2026.11.22.1", "Xlings | Highly abstract package manager - Multi-version management + Everything can be a package", true);
    add_pkg("xim", "xlings", "2026.11.16.1", "Xlings | Highly abstract package manager", false);
    add_pkg("xim", "xlings", "2026.10.26.1", "Xlings | Highly abstract package manager", false);
    add_pkg("xim", "xlings", "2026.10.19.2", "Xlings | Highly abstract package manager", false);
    add_pkg("xim", "xlings", "2026.9.30.1", "Xlings | Highly abstract package manager", false);
    add_pkg("xim", "xlings", "2026.8.27.5", "Xlings | Highly abstract package manager", false);

    add_pkg("xim", "mcpp", "2026.11.21.1", "A modern C++ build tool with module support, dependency/toolchain management, and packaging", true);
    add_pkg("xim", "mcpp", "2026.11.18.1", "A modern C++ build tool", false);
    add_pkg("xim", "mcpp", "2026.11.15.1", "A modern C++ build tool", false);
    add_pkg("xim", "mcpp", "2026.10.31.3", "A modern C++ build tool", false);
    add_pkg("xim", "mcpp", "2026.10.26.2", "A modern C++ build tool", false);
    add_pkg("xim", "mcpp", "2026.9.30.1", "A modern C++ build tool", false);
    add_pkg("xim", "mcpp", "2026.9.16.2", "A modern C++ build tool", false);
    add_pkg("xim", "mcpp", "2026.8.29.1", "A modern C++ build tool", false);

    add_pkg("xim", "glibc", "2.44.2", "The GNU C Library (glibc)", true);
    add_pkg("xim", "glibc", "2.44.3", "The GNU C Library (glibc)", false);
    add_pkg("xim", "glibc", "2.39", "The GNU C Library (glibc)", false);

    add_pkg("local", "node", "26.13.0", "Node.js JavaScript runtime with local override index entry", false, true);
    add_pkg("xim", "bwrap", "0.11.2", "Bubblewrap namespace-based unprivileged sandboxing tool", false);
    add_pkg("xim", "llvm", "22.1.8", "LLVM/Clang C++23 compiler toolchain with libc++ and lld", true);
    add_pkg("xim", "cmake", "4.4.2", "Cross-platform open-source build system generator", true);
    add_pkg("xim", "gtk4", "4.16.13", "GTK 4 multi-platform toolkit for creating graphical user interfaces", true);
    add_pkg("xim", "cairo", "1.18.4", "2D graphics library supporting multiple output devices", true);
    add_pkg("xim", "glib", "2.88.3", "Low-level core library for data structures, event loops, and I/O", true);
    add_pkg("xim", "harfbuzz", "14.4.0", "OpenType text shaping engine for Latin and CJK scripts", true);
    add_pkg("xim", "pango", "1.52.1", "Library for layout and rendering of internationalized text", true);
    add_pkg("xim", "mesa", "25.0.7.2", "Open-source OpenGL and Vulkan graphics driver implementation", true);
    add_pkg("xim", "sqlite", "3.53.4", "Self-contained serverless zero-configuration SQL database engine", true);

    store->installed_raw = raw_pkgs;
    store->installed_groups = GroupInstalledPackages(raw_pkgs);
    store->selected_installed_key = "xim:xlings";

    store->selected_installed_detail = PackageDetail{
        .title = "xim:xlings",
        .package = "xim:xlings",
        .ns = "xim",
        .name = "xlings",
        .description = "Xlings | Highly abstract package manager - Multi-version management + Everything can be a package",
        .status = "stable",
        .selected_version = "2026.11.22.1",
        .available_versions = {
            "2026.11.22.1", "2026.11.16.1", "2026.10.26.1", "2026.10.19.2", "2026.10.17.1",
            "2026.10.9.2", "2026.10.4.1", "2026.9.30.1", "2026.9.26.2", "2026.9.16.1",
            "2026.9.12.1", "2026.9.5.1", "2026.9.3.2", "2026.8.27.5", "2026.8.20.1",
            "2026.8.15.1", "2026.8.10.1", "2026.8.5.1", "2026.8.1.1", "2026.7.25.1",
            "2026.7.20.1", "2026.7.15.1", "2026.7.10.1", "2026.7.5.1", "2026.7.1.1",
        },
        .categories = {"tools", "package-manager"},
        .archs = {"x86_64", "aarch64"},
        .fields = {
            {.label = "status", .value = "stable", .highlight = true},
            {.label = "license", .value = "Apache-2.0", .highlight = false},
            {.label = "homepage", .value = "https://github.com/d2learn/xlings", .highlight = false},
            {.label = "runtime deps", .value = "xim:glibc@2.44.2, xim:gcc-runtime@15.1.0, xim:zlib@1.3.1, xim:openssl@3.4.0, xim:curl@8.12.1", .highlight = false},
            {.label = "build deps", .value = "xim:cmake@4.4.2, xim:llvm@22.1.8, xim:ninja@1.12.1", .highlight = false},
            {.label = "subos", .value = "default", .highlight = true},
            {.label = "maintainer", .value = "d2learn-community", .highlight = false},
            {.label = "checksum", .value = "sha256:9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08", .highlight = false},
        },
    };

    store->selected_installed_versions = {
        {.version = "2026.11.22.1", .path = "/home/farna/.xlings/data/xpkgs/xim-x-xlings/2026.11.22.1", .active = true},
        {.version = "2026.11.16.1", .path = "/home/farna/.xlings/data/xpkgs/xim-x-xlings/2026.11.16.1", .active = false},
        {.version = "2026.10.26.1", .path = "/home/farna/.xlings/data/xpkgs/xim-x-xlings/2026.10.26.1", .active = false},
        {.version = "2026.10.19.2", .path = "/home/farna/.xlings/data/xpkgs/xim-x-xlings/2026.10.19.2", .active = false},
        {.version = "2026.9.30.1", .path = "/home/farna/.xlings/data/xpkgs/xim-x-xlings/2026.9.30.1", .active = false},
        {.version = "2026.8.27.5", .path = "/home/farna/.xlings/data/xpkgs/xim-x-xlings/2026.8.27.5", .active = false},
    };

    store->installed_plan_preview = {
        {.spec = "xim:glibc@2.44.2", .note = "already satisfied", .action_code = 0},
        {.spec = "xim:xlings@2026.11.22.1", .note = "up to date", .action_code = 1},
    };

    RemovePlan xlings_remove_plan{
        .name = "xlings",
        .older_than = "2026.11.22.1",
        .dry_run = true,
        .total_bytes = 1099397952ULL,
        .remove = {
            {.version = "2026.11.16.1", .bytes = 49518230ULL},
            {.version = "2026.10.26.1", .bytes = 49093304ULL},
            {.version = "2026.10.19.2", .bytes = 42194245ULL},
            {.version = "2026.9.30.1", .bytes = 132994043ULL},
            {.version = "2026.8.27.5", .bytes = 118491069ULL},
        },
        .keep = {},
    };
    store->selected_remove_plan = xlings_remove_plan;

    std::vector<RemovePlanItem> mcpp_items;
    mcpp_items.reserve(25);
    for (int i = 0; i < 25; ++i) {
        mcpp_items.push_back(RemovePlanItem{
            .version = "2026.10." + std::to_string(25 - i) + ".1",
            .bytes = 25099039ULL,
        });
    }
    store->has_scanned_expired = true;
    store->expired_plans = {
        ExpiredPackagePlan{
            .key = "xim:xlings",
            .ns = "xim",
            .name = "xlings",
            .cutoff_version = "2026.11.22.1",
            .cutoff_is_active = true,
            .installed_count = 14,
            .plan = RemovePlan{
                .name = "xlings",
                .older_than = "2026.11.22.1",
                .dry_run = true,
                .total_bytes = 1099397952ULL,
                .remove = {
                    {.version = "2026.11.16.1", .bytes = 49518230ULL},
                    {.version = "2026.10.26.1", .bytes = 49093304ULL},
                    {.version = "2026.10.19.2", .bytes = 42194245ULL},
                    {.version = "2026.10.17.1", .bytes = 39979315ULL},
                    {.version = "2026.10.9.2", .bytes = 27998639ULL},
                    {.version = "2026.10.4.1", .bytes = 18910978ULL},
                    {.version = "2026.9.30.1", .bytes = 132994043ULL},
                    {.version = "2026.9.26.2", .bytes = 128322069ULL},
                    {.version = "2026.9.16.1", .bytes = 125578521ULL},
                    {.version = "2026.9.12.1", .bytes = 124695718ULL},
                    {.version = "2026.9.5.1", .bytes = 121500951ULL},
                    {.version = "2026.9.3.2", .bytes = 120120870ULL},
                    {.version = "2026.8.27.5", .bytes = 118491069ULL},
                },
                .keep = {},
            },
        },
        ExpiredPackagePlan{
            .key = "xim:mcpp",
            .ns = "xim",
            .name = "mcpp",
            .cutoff_version = "2026.11.21.1",
            .cutoff_is_active = true,
            .installed_count = 26,
            .plan = RemovePlan{
                .name = "mcpp",
                .older_than = "2026.11.21.1",
                .dry_run = true,
                .total_bytes = 627475989ULL,
                .remove = std::move(mcpp_items),
                .keep = {},
            },
        },
        ExpiredPackagePlan{
            .key = "xim:glibc",
            .ns = "xim",
            .name = "glibc",
            .cutoff_version = "2.44.2",
            .cutoff_is_active = true,
            .installed_count = 3,
            .plan = RemovePlan{
                .name = "glibc",
                .older_than = "2.44.2",
                .dry_run = true,
                .total_bytes = 0ULL,
                .remove = {},
                .keep = {{.version = "2.39", .reason = "linked by xim:gcc-runtime@15.1.0"}},
            },
        },
    };

    store->has_scanned_updates = true;
    store->updatable_plans = {
        UpdatablePackagePlan{
            .key = "xim:huxerui",
            .ns = "xim",
            .name = "huxerui",
            .current_version = "0.3.0",
            .active_version = "0.3.0",
            .latest_version = "0.5.0",
            .is_revision_only = false,
            .is_skipped = false,
        },
        UpdatablePackagePlan{
            .key = "xim:pnpm",
            .ns = "xim",
            .name = "pnpm",
            .current_version = "12.0.0",
            .active_version = "12.0.0",
            .latest_version = "12.22.0",
            .is_revision_only = false,
            .is_skipped = false,
        },
        UpdatablePackagePlan{
            .key = "xim:bwrap",
            .ns = "xim",
            .name = "bwrap",
            .current_version = "0.11.2",
            .active_version = "",
            .latest_version = "0.11.2",
            .installed_revision = 0,
            .latest_revision = 1,
            .is_revision_only = true,
            .is_skipped = false,
        },
        UpdatablePackagePlan{
            .key = "local:node",
            .ns = "local",
            .name = "node",
            .current_version = "26.13.0",
            .active_version = "",
            .is_skipped = true,
            .skip_reason = "package 'local:node' not found in the synced index",
        },
    };

    store->has_searched = true;
    store->search_results = {
        SearchPkg{.package = "xim:gcc", .ns = "xim", .name = "gcc", .version = "16.1.0",
                  .description = "GNU Compiler Collection supporting C, C++23 modules, and Fortran", .status = "stable", .index = "xim"},
        SearchPkg{.package = "xim:llvm", .ns = "xim", .name = "llvm", .version = "22.1.8",
                  .description = "Modular compiler and toolchain technologies", .status = "stable", .index = "xim"},
        SearchPkg{.package = "xim:mcpp", .ns = "xim", .name = "mcpp", .version = "2026.11.21.1",
                  .description = "Modern C++ build tool with module support and dependency resolution", .status = "stable", .index = "xim"},
        SearchPkg{.package = "xim:rust", .ns = "xim", .name = "rust", .version = "1.88.0",
                  .description = "Systems programming language empowering reliable software", .status = "stable", .index = "xim"},
        SearchPkg{.package = "xim:node", .ns = "xim", .name = "node", .version = "26.13.0",
                  .description = "Node.js JavaScript runtime built on Chrome V8 engine", .status = "beta", .index = "xim"},
        SearchPkg{.package = "xim:cmake", .ns = "xim", .name = "cmake", .version = "4.4.2",
                  .description = "Family of tools designed to build, test, and package software", .status = "stable", .index = "xim"},
    };
    store->selected_store_pkg = "xim:gcc";
    store->selected_store_detail = PackageDetail{
        .title = "xim:gcc",
        .package = "xim:gcc",
        .ns = "xim",
        .name = "gcc",
        .description = "The GNU Compiler Collection includes front ends for C, C++, Objective-C, Fortran, Ada, and Go",
        .status = "stable",
        .selected_version = "16.1.0",
        .available_versions = {"16.1.0", "15.1.0", "14.2.0", "13.3.0"},
        .fields = {
            {.label = "namespace", .value = "xim", .highlight = true},
            {.label = "runtime deps", .value = "xim:glibc@2.44.2, xim:binutils@2.47, xim:gmp@6.3.0, xim:mpfr@4.2.1", .highlight = false},
            {.label = "license", .value = "GPL-3.0-or-later", .highlight = false},
        },
    };
    store->store_plan_preview = {
        {.spec = "xim:glibc@2.44.2", .note = "installed", .action_code = 0},
        {.spec = "xim:binutils@2.47", .note = "new install", .action_code = 1},
        {.spec = "xim:gcc@16.1.0", .note = "new install", .action_code = 1},
    };

    store->subos_list = {
        SubosEntry{
            .name = "default",
            .dir = "/home/farna/.xlings/subos/default",
            .abi = "native",
            .carrier = "local",
            .kind = "view",
            .view = "overlay",
            .active = true,
            .host = false,
            .packages = 114,
            .commands = 118,
        },
        SubosEntry{
            .name = "cpp23-dev",
            .dir = "/home/farna/.xlings/subos/cpp23-dev",
            .abi = "x86_64-linux-gnu",
            .carrier = "local",
            .kind = "sandbox",
            .view = "isolated",
            .active = false,
            .host = false,
            .packages = 12,
            .commands = 24,
        },
    };
    store->selected_subos = "default";

    std::vector<std::string> shims;
    for (const char* name : {
             "xlings", "mcpp", "gcc", "g++", "clang", "clang++", "lld", "ld.lld",
             "cmake", "ctest", "ninja", "node", "npm", "npx", "pnpm", "python3",
             "bwrap", "sqlite3", "pkg-config", "patchelf", "fc-list", "hb-shape",
             "pango-view", "glib-compile-schemas", "gtk4-builder-tool", "gdb",
             "llvm-ar", "llvm-nm", "llvm-objdump", "clang-scan-deps", "clangd",
             "clang-format", "clang-tidy", "hcg", "hrc", "xvm", "xim", "make", "tar", "curl"}) {
        shims.emplace_back(name);
    }
    store->active_shims = std::move(shims);

    store->selected_subos_env = {
        {.var = "XLINGS_HOME", .op = "set", .value = "/home/farna/.xlings"},
        {.var = "PATH", .op = "prepend", .value = "/home/farna/.xlings/subos/default/bin:/home/farna/.xlings/bin"},
        {.var = "LD_LIBRARY_PATH", .op = "prepend", .value = "/home/farna/.xlings/subos/default/lib:/home/farna/.xlings/data/xpkgs/xim-x-mesa/25.0.7.2/lib:/home/farna/.xlings/data/xpkgs/xim-x-llvm/22.1.8/lib"},
        {.var = "PKG_CONFIG_LIBDIR", .op = "set", .value = "/home/farna/.xlings/subos/default/usr/lib/pkgconfig:/home/farna/.xlings/subos/default/usr/share/pkgconfig"},
        {.var = "EGL_VENDOR_DIRS", .op = "set", .value = "/home/farna/.xlings/subos/default/share/glvnd/egl_vendor.d"},
    };

    store->selected_subos_events = {
        {.timestamp = "2026-11-22T08:15:10Z", .kind = "lifecycle", .session = "sess-01", .summary = "subos default mounted overlay view cleanly"},
        {.timestamp = "2026-11-22T08:16:02Z", .kind = "ops", .session = "sess-02", .summary = "remove_package xlings --older-than 2026.11.22.1 --dry-run (13 candidates)"},
        {.timestamp = "2026-11-22T08:17:45Z", .kind = "exec", .session = "sess-03", .summary = "exec argv=[mcpp, build, --profile, release] exit=0"},
        {.timestamp = "2026-11-22T08:18:11Z", .kind = "perm", .session = "sess-03", .summary = "allowed read /home/farna/dev/cpp/mcpp/Hxlings"},
        {.timestamp = "2026-11-22T08:19:22Z", .kind = "net", .session = "sess-04", .summary = "sync_index xim completed in 340ms"},
        {.timestamp = "2026-11-22T08:20:05Z", .kind = "fs", .session = "sess-05", .summary = "shim verified /home/farna/.xlings/subos/default/bin/xlings"},
    };

    store->subos_exec_output = "xlings 2026.11.22.1\nsubos: default\nabi: native\nexitCode: 0\n";

    store->repos = {
        {.name = "xim", .scope = "global", .url = "https://gitee.com/sunrisepeak/xim-pkgindex.git"},
        {.name = "custom-team", .scope = "project", .url = "https://github.com/FarnaHerry/custom-xim-pkgindex.git"},
    };

    store->index_versions = {
        IndexRepoVersions{
            .name = "xim",
            .current = "bafdb27",
            .installed = "bafdb27",
            .pinned = "",
            .git_managed = false,
            .snapshots = {
                {.index_version = "bafdb27", .generated_at = "2026-11-22T07:36:45Z", .artifact_name = "xim-bafdb27.tar.gz", .size_bytes = 1957663ULL, .current = true, .installed = true},
                {.index_version = "92c4e10", .generated_at = "2026-11-20T18:12:00Z", .artifact_name = "xim-92c4e10.tar.gz", .size_bytes = 1941200ULL, .current = false, .installed = true},
                {.index_version = "78a112f", .generated_at = "2026-11-18T12:00:00Z", .artifact_name = "xim-78a112f.tar.gz", .size_bytes = 1920041ULL, .current = false, .installed = false},
                {.index_version = "64b009c", .generated_at = "2026-11-15T09:44:10Z", .artifact_name = "xim-64b009c.tar.gz", .size_bytes = 1899110ULL, .current = false, .installed = false},
                {.index_version = "51f9d88", .generated_at = "2026-11-10T14:22:30Z", .artifact_name = "xim-51f9d88.tar.gz", .size_bytes = 1850420ULL, .current = false, .installed = false},
                {.index_version = "40e8b31", .generated_at = "2026-11-05T11:10:00Z", .artifact_name = "xim-40e8b31.tar.gz", .size_bytes = 1812090ULL, .current = false, .installed = false},
                {.index_version = "33c7a19", .generated_at = "2026-10-30T16:05:45Z", .artifact_name = "xim-33c7a19.tar.gz", .size_bytes = 1794500ULL, .current = false, .installed = false},
                {.index_version = "21a4f02", .generated_at = "2026-10-22T08:30:12Z", .artifact_name = "xim-21a4f02.tar.gz", .size_bytes = 1762100ULL, .current = false, .installed = false},
            },
        },
    };

    store->local_xpkgs = {
        {.name = "winscp", .version = "6.7.1", .status = "unique", .source = "/home/farna/dev/cpp/mcpp/Hxlings/examples/recipes/winscp.lua"},
        {.name = "my-custom-tool", .version = "0.2.0", .status = "active", .source = "/home/farna/dev/cpp/mcpp/xim-pkgindex/pkgs/m/my-custom-tool.lua"},
    };

    store->config = ConfigSnapshot{
        .xlings_home = "/home/farna/.xlings",
        .xlings_data = "/home/farna/.xlings/data",
        .xlings_subos = "/home/farna/.xlings/subos",
        .active_subos = "default",
        .bin_dir = "/home/farna/.xlings/subos/default/bin",
        .mirror = "CN",
        .lang = "auto (zh)",
        .ui_mode = "cli",
        .theme = "light",
        .interactive = "false",
        .default_runtime = "glibc@2.44",
        .archive_dirs = {"/tmp/xlings-archives"},
    };

    store->profiles = {
        {.id = "gen-003", .summary = "2026-11-22 15:40 · before upgrading llvm@22.1.8 and gtk4"},
        {.id = "gen-002", .summary = "2026-11-18 11:20 · switched mirror to CN"},
        {.id = "gen-001", .summary = "2026-11-10 09:00 · initial clean subos baseline"},
    };

    store->doctor_output = "[doctor] checking subos 'default'...\n[warn] xvm-active-group-incoherent: libz.so (zlib.files.3)\n[info] 114 packages checked, 1 issue found\n";
    store->clean_output = "[dry-run] reclaimable download cache: 142.4 MiB\n[dry-run] unreferenced payloads: 0 items\n";

    std::vector<SelfLogEntry> recent_logs;
    for (int i = 0; i < 12; ++i) {
        recent_logs.push_back(SelfLogEntry{
            .command = (i % 3 == 0)
                           ? "xlings interface list_packages --args {\"filter\":\"gtk4-development-runtime\"}"
                           : (i % 2 == 0 ? "xlings interface subos_events --args {\"limit\":120,\"name\":\"default\"}" : "xlings interface env"),
            .started = "2026-11-22T08:25:52Z",
            .trace = "trace-" + std::to_string(i),
            .exit_code = (i == 2) ? 1 : 0,
        });
    }
    store->recent_logs = std::move(recent_logs);

    for (int i = 0; i < 12; ++i) {
        store->console_lines.Update([i](std::vector<std::string>& lines) {
            lines.push_back("[08:25:" + std::to_string(10 + i) + "] xlings check line #" + std::to_string(i));
        });
    }

    store->config_themes = std::vector<ConfigThemeItem>{
        ConfigThemeItem{.name = "default", .source = "(built in)", .active = true},
        ConfigThemeItem{.name = "tokyo-night", .source = "@xlings/config/themes/tokyo-night.json", .active = false},
    };
    store->network_endpoints = std::vector<NetworkEndpoint>{
        NetworkEndpoint{.region = "GLOBAL", .resource = "xim", .url = "https://raw.githubusercontent.com/openxlings/xim-pkgindex/main"},
        NetworkEndpoint{.region = "CN", .resource = "xim", .url = "https://raw.gitcode.com/sunrisepeak/xim-pkgindex/raw/main"},
    };
    store->network_probe_result = NetworkProbeResult{
        .url = "https://github.com/FarnaHerry/Hxlings",
        .host = "github.com",
        .status = "reachable",
        .reachable = true,
        .http_status = 200,
        .elapsed_seconds = 0.128,
        .proxy = false,
        .ok = true,
    };
    store->script_output = std::string("[xlings script] hello from test\n");
    store->selected_subos_status = SubosStatusSnapshot{
        .instance = "default",
        .policy_source = "@xlings/config/policies/dev.json",
        .enters = true,
        .backend = "bwrap",
        .gates = {
            SubosGateStatus{.gate = "userns", .enforced = true, .reason = "unprivileged user namespaces ready", .route = "userns", .supported = true},
        },
        .requested = SubosPolicySnapshot{
            .extends = "dev",
            .source = "inline",
            .net = "nat",
            .observe_level = "standard",
            .fetch_default = "ask",
            .index_update = "auto",
        },
    };
    store->selected_subos_policy = store->selected_subos_status.Get().requested;
    store->selected_subos_sessions = std::vector<SubosSessionItem>{
        SubosSessionItem{.id = "sess-01", .instance = "default", .backend = "bwrap", .started = "2026-11-23T00:00:00Z", .state = "running", .pid = 4321, .command = "bash"},
    };
    store->selected_subos_requests = std::vector<SubosRequestItem>{
        SubosRequestItem{.id = "req-1", .instance = "default", .kind = "pkg", .resource = "xim:cmake", .detail = "sandbox install request", .state = "pending"},
    };
    store->selected_subos_report = SubosReportSnapshot{
        .instance = "default",
        .policy_changes = {"net=nat"},
        .sessions = {
            SubosReportSession{.id = "rep-1", .backend = "bwrap", .started = "2026-11-23", .commands = 4, .executions = 2, .exit_code = 0, .duration_ms = 320},
        },
    };
    store->selected_subos_info = std::string("=== default ===\nactive: yes\npackages: 115\ncommands: 119\n");
    store->subos_doctor_snapshot = SubosDoctorSnapshot{
        .errors = 0,
        .findings = {
            SubosDoctorFinding{.instance = "default", .check = "userns", .level = "info", .detail = "sandbox backend operational", .fix = ""},
        },
    };
    store->subos_diff_output = std::string("两个 SubOS 的包清单完全一致（无差异）。");
    store->subos_rollback_list_output = std::string("generation 1 (active)");
    store->app_update_state = AppUpdateState{
        .phase = UpdatePhase::Available,
        .current_version = std::string(kAppVersion),
        .latest = AppReleaseInfo{
            .tag_name = "v0.1.6",
            .version = "0.1.6",
            .name = "Hxlings v0.1.6",
            .published_at = "2026-11-23T01:45:00Z",
            .html_url = "https://github.com/FarnaHerry/Hxlings/releases/tag/v0.1.6",
            .body = "## Highlights\n- Tray popup dark/light mode sync\n- In-app self update with SHA-256 check",
        },
        .selected_asset = ReleaseAssetInfo{
            .name = "hxlings-0.1.6-linux-x86_64.AppImage",
            .download_url = "https://github.com/FarnaHerry/Hxlings/releases/download/v0.1.6/hxlings-0.1.6-linux-x86_64.AppImage",
            .size_bytes = 33405432ULL,
        },
        .install_kind = UpdateInstallKind::LinuxAppImage,
        .total_bytes = 33405432ULL,
        .status_message = "发现新版本 v0.1.6",
        .checked_at = "2026-11-23 09:45:00",
    };

    store->luban_available = true;
    store->luban_os_available = false;
    store->luban_loaded = true;
    store->luban_host_status = LubanHostStatus{
        .ready = true,
        .os = "linux",
        .arch = "x86_64",
        .backend_name = "bwrap",
        .backend_path = "/usr/bin/bwrap",
        .sandbox_ready = true,
        .machine_ready = true,
        .desktop_ready = false,
        .desktop_route = "xlings install luban-os",
        .missing_tools = {
            LubanToolStatus{
                .name = "virtiofsd",
                .package = "xim:virtiofsd",
                .install_cmd = "xlings install xim:virtiofsd",
                .available = false,
            },
        },
    };
    store->luban_envs = std::vector<LubanEnvironment>{
        LubanEnvironment{
            .name = "dev-core",
            .kind = "rootfs",
            .edition = "core",
            .carrier = "bwrap",
            .shell = "bash",
            .packages = 18,
            .running = true,
        },
        LubanEnvironment{
            .name = "agent-box",
            .kind = "rootfs",
            .edition = "dev",
            .carrier = "bwrap",
            .shell = "zsh",
            .desktops = {"xfce"},
            .packages = 24,
            .running = false,
        },
    };
    store->luban_catalog = LubanCatalog{
        .distributions = {
            LubanCatalogItem{
                .group = "distributions",
                .name = "core",
                .ref = "luban:core",
                .version = "2026.11",
                .status = "stable",
                .title_zh = "Luban Core 基础系统",
                .summary_zh = "最小可复现基础 Rootfs 与 xlings 包管理运行时",
                .highlights = {"bwrap", "minimal"},
                .tier = 1,
                .available = true,
            },
            LubanCatalogItem{
                .group = "distributions",
                .name = "dev",
                .ref = "luban:dev",
                .version = "2026.11",
                .status = "stable",
                .title_zh = "Luban Dev 开发工作站",
                .summary_zh = "预置现代构建链与常用系统开发工具",
                .highlights = {"toolchain", "multi-arch"},
                .tier = 1,
                .available = true,
            },
        },
        .desktops = {
            LubanCatalogItem{
                .group = "desktops",
                .name = "xfce",
                .ref = "luban:desktop-xfce",
                .version = "4.18",
                .status = "stable",
                .title_zh = "XFCE 轻量桌面",
                .summary_zh = "低开销离线/远程隔离桌面会话",
                .gpu_hint = "virgl / swrast",
                .highlights = {"lightweight", "x11"},
                .tier = 1,
                .available = true,
            },
            LubanCatalogItem{
                .group = "desktops",
                .name = "gnome",
                .ref = "luban:desktop-gnome",
                .version = "46",
                .status = "stable",
                .title_zh = "GNOME 完整桌面",
                .summary_zh = "标准 Wayland/GUI 工作站桌面",
                .gpu_hint = "virgl",
                .highlights = {"wayland", "desktop"},
                .tier = 2,
                .available = true,
            },
        },
    };
    store->selected_luban_env = std::string("dev-core");
    store->selected_luban_status = LubanEnvStatusDetail{
        .name = "dev-core",
        .kind = "rootfs",
        .edition = "core",
        .carrier = "bwrap",
        .mode = "sandbox",
        .persist = "all",
        .enter = "unprivileged",
        .network = "nat",
        .shell = "bash",
        .notes = {"seeded test environment"},
        .packages = 18,
        .running = true,
    };
    store->selected_luban_config = LubanEnvConfigDetail{
        .name = "dev-core",
        .mode = "sandbox",
        .persist = "all",
        .shell = "bash",
        .policy_extends = "dev",
        .net = "nat",
        .observe_level = "standard",
        .fetch_policy = "auto",
    };
    store->selected_luban_history = std::string("generation 1 (active)");
    store->selected_luban_logs = std::string("[2026-11-24] [luban] dev-core ready");
    store->luban_config_cache = std::map<std::string, LubanEnvConfigDetail>{
        {"dev-core", *store->selected_luban_config.Get()},
    };
    store->saved_luban_stacks = std::vector<SavedLubanStackInfo>{
        SavedLubanStackInfo{
            .id = "team-dev",
            .filename = "team-dev.toml",
            .format = "toml",
            .path = "/tmp/team-dev.toml",
            .display_name = "team-dev",
            .description = "seeded local stack",
            .env_count = 2,
        },
    };

    return store;
}

} // namespace hxlings_test

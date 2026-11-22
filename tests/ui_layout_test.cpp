#include <huxerui/huxerui.h>
#include <huxerui/testing/ui_test.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

import app;
import hxlings.shortcuts;
import hxlings.settings;
import hxlings.store;
import xlings.types;
import xlings.parser;

using namespace huxerui;
using namespace huxerui::testing;
using namespace hxlings;
using namespace std::chrono_literals;

namespace {

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

    return store;
}

struct AuditContext {
    std::vector<std::string> anomalies;

    void Flag(const std::string& scope, const std::string& msg) {
        anomalies.push_back("[" + scope + "] " + msg);
    }
};

std::string FmtRect(const Rect& r) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(1)
       << "{x=" << r.x << ", y=" << r.y << ", w=" << r.width << ", h=" << r.height << "}";
    return os.str();
}

bool IsFiniteRect(const Rect& r) {
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.width) && std::isfinite(r.height) &&
           r.width >= 0.0F && r.height >= 0.0F;
}

void CheckSnapshotSanity(AuditContext& ctx, const std::string& scope, const UiTest& ui) {
    const std::string snap = ui.CaptureSnapshot().ToString();
    if (snap.empty()) {
        ctx.Flag(scope, "CaptureSnapshot().ToString() is empty");
        return;
    }
    const std::array<std::string_view, 5> bad_tokens{" nan", ": nan", " inf", ": inf", "-inf"};
    for (const auto tok : bad_tokens) {
        if (snap.find(tok) != std::string::npos) {
            ctx.Flag(scope, "Snapshot contains non-finite numeric token: '" + std::string(tok) + "'");
        }
    }
}

void CheckSplitPaneNodes(
    AuditContext& ctx,
    const std::string& scope,
    const UiNodeQuery& page_q,
    const Rect& left_bounds,
    const Rect& right_bounds
) {
    const float split_top = std::min(left_bounds.y, right_bounds.y) - 0.5F;

    auto check_list = [&](const std::string& kind_name, UiSelector sel) {
        for (const auto& n : page_q.Find(std::move(sel)).All()) {
            if (!n.participates_in_layout || !n.in_viewport) continue;
            if (!IsFiniteRect(n.bounds) || !IsFiniteRect(n.visible_bounds)) continue;
            if (n.bounds.y + n.bounds.height <= split_top) continue;

            const float cx = n.bounds.x + n.bounds.width * 0.5F;
            const bool in_left = cx < right_bounds.x;
            const Rect& pane = in_left ? left_bounds : right_bounds;
            const char* pane_label = in_left ? "split-left" : "split-right";
            const float min_x = pane.x - 1.0F;
            const float max_x = pane.x + pane.width + 1.0F;

            if (n.bounds.x < min_x || n.bounds.x + n.bounds.width > max_x) {
                std::string extra = n.text.has_value() ? (" text=\"" + n.text->substr(0, 48) + "\"") : "";
                ctx.Flag(scope, std::string(pane_label) + " " + kind_name + " horizontal overflow" + extra +
                                    " bounds=" + FmtRect(n.bounds) + " limit=[" + std::to_string(min_x) + ", " +
                                    std::to_string(max_x) + "]");
            }
            if (n.bounds.width > 1.0F && n.visible_bounds.width + 2.0F < n.bounds.width) {
                std::string extra = n.text.has_value() ? (" text=\"" + n.text->substr(0, 48) + "\"") : "";
                ctx.Flag(scope, std::string(pane_label) + " " + kind_name + " horizontally clipped" + extra +
                                    " bounds=" + FmtRect(n.bounds) + " visible=" + FmtRect(n.visible_bounds));
            }
        }
    };

    check_list("Text", UiSelector::Type<Text>());
    check_list("TextField", UiSelector::Type<TextField>());
    check_list("Checkbox", UiSelector::Type<Checkbox>());
    check_list("Switch", UiSelector::Type<Switch>());
    check_list("Image", UiSelector::Type<Image>());
}

void AuditCurrentScreen(
    AuditContext& ctx,
    const UiTest& ui,
    float viewport_w,
    float viewport_h,
    const std::string& screen_name,
    const std::string& active_page_key,
    bool has_split = false,
    bool modal_open = false
) {
    std::ostringstream sc;
    sc << static_cast<int>(viewport_w) << "x" << static_cast<int>(viewport_h) << " / " << screen_name;
    const std::string scope = sc.str();

    // 1. Shell sidebar nav buttons
    const std::array<const char*, 6> nav_keys{
        "nav-dashboard", "nav-packages", "nav-store", "nav-subos", "nav-indexes", "nav-settings"
    };
    std::vector<UiNodeInfo> nav_nodes;
    for (const char* nk : nav_keys) {
        auto q = ui.Find(UiSelector::Key(nk));
        if (!q.Exists()) {
            ctx.Flag(scope, std::string("Missing nav button: ") + nk);
            continue;
        }
        UiNodeInfo info = q.One();
        if (!IsFiniteRect(info.bounds) || !info.participates_in_layout || !info.in_viewport) {
            ctx.Flag(scope, std::string("Nav button not laid out in viewport: ") + nk + " bounds=" + FmtRect(info.bounds));
        } else {
            if (info.bounds.x + info.bounds.width > 214.0F) {
                ctx.Flag(scope, std::string("Nav button exceeds sidebar width: ") + nk + " bounds=" + FmtRect(info.bounds));
            }
            if (info.visible_bounds.width + 2.0F < info.bounds.width) {
                ctx.Flag(scope, std::string("Nav button clipped horizontally: ") + nk + " bounds=" + FmtRect(info.bounds));
            }
            nav_nodes.push_back(info);
        }
    }
    for (std::size_t i = 0; i + 1 < nav_nodes.size(); ++i) {
        if (nav_nodes[i].bounds.y + nav_nodes[i].bounds.height > nav_nodes[i + 1].bounds.y + 0.5F) {
            ctx.Flag(scope, "Nav buttons overlap vertically between index " + std::to_string(i) + " and " + std::to_string(i + 1));
        }
    }

    // 2. Active page bounds
    std::optional<Rect> active_page_bounds;
    if (!active_page_key.empty()) {
        auto page_q = ui.Find(UiSelector::Key(active_page_key));
        if (!page_q.Exists()) {
            ctx.Flag(scope, "Missing active page key: " + active_page_key);
        } else {
            const UiNodeInfo page_info = page_q.One();
            if (!IsFiniteRect(page_info.bounds) || !page_info.participates_in_layout) {
                ctx.Flag(scope, "Active page does not participate in layout: " + active_page_key);
            } else {
                active_page_bounds = page_info.bounds;
                if (page_info.bounds.x < 210.0F ||
                    page_info.bounds.x + page_info.bounds.width > viewport_w + 0.5F ||
                    page_info.bounds.y + page_info.bounds.height > viewport_h + 0.5F ||
                    page_info.bounds.width < 600.0F || page_info.bounds.height < 200.0F) {
                    ctx.Flag(scope, "Active page out of expected workspace bounds (" + active_page_key + "): " + FmtRect(page_info.bounds));
                }
            }

            if (has_split) {
                const auto left_matches = page_q.Find(UiSelector::Key("split-left")).All();
                const auto right_matches = page_q.Find(UiSelector::Key("split-right")).All();
                if (left_matches.empty() || right_matches.empty()) {
                    ctx.Flag(scope, "SplitLayout panes missing in " + active_page_key);
                } else {
                    const UiNodeInfo& left_info = left_matches.front();
                    const UiNodeInfo& right_info = right_matches.front();
                    if (!left_info.participates_in_layout || !right_info.participates_in_layout) {
                        ctx.Flag(scope, "Split pane not participating in layout in " + active_page_key);
                    } else {
                        if (left_info.bounds.width < 200.0F || left_info.bounds.height < 200.0F ||
                            right_info.bounds.width < 200.0F || right_info.bounds.height < 200.0F) {
                            ctx.Flag(scope, "Collapsed split pane in " + active_page_key +
                                                " left=" + FmtRect(left_info.bounds) +
                                                " right=" + FmtRect(right_info.bounds));
                        }
                        if (left_info.bounds.x + left_info.bounds.width > right_info.bounds.x + 1.5F) {
                            ctx.Flag(scope, "Split panes overlap horizontally: left=" + FmtRect(left_info.bounds) +
                                                " right=" + FmtRect(right_info.bounds));
                        }
                        if (right_info.bounds.x + right_info.bounds.width > viewport_w + 1.0F) {
                            ctx.Flag(scope, "Split right pane overflows viewport: " + FmtRect(right_info.bounds));
                        }
                        CheckSplitPaneNodes(ctx, scope, page_q, left_info.bounds, right_info.bounds);
                    }
                }
            }
        }
    }

    // 3. Global participating node geometry & horizontal overflow / clipping
    auto check_global_type = [&](const std::string& type_name, UiSelector sel) {
        for (const auto& n : ui.Find(std::move(sel)).All()) {
            if (!n.participates_in_layout) continue;
            if (!IsFiniteRect(n.bounds) || !IsFiniteRect(n.visible_bounds)) {
                ctx.Flag(scope, "Non-finite " + type_name + " bounds: " + FmtRect(n.bounds));
                continue;
            }
            if (!n.in_viewport) continue;
            if (n.bounds.x < -1.0F || n.bounds.x + n.bounds.width > viewport_w + 1.0F) {
                std::string extra = n.text.has_value() ? (" text=\"" + n.text->substr(0, 48) + "\"") : "";
                ctx.Flag(scope, type_name + " horizontal viewport overflow" + extra +
                                    " bounds=" + FmtRect(n.bounds));
            }
            if (type_name == "Text") {
                if (n.text.has_value() && !n.text->empty()) {
                    if (n.bounds.width <= 0.5F || n.bounds.height <= 0.5F) {
                        ctx.Flag(scope, "Zero-sized Text \"" + n.text->substr(0, 48) + "\" bounds=" + FmtRect(n.bounds));
                    } else if (n.visible_bounds.width + 2.0F < n.bounds.width) {
                        ctx.Flag(scope, "Horizontally clipped Text \"" + n.text->substr(0, 48) + "\" bounds=" +
                                            FmtRect(n.bounds) + " visible=" + FmtRect(n.visible_bounds));
                    }
                }
            } else if (type_name == "TextField") {
                if (n.bounds.width < 16.0F || n.bounds.height < 16.0F) {
                    ctx.Flag(scope, "Collapsed TextField bounds=" + FmtRect(n.bounds));
                } else if (n.visible_bounds.width + 2.0F < n.bounds.width) {
                    ctx.Flag(scope, "Horizontally clipped TextField bounds=" + FmtRect(n.bounds) +
                                        " visible=" + FmtRect(n.visible_bounds));
                }
            }
        }
    };

    check_global_type("Text", UiSelector::Type<Text>());
    check_global_type("TextField", UiSelector::Type<TextField>());
    check_global_type("Checkbox", UiSelector::Type<Checkbox>());
    check_global_type("Switch", UiSelector::Type<Switch>());
    check_global_type("Image", UiSelector::Type<Image>());

    // 4. Button semantics bounds & overlap check
    const Rect viewport_rect{0.0F, 0.0F, viewport_w, viewport_h};
    struct BtnBox {
        std::string label;
        Rect rect;
    };
    std::vector<BtnBox> visible_buttons;
    const auto all_buttons = ui.FindSemantics(UiSemanticSelector::Role(SemanticRole::Button)).All();
    for (const auto& btn : all_buttons) {
        if (!IsFiniteRect(btn.bounds)) {
            ctx.Flag(scope, "Button '" + btn.label + "' has non-finite bounds: " + FmtRect(btn.bounds));
            continue;
        }
        if (btn.bounds.IsEmpty() || btn.offscreen) continue;
        const Rect vis = btn.bounds.Intersection(viewport_rect);
        if (vis.IsEmpty()) continue;

        if (btn.bounds.x < -1.0F || btn.bounds.x + btn.bounds.width > viewport_w + 1.0F) {
            ctx.Flag(scope, "Button '" + btn.label + "' overflows viewport horizontally: bounds=" + FmtRect(btn.bounds));
        }
        if (btn.bounds.width < 16.0F || btn.bounds.height < 16.0F) {
            ctx.Flag(scope, "Collapsed Button '" + btn.label + "' bounds=" + FmtRect(btn.bounds));
        }
        if (vis.width + 2.0F < btn.bounds.width) {
            ctx.Flag(scope, "Button '" + btn.label + "' horizontally clipped by viewport: bounds=" + FmtRect(btn.bounds));
        }
        Rect effective = vis;
        if (active_page_bounds.has_value() &&
            btn.bounds.x >= active_page_bounds->x - 1.0F &&
            btn.bounds.y >= active_page_bounds->y - 1.0F &&
            btn.bounds.y < active_page_bounds->y + active_page_bounds->height) {
            effective = effective.Intersection(*active_page_bounds);
            if (effective.IsEmpty()) continue;
        }
        visible_buttons.push_back({btn.label, effective});
    }

    if (!modal_open) {
        auto contains = [](const Rect& outer, const Rect& inner) {
            return inner.x >= outer.x - 0.5F &&
                   inner.y >= outer.y - 0.5F &&
                   inner.x + inner.width <= outer.x + outer.width + 0.5F &&
                   inner.y + inner.height <= outer.y + outer.height + 0.5F;
        };
        for (std::size_t i = 0; i < visible_buttons.size(); ++i) {
            for (std::size_t j = i + 1; j < visible_buttons.size(); ++j) {
                const Rect inter = visible_buttons[i].rect.Intersection(visible_buttons[j].rect);
                if (inter.width * inter.height > 4.0F) {
                    if (contains(visible_buttons[i].rect, visible_buttons[j].rect) ||
                        contains(visible_buttons[j].rect, visible_buttons[i].rect)) {
                        ctx.Flag(scope, "Nested buttons detected: '" + visible_buttons[i].label + "' " +
                                            FmtRect(visible_buttons[i].rect) + " and '" +
                                            visible_buttons[j].label + "' " + FmtRect(visible_buttons[j].rect));
                    } else {
                        ctx.Flag(scope, "Overlapping buttons: '" + visible_buttons[i].label + "' " +
                                            FmtRect(visible_buttons[i].rect) + " vs '" +
                                            visible_buttons[j].label + "' " + FmtRect(visible_buttons[j].rect) +
                                            " inter=" + FmtRect(inter));
                    }
                }
            }
        }
    }

    // 5. Snapshot sanity
    CheckSnapshotSanity(ctx, scope, ui);
}

bool SafeTap(AuditContext& ctx, const std::string& scope, const UiNodeQuery& q, const std::string& desc) {
    try {
        q.Tap();
        return true;
    } catch (const UiTestFailure& ex) {
        ctx.Flag(scope, "Tap failed for " + desc + ": " + ex.what());
        return false;
    }
}

void AuditAllPagesAtViewport(
    AuditContext& ctx,
    UiTest& ui,
    const std::shared_ptr<XlingsStore>& store,
    float w,
    float h
) {
    const std::string vp_tag = std::to_string(static_cast<int>(w)) + "x" + std::to_string(static_cast<int>(h));
    ui.SetWindowMetrics({.viewport = {w, h}});
    ui.Pump();

    // Dashboard
    SafeTap(ctx, vp_tag + " / Dashboard", ui.Find(UiSelector::Key("nav-dashboard")), "nav-dashboard");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "Dashboard", "page-dashboard");

    // Dashboard + Expanded TaskConsole + Active Task Progress
    if (SafeTap(ctx, vp_tag + " / Dashboard", ui.Find(UiSelector::Text("展开日志")), "展开日志")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "Dashboard + ExpandedConsole", "page-dashboard");

        store->task_running = true;
        store->task_title = "清理旧版本 xlings (13 个版本)";
        store->task_progress = 0.45F;
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "Dashboard + RunningTaskConsole", "page-dashboard");

        store->task_running = false;
        store->task_progress = std::optional<float>{};
        ui.Pump();
        SafeTap(ctx, vp_tag + " / Dashboard", ui.Find(UiSelector::Text("收起日志")), "收起日志");
        ui.Pump();
    }

    // Packages (with split view)
    SafeTap(ctx, vp_tag + " / Packages", ui.Find(UiSelector::Key("nav-packages")), "nav-packages");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "Packages", "page-packages", true);

    if (SafeTap(ctx, vp_tag + " / BatchPruneDialog", ui.Find(UiSelector::Text("批量清理")), "批量清理")) {
        ui.Pump(500ms);
        ui.Pump();
        const auto dlg_nodes = ui.Find(UiSelector::Text("批量清理过期版本")).All();
        const bool dlg_visible = std::ranges::any_of(
            dlg_nodes,
            [](const UiNodeInfo& n) { return n.participates_in_layout && n.in_viewport; }
        );
        if (!dlg_visible) {
            ctx.Flag(vp_tag + " / BatchPruneDialog", "BatchPruneDialog content not visible in viewport");
        }
        AuditCurrentScreen(ctx, ui, w, h, "BatchPruneDialog", "page-packages", false, true);

        const auto exec_btn_sel = UiSemanticSelector::AllOf(
            UiSemanticSelector::Role(SemanticRole::Button),
            UiSemanticSelector::Label("执行批量清理")
        );
        const auto exec_sem = ui.FindSemantics(exec_btn_sel);
        if (!exec_sem.Exists() || !exec_sem.One().enabled) {
            ctx.Flag(vp_tag + " / BatchPruneDialog",
                     "执行批量清理 should be enabled by default even when a protected package is present");
        }
        if (SafeTap(ctx, vp_tag + " / BatchPruneDialog", ui.Find(UiSelector::Text("全不选")), "全不选")) {
            ui.Pump();
            if (!ui.FindSemantics(exec_btn_sel).Exists() || ui.FindSemantics(exec_btn_sel).One().enabled) {
                ctx.Flag(vp_tag + " / BatchPruneDialog", "执行批量清理 should be disabled after 全不选");
            }
        }
        if (SafeTap(ctx, vp_tag + " / BatchPruneDialog", ui.Find(UiSelector::Text("全选")), "全选")) {
            ui.Pump();
            if (!ui.FindSemantics(exec_btn_sel).Exists() || !ui.FindSemantics(exec_btn_sel).One().enabled) {
                ctx.Flag(vp_tag + " / BatchPruneDialog", "执行批量清理 should be re-enabled after 全选");
            }
        }

        if (SafeTap(ctx, vp_tag + " / BatchPruneDialog", ui.Find(UiSelector::Text("取消")), "取消(BatchPruneDialog)")) {
            try {
                ui.PumpUntil(
                    [&] {
                        return !ui.Find(UiSelector::Text("执行批量清理")).Exists() &&
                               std::ranges::none_of(
                                   ui.Find(UiSelector::Text("批量清理过期版本")).All(),
                                   [](const UiNodeInfo& n) { return n.participates_in_layout; }
                               );
                    },
                    {.timeout = 2s, .step = 50ms}
                );
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / BatchPruneDialog", std::string("BatchPruneDialog still mounted after cancel: ") + ex.what());
            }
        }
    }

    if (SafeTap(ctx, vp_tag + " / BatchUpdateDialog", ui.Find(UiSelector::Text("批量更新")), "批量更新")) {
        ui.Pump(500ms);
        ui.Pump();
        const auto upd_nodes = ui.Find(UiSelector::Text("批量更新软件包")).All();
        const bool upd_visible = std::ranges::any_of(
            upd_nodes,
            [](const UiNodeInfo& n) { return n.participates_in_layout && n.in_viewport; }
        );
        if (!upd_visible) {
            ctx.Flag(vp_tag + " / BatchUpdateDialog", "BatchUpdateDialog content not visible in viewport");
        }
        AuditCurrentScreen(ctx, ui, w, h, "BatchUpdateDialog", "page-packages", false, true);

        const auto upd_exec_sel = UiSemanticSelector::AllOf(
            UiSemanticSelector::Role(SemanticRole::Button),
            UiSemanticSelector::Label("执行批量更新")
        );
        const auto upd_exec_sem = ui.FindSemantics(upd_exec_sel);
        if (!upd_exec_sem.Exists() || !upd_exec_sem.One().enabled) {
            ctx.Flag(vp_tag + " / BatchUpdateDialog",
                     "执行批量更新 should be enabled by default even when a skipped package is present");
        }
        if (SafeTap(ctx, vp_tag + " / BatchUpdateDialog", ui.Find(UiSelector::Text("全不选")), "全不选(BatchUpdateDialog)")) {
            ui.Pump();
            if (!ui.FindSemantics(upd_exec_sel).Exists() || ui.FindSemantics(upd_exec_sel).One().enabled) {
                ctx.Flag(vp_tag + " / BatchUpdateDialog", "执行批量更新 should be disabled after 全不选");
            }
        }
        if (SafeTap(ctx, vp_tag + " / BatchUpdateDialog", ui.Find(UiSelector::Text("全选")), "全选(BatchUpdateDialog)")) {
            ui.Pump();
            if (!ui.FindSemantics(upd_exec_sel).Exists() || !ui.FindSemantics(upd_exec_sel).One().enabled) {
                ctx.Flag(vp_tag + " / BatchUpdateDialog", "执行批量更新 should be re-enabled after 全选");
            }
        }

        if (SafeTap(ctx, vp_tag + " / BatchUpdateDialog", ui.Find(UiSelector::Text("取消")), "取消(BatchUpdateDialog)")) {
            try {
                ui.PumpUntil(
                    [&] {
                        return !ui.Find(UiSelector::Text("执行批量更新")).Exists() &&
                               std::ranges::none_of(
                                   ui.Find(UiSelector::Text("批量更新软件包")).All(),
                                   [](const UiNodeInfo& n) { return n.participates_in_layout; }
                               );
                    },
                    {.timeout = 2s, .step = 50ms}
                );
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / BatchUpdateDialog", std::string("BatchUpdateDialog still mounted after cancel: ") + ex.what());
            }
        }
    }

    // Store (with split view)
    SafeTap(ctx, vp_tag + " / Store", ui.Find(UiSelector::Key("nav-store")), "nav-store");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "Store", "page-store", true);

    // SubOS (all 4 tabs, with split view)
    SafeTap(ctx, vp_tag + " / SubOS", ui.Find(UiSelector::Key("nav-subos")), "nav-subos");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "SubOS[Shims]", "page-subos", true);

    if (SafeTap(ctx, vp_tag + " / SubOS[EnvVars]", ui.Find(UiSelector::Text("环境变量")), "环境变量")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "SubOS[EnvVars]", "page-subos", true);
    }

    if (SafeTap(ctx, vp_tag + " / SubOS[Exec]", ui.Find(UiSelector::Text("执行命令")), "执行命令")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "SubOS[Exec]", "page-subos", true);
    }

    if (SafeTap(ctx, vp_tag + " / SubOS[Events]", ui.Find(UiSelector::Text("审计事件")), "审计事件")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "SubOS[Events]", "page-subos", true);
    }

    if (SafeTap(ctx, vp_tag + " / SubOS[Shims]", ui.Find(UiSelector::Text("Shims")), "Shims")) {
        ui.Pump();
    }

    // New SubOS modal dialog
    if (SafeTap(ctx, vp_tag + " / NewSubosDialog", ui.Find(UiSelector::Text("新建 SubOS")), "新建 SubOS")) {
        ui.Pump(500ms);
        ui.Pump();
        auto dlg_title = ui.Find(UiSelector::Text("新建 SubOS 子系统"));
        if (!dlg_title.Exists() || !dlg_title.One().in_viewport) {
            ctx.Flag(vp_tag + " / NewSubosDialog", "NewSubosDialog content not visible in viewport");
        }
        AuditCurrentScreen(ctx, ui, w, h, "NewSubosDialog", "page-subos", false, true);
        if (SafeTap(ctx, vp_tag + " / NewSubosDialog", ui.Find(UiSelector::Text("取消")), "取消(NewSubosDialog)")) {
            try {
                ui.PumpUntil([&] { return !ui.Find(UiSelector::Text("新建 SubOS 子系统")).Exists(); },
                             {.timeout = 2s, .step = 50ms});
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / NewSubosDialog", std::string("NewSubosDialog still mounted after cancel: ") + ex.what());
            }
        }
    }

    // Indexes + AddRepo dialog
    SafeTap(ctx, vp_tag + " / Indexes", ui.Find(UiSelector::Key("nav-indexes")), "nav-indexes");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "Indexes", "page-indexes");

    if (SafeTap(ctx, vp_tag + " / AddRepoDialog", ui.Find(UiSelector::Text("添加 / 更新仓库")), "添加 / 更新仓库")) {
        ui.Pump(500ms);
        ui.Pump();
        auto dlg_title = ui.Find(UiSelector::Text("添加或更新索引仓库"));
        if (!dlg_title.Exists() || !dlg_title.One().in_viewport) {
            ctx.Flag(vp_tag + " / AddRepoDialog", "AddRepoDialog content not visible in viewport");
        }
        AuditCurrentScreen(ctx, ui, w, h, "AddRepoDialog", "page-indexes", false, true);
        if (SafeTap(ctx, vp_tag + " / AddRepoDialog", ui.Find(UiSelector::Text("取消")), "取消(AddRepoDialog)")) {
            try {
                ui.PumpUntil([&] { return !ui.Find(UiSelector::Text("添加或更新索引仓库")).Exists(); },
                             {.timeout = 2s, .step = 50ms});
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / AddRepoDialog", std::string("AddRepoDialog still mounted after cancel: ") + ex.what());
            }
        }
    }

    // Settings (3 tabs: App Settings, xlings Config, Diagnostics & Maintenance)
    SafeTap(ctx, vp_tag + " / Settings", ui.Find(UiSelector::Key("nav-settings")), "nav-settings");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "Settings[App]", "page-settings");

    if (!ui.Find(UiSelector::Key("settings-lightweight-switch")).Exists()) {
        ctx.Flag(vp_tag + " / Settings[App]", "Missing settings-lightweight-switch");
    }
    if (!ui.Find(UiSelector::Key("settings-about-card")).Exists()) {
        ctx.Flag(vp_tag + " / Settings[App]", "Missing settings-about-card");
    }
    if (SafeTap(ctx, vp_tag + " / ThemeDark", ui.Find(UiSelector::Key("theme-mode-dark")), "theme-mode-dark")) {
        ui.Pump(550ms);
        ui.Pump();
        if (store->theme_mode.Get() != ThemeMode::Dark) {
            ctx.Flag(vp_tag + " / ThemeDark", "Clicking theme-mode-dark did not activate ThemeMode::Dark");
        }
    }
    if (SafeTap(ctx, vp_tag + " / ThemeLight", ui.Find(UiSelector::Key("theme-mode-light")), "theme-mode-light")) {
        ui.Pump(550ms);
        ui.Pump();
        if (store->theme_mode.Get() != ThemeMode::Light) {
            ctx.Flag(vp_tag + " / ThemeLight", "Clicking theme-mode-light did not activate ThemeMode::Light");
        }
    }
    if (SafeTap(ctx, vp_tag + " / ThemeAuto", ui.Find(UiSelector::Key("theme-mode-auto")), "theme-mode-auto")) {
        ui.Pump(550ms);
        ui.Pump();
        if (store->theme_mode.Get() != ThemeMode::Auto) {
            ctx.Flag(vp_tag + " / ThemeAuto", "Clicking theme-mode-auto did not activate ThemeMode::Auto");
        }
    }
    if (SafeTap(ctx, vp_tag + " / SettingsShortcuts", ui.Find(UiSelector::Key("settings-open-shortcuts")), "settings-open-shortcuts")) {
        ui.Pump(500ms);
        ui.Pump();
        if (!ui.Find(UiSelector::Key("shortcuts-dialog")).Exists()) {
            ctx.Flag(vp_tag + " / SettingsShortcuts", "Clicking settings-open-shortcuts did not open shortcuts-dialog");
        } else {
            AuditCurrentScreen(ctx, ui, w, h, "Settings[ShortcutsDialog]", "page-settings", false, true);
            ui.PressKey(Key::Escape);
            try {
                ui.PumpUntil([&] { return !ui.Find(UiSelector::Key("shortcuts-dialog")).Exists(); },
                             {.timeout = 2s, .step = 50ms});
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / SettingsShortcuts", std::string("Escape did not close shortcuts-dialog: ") + ex.what());
            }
        }
    }

    if (SafeTap(ctx, vp_tag + " / Settings[XlingsConfig]", ui.Find(UiSelector::Text("xlings 全局配置")), "xlings 全局配置")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "Settings[XlingsConfig]", "page-settings");
    }

    if (SafeTap(ctx, vp_tag + " / Settings[Maintenance]", ui.Find(UiSelector::Text("环境诊断与维护")), "环境诊断与维护")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "Settings[Maintenance]", "page-settings");
    }

    SafeTap(ctx, vp_tag + " / Settings[AppReset]", ui.Find(UiSelector::Text("Hxlings 应用设置")), "Hxlings 应用设置");
    ui.Pump();

    // Dark mode check on Dashboard & Settings
    store->SetDarkMode(true);
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "Settings(Dark)", "page-settings");
    SafeTap(ctx, vp_tag + " / Dashboard(Dark)", ui.Find(UiSelector::Key("nav-dashboard")), "nav-dashboard");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "Dashboard(Dark)", "page-dashboard");

    store->SetDarkMode(false);
    ui.Pump();
}

void TestDuplicateDataNoDuplicateKeys(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    try {
        PkgGroup dup_group{
            .key = "xim:dup-pkg",
            .ns = "xim",
            .name = "dup-pkg",
            .description = "duplicate group regression entry",
            .active_version = "1.0.0",
            .versions = {
                InstalledPkg{.raw_spec = "xim:dup-pkg@1.0.0", .ns = "xim", .name = "dup-pkg", .version = "1.0.0", .is_active = true},
            },
        };
        store->installed_groups = {dup_group, dup_group};
        store->selected_installed_key = "xim:dup-pkg";
        store->selected_installed_detail = PackageDetail{
            .title = "xim:dup-pkg",
            .package = "xim:dup-pkg",
            .ns = "xim",
            .name = "dup-pkg",
            .description = "duplicate detail versions",
            .status = "stable",
            .selected_version = "1.0.0",
            .available_versions = {"1.0.0", "1.0.0", "0.9.0"},
        };

        SearchPkg dup_search{
            .package = "xim:dup-search",
            .ns = "xim",
            .name = "dup-search",
            .version = "2.0.0",
            .description = "duplicate search result",
            .status = "stable",
            .index = "xim",
        };
        store->search_results = {dup_search, dup_search};
        store->selected_store_pkg = "xim:dup-search";
        store->selected_store_detail = PackageDetail{
            .title = "xim:dup-search",
            .package = "xim:dup-search",
            .ns = "xim",
            .name = "dup-search",
            .description = "duplicate store versions",
            .status = "stable",
            .selected_version = "2.0.0",
            .available_versions = {"2.0.0", "2.0.0", "1.0.0"},
        };

        SubosEntry dup_subos{
            .name = "default",
            .dir = "/home/farna/.xlings/subos/default",
            .abi = "native",
            .kind = "view",
            .view = "overlay",
            .active = true,
            .packages = 10,
            .commands = 5,
        };
        store->subos_list = {dup_subos, dup_subos};
        store->selected_subos = "default";
        store->active_shims = {"gcc", "gcc", "clang"};

        IndexSnapshot dup_snap{
            .index_version = "bafdb27",
            .generated_at = "2026-11-22T07:36:45Z",
            .artifact_name = "xim-bafdb27.tar.gz",
            .size_bytes = 1024ULL,
            .current = true,
            .installed = true,
        };
        IndexRepoVersions dup_repo{
            .name = "xim",
            .current = "bafdb27",
            .installed = "bafdb27",
            .snapshots = {dup_snap, dup_snap},
        };
        store->index_versions = {dup_repo, dup_repo};

        for (const char* nav_key : {"nav-packages", "nav-store", "nav-subos", "nav-indexes"}) {
            if (SafeTap(ctx, "DuplicateKeysRegression", ui.Find(UiSelector::Key(nav_key)), nav_key)) {
                ui.Pump();
            }
        }
    } catch (const std::exception& ex) {
        ctx.Flag("DuplicateKeysRegression", std::string("Exception during duplicate data recomposition: ") + ex.what());
    }
}

void TestCrossPlatformShortcuts(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    ui.SetWindowMetrics({.viewport = {1180.0F, 760.0F}});
    store->active_nav = 0;
    store->SetDarkMode(false);
    store->console_expanded = false;
    ui.Pump();

    ui.PressKey(Key::Digit2, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 1) {
        ctx.Flag("Shortcuts", "Primary+2 did not switch to Packages page");
    }

    ui.PressKey(Key::Digit3, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 2) {
        ctx.Flag("Shortcuts", "Primary+3 did not switch to Store page");
    }

    ui.PressKey(Key::Comma, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 5) {
        ctx.Flag("Shortcuts", "Primary+Comma did not switch to Settings page");
    }

    ui.PressKey(Key::K, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 2) {
        ctx.Flag("Shortcuts", "Primary+K did not switch to Store page");
    }
    const auto search_box = ui.Find(UiSelector::Key("store-search-input"));
    if (!search_box.Exists() || !search_box.One().focused) {
        ctx.Flag("Shortcuts", "Primary+K did not focus store-search-input");
    }

    ui.PressKey(Key::J, PrimaryModifier());
    ui.Pump();
    if (!store->console_expanded.Get()) {
        ctx.Flag("Shortcuts", "Primary+J did not expand task console");
    }
    ui.PressKey(Key::J, PrimaryModifier());
    ui.Pump();
    if (store->console_expanded.Get()) {
        ctx.Flag("Shortcuts", "Second Primary+J did not collapse task console");
    }

    ui.PressKey(Key::D, PrimaryShiftModifier());
    ui.Pump(300ms);
    ui.Pump();
    if (!store->dark_mode.Get()) {
        ctx.Flag("Shortcuts", "Primary+Shift+D did not enable dark mode");
    }
    ui.PressKey(Key::D, PrimaryShiftModifier());
    ui.Pump(300ms);
    ui.Pump();
    if (store->dark_mode.Get()) {
        ctx.Flag("Shortcuts", "Second Primary+Shift+D did not restore light mode");
    }

    ui.PressKey(Key::Slash, PrimaryModifier());
    ui.Pump(500ms);
    ui.Pump();
    if (!ui.Find(UiSelector::Key("shortcuts-dialog")).Exists()) {
        ctx.Flag("Shortcuts", "Primary+/ did not open shortcuts-dialog");
    } else {
        AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "ShortcutsDialog", "page-store", false, true);
        ui.PressKey(Key::Escape);
        try {
            ui.PumpUntil([&] { return !ui.Find(UiSelector::Key("shortcuts-dialog")).Exists(); },
                         {.timeout = 2s, .step = 50ms});
        } catch (const UiTestFailure& ex) {
            ctx.Flag("Shortcuts", std::string("Escape did not close shortcuts-dialog: ") + ex.what());
        }
    }

    ui.PressKey(Key::U, PrimaryShiftModifier());
    ui.Pump(500ms);
    ui.Pump();
    const bool upd_open = std::ranges::any_of(
        ui.Find(UiSelector::Text("批量更新软件包")).All(),
        [](const UiNodeInfo& n) { return n.participates_in_layout && n.in_viewport; }
    );
    if (store->active_nav.Get() != 1 || !upd_open) {
        ctx.Flag("Shortcuts", "Primary+Shift+U did not open BatchUpdateDialog on Packages page");
    } else {
        ui.PressKey(Key::Escape);
        try {
            ui.PumpUntil(
                [&] {
                    return !ui.Find(UiSelector::Text("执行批量更新")).Exists() &&
                           std::ranges::none_of(
                               ui.Find(UiSelector::Text("批量更新软件包")).All(),
                               [](const UiNodeInfo& n) { return n.participates_in_layout; }
                           );
                },
                {.timeout = 2s, .step = 50ms}
            );
        } catch (const UiTestFailure& ex) {
            ctx.Flag("Shortcuts", std::string("Escape did not close BatchUpdateDialog: ") + ex.what());
        }
    }
}

void TestMacOsTitleBarMetrics(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    store->active_nav = 0;
    for (const auto [w, h] : std::array<std::pair<float, float>, 2>{{{1180.0F, 760.0F}, {880.0F, 560.0F}}}) {
        ui.SetWindowMetrics({
            .viewport = {w, h},
            .title_bar = WindowTitleBarMetrics{
                .height = 44.0F,
                .left_inset = 78.0F,
                .right_inset = 0.0F,
                .maximized = false,
            },
        });
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "MacTitleBar", "page-dashboard");
    }
    ui.SetWindowMetrics({.viewport = {1180.0F, 760.0F}});
    ui.Pump();
}

void TestStorePaginationAndErrorStates(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    ui.SetWindowMetrics({.viewport = {1180.0F, 760.0F}});
    store->active_nav = 2;
    const auto saved_results = store->search_results.Get();
    const auto saved_error = store->search_error.Get();

    std::vector<SearchPkg> many;
    many.reserve(80);
    for (int i = 0; i < 80; ++i) {
        many.push_back(SearchPkg{
            .package = "xim:pkg-" + std::to_string(i),
            .ns = "xim",
            .name = "pkg-" + std::to_string(i),
            .version = "1.0." + std::to_string(i),
            .description = "Sample package description #" + std::to_string(i),
            .status = "stable",
            .index = "xim",
        });
    }
    store->search_error = std::string{};
    store->search_results = std::move(many);
    ui.Pump();
    AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "Store[80Results]", "page-store", true);

    if (!ui.Find(UiSelector::Key("store-load-more")).Exists()) {
        ctx.Flag("StorePagination", "Missing store-load-more button when 80 results are present");
    }

    store->search_results = std::vector<SearchPkg>{};
    store->search_error = std::string("search failed: index not synced");
    ui.Pump();
    if (!ui.Find(UiSelector::Text("搜索软件包失败")).Exists()) {
        ctx.Flag("StoreError", "Store error state message not rendered");
    }
    AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "Store[ErrorState]", "page-store", true);

    store->search_error = saved_error;
    store->search_results = saved_results;
    ui.Pump();
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path res_root = ResolveResourcesRoot(argc > 0 ? argv[0] : nullptr);
    auto resources = std::make_shared<DeployedResources>(res_root);
    std::shared_ptr<XlingsStore> store;

    Application test_app(
        App,
        {
            .window = {
                .title = "Hxlings — xlings Manager",
                .initial_size = {1180.0F, 760.0F},
                .minimum_size = Size{880.0F, 560.0F},
                .chrome_mode = WindowChromeMode::Custom,
                .title_bar_height = 44.0F,
            },
            .show_debug_overlay = false,
            .application_hooks = {
                [&store](ApplicationContext& context) {
                    store = MakeSeededStore();
                    context.Provide(store);
                },
            },
        }
    );

    UiTest ui(test_app, {
        .viewport = {1180.0F, 760.0F},
        .resource_provider = resources,
    });
    ui.Pump();

    AuditContext ctx;
    AuditAllPagesAtViewport(ctx, ui, store, 1180.0F, 760.0F);
    AuditAllPagesAtViewport(ctx, ui, store, 880.0F, 560.0F);

    // Spot check 1440x900
    ui.SetWindowMetrics({.viewport = {1440.0F, 900.0F}});
    ui.Pump();
    SafeTap(ctx, "1440x900 / Dashboard", ui.Find(UiSelector::Key("nav-dashboard")), "nav-dashboard");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, 1440.0F, 900.0F, "Dashboard", "page-dashboard");
    SafeTap(ctx, "1440x900 / Packages", ui.Find(UiSelector::Key("nav-packages")), "nav-packages");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, 1440.0F, 900.0F, "Packages", "page-packages", true);

    TestCrossPlatformShortcuts(ctx, ui, store);
    TestMacOsTitleBarMetrics(ctx, ui, store);
    TestStorePaginationAndErrorStates(ctx, ui, store);
    TestDuplicateDataNoDuplicateKeys(ctx, ui, store);

    if (ctx.anomalies.empty()) {
        std::cout << "UI layout test passed with 0 anomalies.\n";
        return 0;
    }

    std::cerr << "Detected " << ctx.anomalies.size() << " UI layout anomaly(ies):\n";
    for (std::size_t i = 0; i < ctx.anomalies.size(); ++i) {
        std::cerr << "  " << (i + 1) << ". " << ctx.anomalies[i] << '\n';
    }
    return 1;
}

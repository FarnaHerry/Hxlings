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
import hxlings.theme;
import hxlings.shortcuts;
import hxlings.settings;
import hxlings.store;
import xlings.types;
import xlings.parser;
import hxlings.updater;
import hxlings.luban.types;

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
        .persist = "rootfs",
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
        .persist = "rootfs",
        .shell = "bash",
        .policy_extends = "dev",
        .net = "nat",
        .observe_level = "standard",
        .fetch_policy = "auto",
    };
    store->selected_luban_history = std::string("generation 1 (active)");
    store->selected_luban_logs = std::string("[2026-11-24] [luban] dev-core ready");

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
    const std::array<const char*, 7> nav_keys{
        "nav-dashboard", "nav-packages", "nav-store", "nav-subos", "nav-luban", "nav-indexes", "nav-settings"
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

    // SubOS (all 6 tabs, with split view + Diff / Export modals)
    SafeTap(ctx, vp_tag + " / SubOS", ui.Find(UiSelector::Key("nav-subos")), "nav-subos");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "SubOS[Shims]", "page-subos", true);

    if (SafeTap(ctx, vp_tag + " / SubOS[EnvVars]", ui.Find(UiSelector::Text("环境")), "环境")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "SubOS[EnvVars]", "page-subos", true);
    }

    if (SafeTap(ctx, vp_tag + " / SubOS[Exec]", ui.Find(UiSelector::Text("执行")), "执行")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "SubOS[Exec]", "page-subos", true);
    }

    if (SafeTap(ctx, vp_tag + " / SubOS[Policy]", ui.Find(UiSelector::Text("策略")), "策略")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "SubOS[Policy]", "page-subos", true);
    }

    if (SafeTap(ctx, vp_tag + " / SubOS[Sessions]", ui.Find(UiSelector::Text("会话")), "会话")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "SubOS[Sessions]", "page-subos", true);
    }

    if (SafeTap(ctx, vp_tag + " / SubOS[Events]", ui.Find(UiSelector::Text("审计")), "审计")) {
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

    // SubOS Diff modal dialog
    if (SafeTap(ctx, vp_tag + " / SubosDiffDialog", ui.Find(UiSelector::Text("对比 Diff")), "对比 Diff")) {
        ui.Pump(500ms);
        ui.Pump();
        if (!ui.Find(UiSelector::Key("subos-diff-dialog")).Exists()) {
            ctx.Flag(vp_tag + " / SubosDiffDialog", "SubosDiffDialog did not mount");
        }
        AuditCurrentScreen(ctx, ui, w, h, "SubosDiffDialog", "page-subos", false, true);
        if (SafeTap(ctx, vp_tag + " / SubosDiffDialog", ui.Find(UiSelector::Text("关闭")), "关闭(SubosDiffDialog)")) {
            try {
                ui.PumpUntil([&] { return !ui.Find(UiSelector::Key("subos-diff-dialog")).Exists(); },
                             {.timeout = 2s, .step = 50ms});
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / SubosDiffDialog", std::string("SubosDiffDialog still mounted after close: ") + ex.what());
            }
        }
    }

    // SubOS Export / Pack modal dialog
    if (SafeTap(ctx, vp_tag + " / SubosExportDialog", ui.Find(UiSelector::Text("导出 / 打包")), "导出 / 打包")) {
        ui.Pump(500ms);
        ui.Pump();
        if (!ui.Find(UiSelector::Key("subos-export-dialog")).Exists()) {
            ctx.Flag(vp_tag + " / SubosExportDialog", "SubosExportDialog did not mount");
        }
        AuditCurrentScreen(ctx, ui, w, h, "SubosExportDialog", "page-subos", false, true);
        if (SafeTap(ctx, vp_tag + " / SubosExportDialog", ui.Find(UiSelector::Text("取消")), "取消(SubosExportDialog)")) {
            try {
                ui.PumpUntil([&] { return !ui.Find(UiSelector::Key("subos-export-dialog")).Exists(); },
                             {.timeout = 2s, .step = 50ms});
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / SubosExportDialog", std::string("SubosExportDialog still mounted after cancel: ") + ex.what());
            }
        }
    }

    // LubanOS (all 3 outer tabs, 3 env detail tabs, and NewLubanEnv modal)
    store->luban_tab = 0;
    SafeTap(ctx, vp_tag + " / LubanOS", ui.Find(UiSelector::Key("nav-luban")), "nav-luban");
    ui.Pump();
    AuditCurrentScreen(ctx, ui, w, h, "LubanOS[Environments]", "page-luban", true);

    if (SafeTap(ctx, vp_tag + " / LubanOS[RunCmd]", ui.Find(UiSelector::Text("执行命令")), "执行命令")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "LubanOS[RunCmd]", "page-luban", true);
    }

    if (SafeTap(ctx, vp_tag + " / LubanOS[HistoryLogs]", ui.Find(UiSelector::Text("历史与日志")), "历史与日志")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "LubanOS[HistoryLogs]", "page-luban", true);
    }

    if (SafeTap(ctx, vp_tag + " / LubanOS[Overview]", ui.Find(UiSelector::Text("概览与配置")), "概览与配置")) {
        ui.Pump();
    }

    if (SafeTap(ctx, vp_tag + " / NewLubanEnvDialog", ui.Find(UiSelector::Text("新建环境")), "新建环境")) {
        ui.Pump(500ms);
        ui.Pump();
        if (!ui.Find(UiSelector::Key("new-luban-env-dialog")).Exists()) {
            ctx.Flag(vp_tag + " / NewLubanEnvDialog", "NewLubanEnvDialog did not mount");
        }
        AuditCurrentScreen(ctx, ui, w, h, "NewLubanEnvDialog", "page-luban", false, true);
        if (SafeTap(ctx, vp_tag + " / NewLubanEnvDialog", ui.Find(UiSelector::Text("取消")), "取消(NewLubanEnvDialog)")) {
            try {
                ui.PumpUntil([&] { return !ui.Find(UiSelector::Key("new-luban-env-dialog")).Exists(); },
                             {.timeout = 2s, .step = 50ms});
            } catch (const UiTestFailure& ex) {
                ctx.Flag(vp_tag + " / NewLubanEnvDialog",
                         std::string("NewLubanEnvDialog still mounted after cancel: ") + ex.what());
            }
        }
    }

    if (SafeTap(ctx, vp_tag + " / LubanOS[Catalog]", ui.Find(UiSelector::Text("发行版与桌面目录")), "发行版与桌面目录")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "LubanOS[Catalog]", "page-luban");
    }

    if (SafeTap(ctx, vp_tag + " / LubanOS[Stacks]", ui.Find(UiSelector::Text("蓝图编排")), "蓝图编排")) {
        ui.Pump();
        AuditCurrentScreen(ctx, ui, w, h, "LubanOS[Stacks]", "page-luban");
    }

    if (SafeTap(ctx, vp_tag + " / LubanOS[BackToEnvs]", ui.Find(UiSelector::Text("环境与机器")), "环境与机器")) {
        ui.Pump();
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
    store->last_opened_url = std::string{};
    try {
        auto scroll_q = ui.Find(UiSelector::Key("settings-scroll"));
        auto link_q = scroll_q.ScrollUntil(UiSelector::Key("about-link-hxlings"));
        ui.Pump();
        if (SafeTap(ctx, vp_tag + " / SettingsAboutLink", link_q, "about-link-hxlings")) {
            ui.Pump();
            if (store->last_opened_url.Get() != "https://github.com/FarnaHerry/Hxlings") {
                ctx.Flag(vp_tag + " / SettingsAboutLink",
                         "Clicking about-link-hxlings did not record GitHub URL (got '" +
                             store->last_opened_url.Get() + "')");
            }
        }
        auto mcpp_link_q = scroll_q.ScrollUntil(UiSelector::Key("about-link-mcpp"));
        ui.Pump();
        if (SafeTap(ctx, vp_tag + " / SettingsAboutMcppLink", mcpp_link_q, "about-link-mcpp")) {
            ui.Pump();
            if (store->last_opened_url.Get() != "https://github.com/mcpp-community/mcpp") {
                ctx.Flag(vp_tag + " / SettingsAboutMcppLink",
                         "Clicking about-link-mcpp did not record mcpp GitHub URL (got '" +
                             store->last_opened_url.Get() + "')");
            }
        }
        auto upd_btn_q = scroll_q.ScrollUntil(UiSelector::Key("about-check-update-btn"));
        ui.Pump();
        if (!ui.Find(UiSelector::Key("settings-app-update-box")).Exists()) {
            ctx.Flag(vp_tag + " / SettingsAboutUpdate", "Missing settings-app-update-box");
        }
        AuditCurrentScreen(ctx, ui, w, h, "Settings[AppAboutUpdate]", "page-settings");
        if (SafeTap(ctx, vp_tag + " / SettingsCheckUpdate", upd_btn_q, "about-check-update-btn")) {
            ui.Pump();
            if (store->app_update_state.Get().status_message.empty()) {
                ctx.Flag(vp_tag + " / SettingsCheckUpdate",
                         "Tapping about-check-update-btn in test mode did not update status_message");
            }
        }
        scroll_q.ScrollBy({0.0F, -2000.0F});
        ui.Pump();
    } catch (const UiTestFailure& ex) {
        ctx.Flag(vp_tag + " / SettingsAboutLink", std::string("ScrollUntil about-link-hxlings failed: ") + ex.what());
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

        LubanEnvironment dup_luban{
            .name = "dev-core",
            .kind = "rootfs",
            .edition = "core",
            .carrier = "bwrap",
            .shell = "bash",
            .packages = 18,
            .running = true,
        };
        store->luban_envs = {dup_luban, dup_luban};
        store->selected_luban_env = "dev-core";

        LubanCatalogItem dup_dist{
            .group = "distributions",
            .name = "core",
            .ref = "luban:core",
            .version = "2026.11",
            .status = "stable",
            .title_zh = "Luban Core",
            .summary_zh = "duplicate dist item",
            .tier = 1,
            .available = true,
        };
        LubanCatalogItem dup_desk{
            .group = "desktops",
            .name = "xfce",
            .ref = "luban:desktop-xfce",
            .version = "4.18",
            .status = "stable",
            .title_zh = "XFCE",
            .summary_zh = "duplicate desk item",
            .tier = 1,
            .available = true,
        };
        store->luban_catalog = LubanCatalog{
            .distributions = {dup_dist, dup_dist},
            .desktops = {dup_desk, dup_desk},
        };

        for (const char* nav_key : {"nav-packages", "nav-store", "nav-subos", "nav-luban", "nav-indexes"}) {
            if (SafeTap(ctx, "DuplicateKeysRegression", ui.Find(UiSelector::Key(nav_key)), nav_key)) {
                ui.Pump();
            }
        }
        store->luban_tab = 1;
        store->active_nav = 4;
        ui.Pump();
        store->luban_tab = 0;
        ui.Pump();
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

    ui.PressKey(Key::Digit5, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 4) {
        ctx.Flag("Shortcuts", "Primary+5 did not switch to LubanOS page");
    }

    ui.PressKey(Key::Digit6, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 5) {
        ctx.Flag("Shortcuts", "Primary+6 did not switch to Indexes page");
    }

    ui.PressKey(Key::Digit7, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 6) {
        ctx.Flag("Shortcuts", "Primary+7 did not switch to Settings page");
    }

    store->active_nav = 0;
    ui.Pump();
    ui.PressKey(Key::Comma, PrimaryModifier());
    ui.Pump();
    if (store->active_nav.Get() != 6) {
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

    if (ui.Find(UiSelector::Key("store-load-more")).Exists()) {
        ctx.Flag("StoreVirtualScroll", "Manual store-load-more button should not exist");
    }
    if (!ui.Find(UiSelector::Key("store-results-list")).Exists()) {
        ctx.Flag("StoreVirtualScroll", "Missing store-results-list virtual list");
    }
    if (!ui.Find(UiSelector::Key("search-xim:pkg-0-0")).Exists()) {
        ctx.Flag("StoreVirtualScroll", "Missing initial virtualized item search-xim:pkg-0-0");
    }
    if (ui.Find(UiSelector::Key("search-xim:pkg-79-79")).Exists()) {
        ctx.Flag("StoreVirtualScroll", "Tail item search-xim:pkg-79-79 should not be eagerly mounted before scrolling");
    }

    try {
        auto list_q = ui.Find(UiSelector::Key("store-results-list"));
        auto tail_q = list_q.ScrollUntil(
            UiSelector::Key("search-xim:pkg-79-79"),
            {.step = {0.0F, 420.0F}, .maximum_steps = 30}
        );
        ui.Pump();
        if (!tail_q.Exists() || !tail_q.One().in_viewport) {
            ctx.Flag("StoreVirtualScroll", "Scrolling store-results-list did not reveal search-xim:pkg-79-79");
        }
        AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "Store[80ResultsScrolled]", "page-store", true);
    } catch (const UiTestFailure& ex) {
        ctx.Flag("StoreVirtualScroll", std::string("ScrollUntil search-xim:pkg-79-79 failed: ") + ex.what());
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

void TestSearchBoxes(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    ui.SetWindowMetrics({.viewport = {1180.0F, 760.0F}});
    store->selected_subos = "default";
    ui.Pump();

    auto verify_search_box = [&](int nav_idx,
                                 const std::string& page_key,
                                 const std::string& input_key,
                                 bool has_split,
                                 const std::string& sample_text) {
        const std::string scope = "SearchBox[" + input_key + "]";
        store->active_nav = nav_idx;
        ui.Pump();

        if (ui.Find(UiSelector::Key("store-search-clear")).Exists()) {
            ctx.Flag(scope, "External store-search-clear button should not exist");
        }

        const auto field_q = ui.Find(UiSelector::Key(input_key));
        if (!field_q.Exists()) {
            ctx.Flag(scope, "Missing search input key: " + input_key);
            return;
        }
        if (field_q.One().value.value_or("") != "") {
            ctx.Flag(scope, "Expected initial empty value, got '" + field_q.One().value.value_or("") + "'");
        }

        const auto clear_q = ui.FindSemantics(UiSemanticSelector::AllOf(
            UiSemanticSelector::Role(SemanticRole::Button),
            UiSemanticSelector::Label("清空搜索")
        ));
        if (clear_q.Exists()) {
            ctx.Flag(scope, "Clear button semantic node should be hidden when input is empty");
        }

        field_q.Tap();
        field_q.EnterText(sample_text);
        if (field_q.One().value.value_or("") != sample_text) {
            ctx.Flag(scope, "EnterText failed: expected '" + sample_text + "', got '" +
                                field_q.One().value.value_or("") + "'");
            return;
        }
        if (clear_q.Count() != 1) {
            ctx.Flag(scope, "Expected 1 clear button semantic node after text entry, got " +
                                std::to_string(clear_q.Count()));
            return;
        }

        const Rect field_bounds = field_q.One().bounds;
        const Rect clear_bounds = clear_q.One().bounds;
        if (clear_bounds.x < field_bounds.x - 0.5F ||
            clear_bounds.y < field_bounds.y - 0.5F ||
            clear_bounds.x + clear_bounds.width > field_bounds.x + field_bounds.width + 0.5F ||
            clear_bounds.y + clear_bounds.height > field_bounds.y + field_bounds.height + 0.5F) {
            ctx.Flag(scope, "Clear button bounds " + FmtRect(clear_bounds) +
                                " not contained in search input bounds " + FmtRect(field_bounds));
        }
        if (clear_bounds.x < field_bounds.x + field_bounds.width * 0.5F) {
            ctx.Flag(scope, "Clear button bounds " + FmtRect(clear_bounds) +
                                " not on trailing half of " + FmtRect(field_bounds));
        }
        if (std::abs(clear_bounds.width - clear_bounds.height) >= 0.5F ||
            clear_bounds.width < 24.0F || clear_bounds.width > 30.0F) {
            ctx.Flag(scope, "Unexpected clear button action size: " + FmtRect(clear_bounds));
        }

        AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "SearchPopulated[" + input_key + "]", page_key, has_split);

        const Point clear_center{
            clear_bounds.x + clear_bounds.width * 0.5F,
            clear_bounds.y + clear_bounds.height * 0.5F,
        };
        ui.SendPointer(PointerEvent{
            .type = PointerEventType::Move,
            .pointer_id = 50,
            .position = {0.0F, 0.0F},
            .device_kind = PointerDeviceKind::Mouse,
        });
        ui.Pump();
        const UiSnapshot before_hover = ui.CaptureSnapshot();

        ui.SendPointer(PointerEvent{
            .type = PointerEventType::Move,
            .pointer_id = 50,
            .position = clear_center,
            .device_kind = PointerDeviceKind::Mouse,
        });
        ui.Pump();
        const UiSnapshot after_hover = ui.CaptureSnapshot();
        if (before_hover == after_hover) {
            ctx.Flag(scope, "Hovering internal clear button did not update visual snapshot");
        }

        ui.TapAt(clear_center, {.device_kind = PointerDeviceKind::Mouse, .pointer_id = 51});
        ui.Pump();
        ui.SendPointer(PointerEvent{
            .type = PointerEventType::Move,
            .pointer_id = 50,
            .position = {0.0F, 0.0F},
            .device_kind = PointerDeviceKind::Mouse,
        });
        ui.Pump();

        if (field_q.One().value.value_or("") != "") {
            ctx.Flag(scope, "Tapping internal clear button did not clear input text (got '" +
                                field_q.One().value.value_or("") + "')");
        }
        if (clear_q.Exists()) {
            ctx.Flag(scope, "Clear button semantic node still exists after clearing input");
        }

        AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "SearchCleared[" + input_key + "]", page_key, has_split);
    };

    verify_search_box(2, "page-store", "store-search-input", true, "gcc");
    verify_search_box(1, "page-packages", "packages-filter-input", true, "llvm");
    verify_search_box(3, "page-subos", "subos-shim-filter-input", true, "gcc");
    store->luban_tab = 0;
    verify_search_box(4, "page-luban", "luban-filter-input", true, "dev");

    store->SetThemeMode(ThemeMode::Dark);
    ui.Pump();
    verify_search_box(2, "page-store", "store-search-input", true, "rust");
    store->SetThemeMode(ThemeMode::Auto);
    ui.Pump();
}

void TestThemeAndDisabledControls(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    const auto srgb_channel = [](float c) -> float {
        return (c <= 0.04045F) ? (c / 12.92F) : std::pow((c + 0.055F) / 1.055F, 2.4F);
    };
    const auto srgb_luminance = [&](Color c) -> float {
        return 0.2126F * srgb_channel(c.red) +
               0.7152F * srgb_channel(c.green) +
               0.0722F * srgb_channel(c.blue);
    };
    const auto contrast_ratio = [&](Color a, Color b) -> float {
        const float l1 = srgb_luminance(a);
        const float l2 = srgb_luminance(b);
        return (std::max(l1, l2) + 0.05F) / (std::min(l1, l2) + 0.05F);
    };
    const auto blend_over = [](Color fg, float opacity, Color bg) -> Color {
        const float a = std::clamp(fg.alpha * opacity, 0.0F, 1.0F);
        return Color{
            fg.red * a + bg.red * (1.0F - a),
            fg.green * a + bg.green * (1.0F - a),
            fg.blue * a + bg.blue * (1.0F - a),
            1.0F,
        };
    };

    const auto presets = BrandPresets();
    for (const bool dark : {false, true}) {
        for (std::size_t idx = 0; idx < presets.size(); ++idx) {
            const ThemeSpec theme = AppThemeSpec(dark, presets[idx]);
            const StatusPalette sp = ResolveStatusPalette(theme);
            const std::string scope = std::string("ThemeToken[") + (dark ? "Dark" : "Light") +
                                      ",preset=" + std::to_string(idx) + "]";

            if (!(theme.colors.error == sp.danger)) {
                ctx.Flag(scope, "theme.colors.error does not match sp.danger");
            }
            if (!(sp.sidebar_bg == theme.colors.surface_container_low)) {
                ctx.Flag(scope, "sp.sidebar_bg does not match surface_container_low");
            }
            if (!(sp.card_bg == theme.colors.surface_container)) {
                ctx.Flag(scope, "sp.card_bg does not match surface_container");
            }
            if (theme.interactions.disabled_opacity < 0.48F) {
                ctx.Flag(scope, "disabled_opacity below 0.48F: " + std::to_string(theme.interactions.disabled_opacity));
            }

            const Color disabled_ghost = blend_over(
                sp.ghost_disabled_fg, theme.interactions.disabled_opacity, theme.colors.surface);
            const float disabled_contrast = contrast_ratio(disabled_ghost, theme.colors.surface);
            const float enabled_contrast = contrast_ratio(theme.colors.on_surface_variant, theme.colors.surface);
            if (disabled_contrast < 2.8F) {
                ctx.Flag(scope, "Disabled ghost button contrast too low: " + std::to_string(disabled_contrast));
            }
            if (disabled_contrast >= enabled_contrast) {
                ctx.Flag(scope, "Disabled ghost contrast (" + std::to_string(disabled_contrast) +
                                    ") not visually distinct from enabled (" + std::to_string(enabled_contrast) + ")");
            }

            const float danger_contrast = contrast_ratio(sp.on_danger_fill, sp.danger_fill);
            if (danger_contrast < 4.5F) {
                ctx.Flag(scope, "Danger fill contrast below 4.5:1: " + std::to_string(danger_contrast));
            }
        }
    }

    const auto saved_lines = store->console_lines.Get();
    ui.SetWindowMetrics({.viewport = {1180.0F, 760.0F}});
    store->active_nav = 2;
    store->console_lines = std::vector<std::string>{};
    store->SetThemeMode(ThemeMode::Light);
    ui.Pump();

    const auto copy_q = ui.FindSemantics(UiSemanticSelector::AllOf(
        UiSemanticSelector::Role(SemanticRole::Button),
        UiSemanticSelector::Label("复制")
    ));
    const auto clear_q = ui.FindSemantics(UiSemanticSelector::AllOf(
        UiSemanticSelector::Role(SemanticRole::Button),
        UiSemanticSelector::Label("清空")
    ));
    if (!copy_q.Exists() || copy_q.One().enabled) {
        ctx.Flag("TaskConsoleEmpty(Light)", "Expected disabled '复制' button when console_lines is empty");
    }
    if (!clear_q.Exists() || clear_q.One().enabled) {
        ctx.Flag("TaskConsoleEmpty(Light)", "Expected disabled '清空' button when console_lines is empty");
    }
    AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "TaskConsoleEmpty(Light)", "page-store", true);

    store->console_lines = std::vector<std::string>{"sample log line"};
    ui.Pump();
    if (!copy_q.Exists() || !copy_q.One().enabled) {
        ctx.Flag("TaskConsoleNonEmpty(Light)", "Expected enabled '复制' button when console_lines is non-empty");
    }
    if (!clear_q.Exists() || !clear_q.One().enabled) {
        ctx.Flag("TaskConsoleNonEmpty(Light)", "Expected enabled '清空' button when console_lines is non-empty");
    }

    store->console_lines = saved_lines;
    store->SetThemeMode(ThemeMode::Auto);
    ui.Pump();
}

void TestUnifiedRefreshStates(AuditContext& ctx, UiTest& ui, const std::shared_ptr<XlingsStore>& store) {
    ui.SetWindowMetrics({.viewport = {1180.0F, 760.0F}});

    auto verify_in_place = [&](const std::string& scope, const std::string& key, auto&& set_loading) {
        auto q = ui.Find(UiSelector::Key(key));
        if (!q.Exists()) {
            ctx.Flag(scope, "Missing refresh control key: " + key);
            return;
        }
        const UiNodeInfo idle_info = q.One();
        if (q.Find(UiSelector::Enabled(false)).Exists() || !q.Find(UiSelector::Enabled(true)).Exists()) {
            ctx.Flag(scope, "Refresh control '" + key + "' expected enabled before loading");
        }
        if (!q.Find(UiSelector::Type<Image>()).Exists()) {
            ctx.Flag(scope, "Refresh control '" + key + "' missing refresh Image before loading");
        }

        set_loading(true);
        ui.Pump();

        auto q_loading = ui.Find(UiSelector::Key(key));
        if (!q_loading.Exists()) {
            ctx.Flag(scope, "Refresh control '" + key + "' disappeared while loading");
            set_loading(false);
            ui.Pump();
            return;
        }
        const UiNodeInfo loading_info = q_loading.One();
        if (!q_loading.Find(UiSelector::Enabled(false)).Exists()) {
            ctx.Flag(scope, "Refresh control '" + key + "' expected disabled interactive node while loading");
        }
        if (q_loading.Find(UiSelector::Type<Image>()).Exists()) {
            ctx.Flag(scope, "Refresh control '" + key + "' still rendering static icon while loading");
        }
        if (std::fabs(loading_info.bounds.x - idle_info.bounds.x) > 1.0F ||
            std::fabs(loading_info.bounds.width - idle_info.bounds.width) > 1.0F) {
            ctx.Flag(scope, "Refresh control '" + key + "' shifted/resized while loading: idle=" +
                                FmtRect(idle_info.bounds) + " loading=" + FmtRect(loading_info.bounds));
        }

        set_loading(false);
        ui.Pump();

        auto q_restored = ui.Find(UiSelector::Key(key));
        if (!q_restored.Exists() || q_restored.Find(UiSelector::Enabled(false)).Exists() ||
            !q_restored.Find(UiSelector::Type<Image>()).Exists()) {
            ctx.Flag(scope, "Refresh control '" + key + "' did not restore enabled state and icon after loading");
        }
    };

    // 1. Dashboard & TitleBar in Light and Dark modes
    store->active_nav = 0;
    for (const ThemeMode mode : {ThemeMode::Light, ThemeMode::Dark}) {
        const std::string mode_tag = (mode == ThemeMode::Light) ? "Light" : "Dark";
        store->SetThemeMode(mode);
        store->refreshing = false;
        ui.Pump();

        verify_in_place("UnifiedRefresh[" + mode_tag + " / title]", "title-refresh-btn", [&](bool v) {
            store->refreshing = v;
        });
        verify_in_place("UnifiedRefresh[" + mode_tag + " / dashboard]", "dashboard-refresh-btn", [&](bool v) {
            store->refreshing = v;
        });

        store->refreshing = true;
        ui.Pump();
        AuditCurrentScreen(ctx, ui, 1180.0F, 760.0F, "DashboardRefreshing[" + mode_tag + "]", "page-dashboard");
        store->refreshing = false;
        ui.Pump();
    }

    store->SetThemeMode(ThemeMode::Light);

    // 2. Indexes page
    store->active_nav = 5;
    ui.Pump();
    verify_in_place("UnifiedRefresh / Indexes(local)", "indexes-refresh-btn", [&](bool v) {
        store->loading_indexes = v;
    });
    verify_in_place("UnifiedRefresh / Indexes(versions)", "indexes-refresh-btn", [&](bool v) {
        store->loading_index_versions = v;
    });

    // 3. Settings Tab 0 / 1 / 2
    store->active_nav = 6;
    store->settings_tab = 0;
    ui.Pump();
    const AppUpdateState saved_upd = store->app_update_state.Get();
    try {
        (void)ui.Find(UiSelector::Key("settings-scroll")).ScrollUntil(UiSelector::Key("about-check-update-btn"));
        ui.Pump();
    } catch (const UiTestFailure& ex) {
        ctx.Flag("UnifiedRefresh / SettingsAbout", std::string("ScrollUntil about-check-update-btn failed: ") + ex.what());
    }
    verify_in_place("UnifiedRefresh / SettingsAbout", "about-check-update-btn", [&](bool v) {
        AppUpdateState next = saved_upd;
        next.phase = v ? UpdatePhase::Checking : UpdatePhase::Available;
        store->app_update_state = std::move(next);
    });
    store->app_update_state = saved_upd;
    ui.Find(UiSelector::Key("settings-scroll")).ScrollBy({0.0F, -2000.0F});
    ui.Pump();

    store->settings_tab = 1;
    ui.Pump();
    verify_in_place("UnifiedRefresh / SettingsConfig", "settings-refresh-cfg-btn", [&](bool v) {
        store->loading_xlings_config = v;
    });
    try {
        (void)ui.Find(UiSelector::Key("settings-scroll")).ScrollUntil(UiSelector::Key("settings-refresh-themes-btn"));
        ui.Pump();
    } catch (const UiTestFailure& ex) {
        ctx.Flag("UnifiedRefresh / SettingsThemes", std::string("ScrollUntil settings-refresh-themes-btn failed: ") + ex.what());
    }
    verify_in_place("UnifiedRefresh / SettingsThemes", "settings-refresh-themes-btn", [&](bool v) {
        store->loading_config_themes = v;
    });
    ui.Find(UiSelector::Key("settings-scroll")).ScrollBy({0.0F, -2000.0F});
    ui.Pump();

    store->settings_tab = 2;
    ui.Pump();
    verify_in_place("UnifiedRefresh / SettingsNetwork", "settings-refresh-net-btn", [&](bool v) {
        store->loading_network = v;
    });
    try {
        (void)ui.Find(UiSelector::Key("settings-scroll")).ScrollUntil(UiSelector::Key("settings-refresh-profiles-btn"));
        ui.Pump();
    } catch (const UiTestFailure& ex) {
        ctx.Flag("UnifiedRefresh / SettingsProfiles", std::string("ScrollUntil settings-refresh-profiles-btn failed: ") + ex.what());
    }
    verify_in_place("UnifiedRefresh / SettingsProfiles", "settings-refresh-profiles-btn", [&](bool v) {
        store->loading_profiles = v;
    });
    ui.Find(UiSelector::Key("settings-scroll")).ScrollBy({0.0F, -2000.0F});
    ui.Pump();

    // 4. SubOS detail slot stability + events/policy refresh
    store->active_nav = 3;
    ui.Pump();
    if (SafeTap(ctx, "UnifiedRefresh / SubOS[Events]", ui.Find(UiSelector::Text("审计")), "审计")) {
        ui.Pump();
        auto find_detail_active_badge = [&]() -> std::optional<Rect> {
            std::optional<Rect> best;
            for (const auto& n : ui.Find(UiSelector::Text("当前活跃")).All()) {
                if (n.participates_in_layout && n.in_viewport && n.bounds.x > 500.0F) {
                    if (!best.has_value() || n.bounds.x > best->x) {
                        best = n.bounds;
                    }
                }
            }
            return best;
        };
        const std::optional<Rect> active_badge_before = find_detail_active_badge();
        if (!active_badge_before.has_value()) {
            ctx.Flag("UnifiedRefresh / SubOSDetailHeader", "Expected visible SubOS detail '当前活跃' badge");
        }
        verify_in_place("UnifiedRefresh / SubOSEvents", "subos-refresh-events-btn", [&](bool v) {
            store->loading_subos_detail = v;
        });
        if (active_badge_before.has_value()) {
            store->loading_subos_detail = true;
            ui.Pump();
            const std::optional<Rect> active_badge_after = find_detail_active_badge();
            if (!active_badge_after.has_value() ||
                std::fabs(active_badge_after->x - active_badge_before->x) > 1.0F) {
                ctx.Flag("UnifiedRefresh / SubOSDetailHeader",
                         "SubOS '当前活跃' badge shifted horizontally when loading_subos_detail turned true");
            }
            store->loading_subos_detail = false;
            ui.Pump();
        }
    }
    if (SafeTap(ctx, "UnifiedRefresh / SubOS[Policy]", ui.Find(UiSelector::Text("策略")), "策略")) {
        ui.Pump();
        verify_in_place("UnifiedRefresh / SubOSPolicy", "subos-refresh-policy-btn", [&](bool v) {
            store->loading_subos_governance = v;
        });
    }

    // 5. LubanOS outer refresh + detail refresh
    store->active_nav = 4;
    store->luban_tab = 0;
    ui.Pump();
    verify_in_place("UnifiedRefresh / Luban", "luban-refresh-btn", [&](bool v) {
        store->loading_luban = v;
    });
    if (SafeTap(ctx, "UnifiedRefresh / Luban[HistoryLogs]", ui.Find(UiSelector::Text("历史与日志")), "历史与日志")) {
        ui.Pump();
        verify_in_place("UnifiedRefresh / LubanDetail", "luban-refresh-detail-btn", [&](bool v) {
            store->loading_luban_detail = v;
        });
    }

    store->SetThemeMode(ThemeMode::Auto);
    store->active_nav = 0;
    store->settings_tab = 0;
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
    TestSearchBoxes(ctx, ui, store);
    TestThemeAndDisabledControls(ctx, ui, store);
    TestUnifiedRefreshStates(ctx, ui, store);

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

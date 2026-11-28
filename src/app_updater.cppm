export module hxlings.updater;

import std;
import mcpplibs.os;
import nlohmann.json;
import xlings.types;
import xlings.parser;
import xlings.process;

export namespace hxlings {

inline constexpr std::string_view kAppVersion = "0.1.7";
inline constexpr std::string_view kAppReleaseApiUrl =
    "https://api.github.com/repos/FarnaHerry/Hxlings/releases/latest";
inline constexpr std::string_view kAppReleasePageUrl =
    "https://github.com/FarnaHerry/Hxlings/releases/latest";

enum class UpdatePhase {
    Idle,
    Checking,
    UpToDate,
    Available,
    Downloading,
    Verifying,
    ReadyToInstall,
    Installing,
    Error,
};

enum class UpdateInstallKind {
    WindowsSetupExe,
    WindowsMsi,
    WindowsZip,
    MacOsDmg,
    MacOsTarXz,
    MacOsTarGz,
    LinuxAppImage,
    LinuxDeb,
    LinuxRpm,
    LinuxTarXz,
    LinuxTarGz,
    ManualOnly,
};

struct ReleaseAssetInfo {
    std::string name;
    std::string download_url;
    std::uint64_t size_bytes = 0;

    bool operator==(const ReleaseAssetInfo&) const = default;
};

struct AppReleaseInfo {
    std::string tag_name;
    std::string version;
    std::string name;
    std::string published_at;
    std::string html_url;
    std::string body;
    std::vector<ReleaseAssetInfo> assets;
    std::string checksums_url;

    bool operator==(const AppReleaseInfo&) const = default;
};

struct PlatformRuntimeHints {
    std::string os_name;
    std::string arch;
    std::string appimage_path;
    std::string exe_path;
    bool has_dpkg = false;
    bool has_rpm = false;

    bool operator==(const PlatformRuntimeHints&) const = default;
};

struct AppUpdateState {
    UpdatePhase phase = UpdatePhase::Idle;
    std::string current_version = std::string(kAppVersion);
    std::optional<AppReleaseInfo> latest;
    std::optional<ReleaseAssetInfo> selected_asset;
    UpdateInstallKind install_kind = UpdateInstallKind::ManualOnly;
    std::uint64_t downloaded_bytes = 0;
    std::uint64_t total_bytes = 0;
    std::string downloaded_file_path;
    bool sha256_verified = false;
    std::string status_message;
    std::string error_message;
    std::string checked_at;

    bool operator==(const AppUpdateState&) const = default;
};

struct ApplyUpdateResult {
    bool ok = false;
    bool should_quit_app = false;
    std::string message;
    std::string install_hint;

    bool operator==(const ApplyUpdateResult&) const = default;
};

class Sha256Hasher {
public:
    Sha256Hasher() noexcept { Reset(); }

    void Reset() noexcept {
        state_ = {
            0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
            0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
        };
        total_bytes_ = 0;
        buf_len_ = 0;
    }

    void Update(std::span<const std::byte> data) noexcept {
        for (const std::byte b : data) {
            buffer_[buf_len_++] = static_cast<std::uint8_t>(b);
            ++total_bytes_;
            if (buf_len_ == 64) {
                Transform(buffer_.data());
                buf_len_ = 0;
            }
        }
    }

    void UpdateString(std::string_view sv) noexcept {
        Update(std::as_bytes(std::span{sv.data(), sv.size()}));
    }

    [[nodiscard]] std::string FinalHex() {
        const std::uint64_t bit_len = total_bytes_ * 8ULL;
        buffer_[buf_len_++] = 0x80u;
        if (buf_len_ > 56) {
            while (buf_len_ < 64) buffer_[buf_len_++] = 0u;
            Transform(buffer_.data());
            buf_len_ = 0;
        }
        while (buf_len_ < 56) buffer_[buf_len_++] = 0u;
        for (int shift = 56; shift >= 0; shift -= 8) {
            buffer_[buf_len_++] = static_cast<std::uint8_t>((bit_len >> shift) & 0xFFu);
        }
        Transform(buffer_.data());

        std::string out;
        out.reserve(64);
        for (const std::uint32_t w : state_) {
            out += std::format("{:08x}", w);
        }
        return out;
    }

private:
    static constexpr std::array<std::uint32_t, 64> kRoundConstants{
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
    };

    static constexpr std::uint32_t RotR(std::uint32_t x, unsigned int n) noexcept {
        return (x >> n) | (x << (32u - n));
    }

    void Transform(const std::uint8_t* block) noexcept {
        std::array<std::uint32_t, 64> w{};
        for (std::size_t i = 0; i < 16; ++i) {
            const std::size_t j = i * 4;
            w[i] = (static_cast<std::uint32_t>(block[j]) << 24) |
                   (static_cast<std::uint32_t>(block[j + 1]) << 16) |
                   (static_cast<std::uint32_t>(block[j + 2]) << 8) |
                   static_cast<std::uint32_t>(block[j + 3]);
        }
        for (std::size_t i = 16; i < 64; ++i) {
            const std::uint32_t s0 = RotR(w[i - 15], 7) ^ RotR(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = RotR(w[i - 2], 17) ^ RotR(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        std::uint32_t e = state_[4];
        std::uint32_t f = state_[5];
        std::uint32_t g = state_[6];
        std::uint32_t h = state_[7];

        for (std::size_t i = 0; i < 64; ++i) {
            const std::uint32_t S1 = RotR(e, 6) ^ RotR(e, 11) ^ RotR(e, 25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = h + S1 + ch + kRoundConstants[i] + w[i];
            const std::uint32_t S0 = RotR(a, 2) ^ RotR(a, 13) ^ RotR(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = S0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::uint64_t total_bytes_ = 0;
    std::size_t buf_len_ = 0;
};

std::string Sha256Hex(std::string_view data) {
    Sha256Hasher h;
    h.UpdateString(data);
    return h.FinalHex();
}

std::string NormalizeSemverTag(std::string_view tag) {
    std::size_t start = 0;
    while (start < tag.size() && (tag[start] == ' ' || tag[start] == '\t' || tag[start] == '\r' || tag[start] == '\n')) {
        ++start;
    }
    std::size_t end = tag.size();
    while (end > start && (tag[end - 1] == ' ' || tag[end - 1] == '\t' || tag[end - 1] == '\r' || tag[end - 1] == '\n')) {
        --end;
    }
    std::string_view sv = tag.substr(start, end - start);
    if (!sv.empty() && (sv.front() == 'v' || sv.front() == 'V')) {
        sv.remove_prefix(1);
    }
    return std::string(sv);
}

bool IsNewerAppVersion(std::string_view current, std::string_view candidate) {
    const std::string cur_norm = NormalizeSemverTag(current);
    const std::string cand_norm = NormalizeSemverTag(candidate);
    if (cur_norm.empty() || cand_norm.empty()) return false;
    return CompareVersionStrings(cand_norm, cur_norm) > 0;
}

std::optional<AppReleaseInfo> ParseGitHubLatestReleaseJson(std::string_view json_body) {
    try {
        const auto doc = nlohmann::json::parse(json_body);
        if (!doc.is_object()) return std::nullopt;

        auto str_field = [&](std::string_view key) -> std::string {
            const auto it = doc.find(std::string(key));
            return (it != doc.end() && it->is_string()) ? it->get<std::string>() : std::string{};
        };

        AppReleaseInfo info;
        info.tag_name = str_field("tag_name");
        info.version = NormalizeSemverTag(info.tag_name);
        info.name = str_field("name");
        if (info.name.empty()) info.name = info.tag_name;
        info.published_at = str_field("published_at");
        info.html_url = NormalizeBrowserUrl(str_field("html_url"));
        info.body = str_field("body");
        if (info.tag_name.empty()) return std::nullopt;

        if (doc.contains("assets") && doc["assets"].is_array()) {
            for (const auto& item : doc["assets"]) {
                if (!item.is_object()) continue;
                const auto name_it = item.find("name");
                const auto url_it = item.find("browser_download_url");
                if (name_it == item.end() || !name_it->is_string() ||
                    url_it == item.end() || !url_it->is_string()) {
                    continue;
                }
                const std::string asset_name = name_it->get<std::string>();
                const std::string dl_url = NormalizeBrowserUrl(url_it->get<std::string>());
                if (asset_name.empty() || dl_url.empty()) continue;

                std::uint64_t size_bytes = 0;
                if (const auto sz_it = item.find("size"); sz_it != item.end() && sz_it->is_number_unsigned()) {
                    size_bytes = sz_it->get<std::uint64_t>();
                }
                if (asset_name == "SHA256SUMS.txt") {
                    info.checksums_url = dl_url;
                } else {
                    info.assets.push_back(ReleaseAssetInfo{
                        .name = asset_name,
                        .download_url = dl_url,
                        .size_bytes = size_bytes,
                    });
                }
            }
        }
        return info;
    } catch (...) {
        return std::nullopt;
    }
}

std::string ParseSha256SumsText(std::string_view text, std::string_view asset_name) {
    if (asset_name.empty()) return {};
    auto ascii_lower = [](std::string_view s) -> std::string {
        std::string out(s);
        for (char& c : out) {
            if (c >= 'A' && c <= 'F') c = static_cast<char>(c - 'A' + 'a');
        }
        return out;
    };
    auto is_hex64 = [](std::string_view s) -> bool {
        if (s.size() != 64) return false;
        for (const char ch : s) {
            const bool ok = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
            if (!ok) return false;
        }
        return true;
    };

    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t nl = text.find('\n', start);
        const std::size_t end = (nl == std::string_view::npos) ? text.size() : nl;
        std::string_view line = text.substr(start, end - start);
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t' || line.front() == '\r')) {
            line.remove_prefix(1);
        }
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.remove_suffix(1);
        }
        if (!line.empty() && line.front() != '#') {
            std::size_t sp = 0;
            while (sp < line.size() && line[sp] != ' ' && line[sp] != '\t') ++sp;
            const std::string_view hash_part = line.substr(0, sp);
            std::string_view file_part = (sp < line.size()) ? line.substr(sp) : std::string_view{};
            while (!file_part.empty() && (file_part.front() == ' ' || file_part.front() == '\t' || file_part.front() == '*')) {
                file_part.remove_prefix(1);
            }
            if (file_part.starts_with("./")) file_part.remove_prefix(2);
            const std::size_t slash = file_part.find_last_of("/\\");
            const std::string_view base = (slash == std::string_view::npos) ? file_part : file_part.substr(slash + 1);
            if (is_hex64(hash_part) && base == asset_name) {
                return ascii_lower(hash_part);
            }
        }
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return {};
}

PlatformRuntimeHints DetectCurrentPlatformHints() {
    namespace os = mcpplibs::os;
    PlatformRuntimeHints hints;
    if constexpr (os::is_windows) {
        hints.os_name = "windows";
    } else if constexpr (os::is_macos) {
        hints.os_name = "macos";
    } else {
        hints.os_name = "linux";
    }
    hints.arch = os::machine::arch();
    if (hints.arch == "amd64") hints.arch = "x86_64";
    if (hints.arch == "aarch64" && hints.os_name == "macos") hints.arch = "arm64";

    if (const auto ai = os::env::get("APPIMAGE"); ai.has_value() && !ai->empty()) {
        hints.appimage_path = *ai;
    }
    if (const auto exe = os::env::image(); exe.has_value() && !exe->empty()) {
        hints.exe_path = os::fs::utf8(*exe);
    }
    if constexpr (os::is_linux) {
        std::error_code ec;
        const bool sys_install = hints.exe_path.starts_with("/usr/") || hints.exe_path.starts_with("/opt/");
        if (hints.appimage_path.empty() && sys_install) {
            hints.has_dpkg = std::filesystem::exists("/usr/bin/dpkg", ec) ||
                             std::filesystem::exists("/usr/bin/apt", ec);
            hints.has_rpm = std::filesystem::exists("/usr/bin/rpm", ec) ||
                            std::filesystem::exists("/usr/bin/dnf", ec);
        }
    }
    return hints;
}

std::pair<std::optional<ReleaseAssetInfo>, UpdateInstallKind> SelectPlatformReleaseAsset(
    const AppReleaseInfo& release,
    const PlatformRuntimeHints& hints = DetectCurrentPlatformHints()
) {
    auto ascii_lower = [](std::string_view s) {
        std::string out(s);
        for (char& c : out) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        return out;
    };
    const std::string arch_norm = ascii_lower(hints.arch);
    const bool want_arm64 = (arch_norm == "arm64" || arch_norm == "aarch64");
    const bool want_x64 = (arch_norm == "x86_64" || arch_norm == "amd64" || arch_norm == "x64");

    auto arch_compatible = [&](const std::string& lower_name) -> bool {
        const bool has_arm64 = lower_name.find("arm64") != std::string::npos ||
                               lower_name.find("aarch64") != std::string::npos;
        const bool has_x64 = lower_name.find("x86_64") != std::string::npos ||
                             lower_name.find("amd64") != std::string::npos;
        if (want_arm64 && has_x64 && !has_arm64) return false;
        if (want_x64 && has_arm64 && !has_x64) return false;
        return true;
    };

    auto find_matching = [&](std::string_view os_token, std::string_view suffix, UpdateInstallKind kind)
        -> std::optional<std::pair<ReleaseAssetInfo, UpdateInstallKind>> {
        for (const auto& a : release.assets) {
            const std::string low = ascii_lower(a.name);
            if (!os_token.empty() && low.find(os_token) == std::string::npos) continue;
            if (!low.ends_with(suffix)) continue;
            if (!arch_compatible(low)) continue;
            return std::pair{a, kind};
        }
        return std::nullopt;
    };

    if (hints.os_name == "windows") {
        if (auto m = find_matching("windows", "-setup.exe", UpdateInstallKind::WindowsSetupExe)) {
            return {m->first, m->second};
        }
        if (auto m = find_matching("windows", ".msi", UpdateInstallKind::WindowsMsi)) {
            return {m->first, m->second};
        }
        if (auto m = find_matching("windows", ".zip", UpdateInstallKind::WindowsZip)) {
            return {m->first, m->second};
        }
        return {std::nullopt, UpdateInstallKind::ManualOnly};
    }

    if (hints.os_name == "macos") {
        if (auto m = find_matching("macos", ".dmg", UpdateInstallKind::MacOsDmg)) {
            return {m->first, m->second};
        }
        if (auto m = find_matching("macos", ".tar.xz", UpdateInstallKind::MacOsTarXz)) {
            return {m->first, m->second};
        }
        if (auto m = find_matching("macos", ".tar.gz", UpdateInstallKind::MacOsTarGz)) {
            return {m->first, m->second};
        }
        return {std::nullopt, UpdateInstallKind::ManualOnly};
    }

    // Linux selection order
    if (!hints.appimage_path.empty()) {
        if (auto m = find_matching("linux", ".appimage", UpdateInstallKind::LinuxAppImage)) {
            return {m->first, m->second};
        }
    }
    if (hints.has_dpkg) {
        if (auto m = find_matching("linux", ".deb", UpdateInstallKind::LinuxDeb)) {
            return {m->first, m->second};
        }
    }
    if (hints.has_rpm) {
        if (auto m = find_matching("linux", ".rpm", UpdateInstallKind::LinuxRpm)) {
            return {m->first, m->second};
        }
    }
    if (auto m = find_matching("linux", ".appimage", UpdateInstallKind::LinuxAppImage)) {
        return {m->first, m->second};
    }
    if (auto m = find_matching("linux", ".deb", UpdateInstallKind::LinuxDeb)) {
        return {m->first, m->second};
    }
    if (auto m = find_matching("linux", ".rpm", UpdateInstallKind::LinuxRpm)) {
        return {m->first, m->second};
    }
    if (auto m = find_matching("linux", ".tar.xz", UpdateInstallKind::LinuxTarXz)) {
        return {m->first, m->second};
    }
    if (auto m = find_matching("linux", ".tar.gz", UpdateInstallKind::LinuxTarGz)) {
        return {m->first, m->second};
    }
    return {std::nullopt, UpdateInstallKind::ManualOnly};
}

std::filesystem::path ResolveUpdateDownloadDir() {
    namespace os = mcpplibs::os;
    if (const auto cache = os::dirs::cache(); cache.has_value() && !cache->empty()) {
        return *cache / "hxlings" / "updates";
    }
    return os::dirs::temp() / "hxlings-updates";
}

std::string CurrentLocalTimestampNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    static std::mutex now_mu;
    std::tm tm_val{};
    {
        std::lock_guard lock(now_mu);
        if (const std::tm* ptm = std::localtime(&tt)) {
            tm_val = *ptm;
        } else {
            return FormatEpochWithOffsetMinutes(static_cast<std::int64_t>(tt), 0);
        }
    }
    return std::format(
        "{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
        tm_val.tm_year + 1900,
        tm_val.tm_mon + 1,
        tm_val.tm_mday,
        tm_val.tm_hour,
        tm_val.tm_min,
        tm_val.tm_sec
    );
}

ApplyUpdateResult ApplyDownloadedUpdate(
    const std::filesystem::path& file_path,
    UpdateInstallKind kind,
    const PlatformRuntimeHints& hints = DetectCurrentPlatformHints()
) {
    namespace os = mcpplibs::os;
    std::error_code ec;
    if (file_path.empty() || !std::filesystem::is_regular_file(file_path, ec)) {
        return ApplyUpdateResult{
            .ok = false,
            .message = "更新包文件不存在，请先下载更新",
        };
    }

    const std::string file_utf8 = os::fs::utf8(file_path);
    switch (kind) {
        case UpdateInstallKind::WindowsSetupExe:
        case UpdateInstallKind::WindowsMsi: {
            if (!OpenLocalPath(file_path)) {
                return ApplyUpdateResult{
                    .ok = false,
                    .message = "无法启动更新安装程序: " + file_utf8,
                };
            }
            return ApplyUpdateResult{
                .ok = true,
                .should_quit_app = true,
                .message = "已启动安装程序，正在退出当前应用以完成升级...",
            };
        }
        case UpdateInstallKind::MacOsDmg: {
            if (!OpenLocalPath(file_path)) {
                return ApplyUpdateResult{
                    .ok = false,
                    .message = "无法挂载更新镜像: " + file_utf8,
                };
            }
            return ApplyUpdateResult{
                .ok = true,
                .should_quit_app = false,
                .message = "已打开 .dmg 安装镜像，请将 Hxlings.app 拖入“应用程序”完成覆盖升级",
            };
        }
        case UpdateInstallKind::LinuxAppImage: {
            std::filesystem::permissions(
                file_path,
                std::filesystem::perms::owner_all |
                    std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
                    std::filesystem::perms::others_read | std::filesystem::perms::others_exec,
                ec
            );
            if (!hints.appimage_path.empty()) {
                const std::filesystem::path current_ai = os::fs::from_utf8(hints.appimage_path);
                std::filesystem::path staged = current_ai;
                staged += ".new";
                std::filesystem::copy_file(
                    file_path, staged, std::filesystem::copy_options::overwrite_existing, ec);
                if (!ec) {
                    std::filesystem::permissions(
                        staged,
                        std::filesystem::perms::owner_all |
                            std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
                            std::filesystem::perms::others_read | std::filesystem::perms::others_exec,
                        ec
                    );
                    std::filesystem::rename(staged, current_ai, ec);
                    if (!ec && LaunchDetachedExecutable(current_ai)) {
                        return ApplyUpdateResult{
                            .ok = true,
                            .should_quit_app = true,
                            .message = "AppImage 已就地替换，正在重启新版 Hxlings...",
                        };
                    }
                    std::error_code rm_ec;
                    std::filesystem::remove(staged, rm_ec);
                }
            }
            if (LaunchDetachedExecutable(file_path)) {
                return ApplyUpdateResult{
                    .ok = true,
                    .should_quit_app = true,
                    .message = "已启动新版 AppImage，当前实例即将退出...",
                };
            }
            (void)OpenLocalPath(file_path.parent_path());
            return ApplyUpdateResult{
                .ok = false,
                .message = "已打开下载目录，请手动替换或赋予执行权限后运行新版 AppImage",
                .install_hint = "chmod +x \"" + file_utf8 + "\" && \"" + file_utf8 + "\"",
            };
        }
        case UpdateInstallKind::LinuxDeb: {
            (void)OpenLocalPath(file_path.parent_path());
            return ApplyUpdateResult{
                .ok = true,
                .should_quit_app = false,
                .message = "已就绪 .deb 安装包并打开所在目录，请在终端提权安装",
                .install_hint = "sudo apt install \"" + file_utf8 + "\"",
            };
        }
        case UpdateInstallKind::LinuxRpm: {
            (void)OpenLocalPath(file_path.parent_path());
            return ApplyUpdateResult{
                .ok = true,
                .should_quit_app = false,
                .message = "已就绪 .rpm 安装包并打开所在目录，请在终端提权安装",
                .install_hint = "sudo dnf install -y \"" + file_utf8 + "\"",
            };
        }
        case UpdateInstallKind::WindowsZip:
        case UpdateInstallKind::MacOsTarXz:
        case UpdateInstallKind::MacOsTarGz:
        case UpdateInstallKind::LinuxTarXz:
        case UpdateInstallKind::LinuxTarGz:
        case UpdateInstallKind::ManualOnly: {
            (void)OpenLocalPath(file_path.parent_path());
            return ApplyUpdateResult{
                .ok = true,
                .should_quit_app = false,
                .message = "已下载更新归档并打开目录: " + file_utf8,
                .install_hint = file_utf8,
            };
        }
    }
    return ApplyUpdateResult{};
}

} // namespace hxlings

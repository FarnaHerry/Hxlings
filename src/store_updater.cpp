module hxlings.store;

import std;
import mcpplibs.os;
import huxerui;
import xlings.types;
import xlings.parser;
import xlings.process;
import xlings.client;
import hxlings.theme;
import hxlings.settings;
import hxlings.updater;
import hxlings.single_instance;
import hxlings.luban.types;
import hxlings.luban.client;
import hxlings.luban.stack;

using namespace huxerui;

namespace hxlings {

void XlingsStore::CheckForAppUpdateAsync(
    TaskScope tasks,
    ToastHandle toast,
    std::shared_ptr<HttpClient> http
) {
    if (skip_initial_refresh) {
        app_update_state.Update([](AppUpdateState& st) {
            st.status_message = "测试模式下跳过联网检查";
            st.error_message.clear();
        });
        return;
    }
    const AppUpdateState cur = app_update_state.Get();
    if (cur.phase == UpdatePhase::Checking ||
        cur.phase == UpdatePhase::Downloading ||
        cur.phase == UpdatePhase::Verifying) {
        toast.Show("正在检查或下载更新，请稍候");
        return;
    }
    if (!http) {
        app_update_state.Update([](AppUpdateState& st) {
            st.phase = UpdatePhase::Error;
            st.error_message = "HTTP 网络客户端不可用";
            st.status_message = st.error_message;
        });
        toast.Show("HTTP 网络客户端不可用");
        return;
    }

    app_update_task_.Cancel();
    app_update_state.Update([](AppUpdateState& st) {
        st.phase = UpdatePhase::Checking;
        st.error_message.clear();
        st.status_message = "正在检查 GitHub Releases 最新版本...";
    });

    auto self = shared_from_this();
    app_update_task_ = tasks.Launch([self, http = std::move(http), toast]() -> Task<void> {
        const HttpRequest req{
            .url = std::string(kAppReleaseApiUrl),
            .headers = {
                {"accept", "application/vnd.github+json"},
                {"user-agent", "Hxlings/" + std::string(kAppVersion)},
            },
            .timeout = std::chrono::milliseconds{15000},
        };
        const auto resp = co_await http->SendAsync(req);
        if (!resp.Succeeded()) {
            const std::string err = "检查更新失败: " + resp.Error().message;
            self->app_update_state.Update([&](AppUpdateState& st) {
                st.phase = UpdatePhase::Error;
                st.error_message = err;
                st.status_message = err;
                st.checked_at = CurrentLocalTimestampNow();
            });
            self->ShowToastSafe(err, toast);
            co_return;
        }

        const HttpResponse& http_val = resp.Value();
        if (http_val.status_code < 200 || http_val.status_code >= 300) {
            const std::string err = "GitHub API 返回 HTTP " + std::to_string(http_val.status_code);
            self->app_update_state.Update([&](AppUpdateState& st) {
                st.phase = UpdatePhase::Error;
                st.error_message = err;
                st.status_message = err;
                st.checked_at = CurrentLocalTimestampNow();
            });
            self->ShowToastSafe(err, toast);
            co_return;
        }

        const std::string_view body_sv(
            reinterpret_cast<const char*>(http_val.body.data()),
            http_val.body.size()
        );
        auto parsed = ParseGitHubLatestReleaseJson(body_sv);
        if (!parsed.has_value()) {
            const std::string err = "解析 GitHub Release 响应失败";
            self->app_update_state.Update([&](AppUpdateState& st) {
                st.phase = UpdatePhase::Error;
                st.error_message = err;
                st.status_message = err;
                st.checked_at = CurrentLocalTimestampNow();
            });
            self->ShowToastSafe(err, toast);
            co_return;
        }

        auto [selected_asset, install_kind] = SelectPlatformReleaseAsset(*parsed);
        const bool newer = IsNewerAppVersion(kAppVersion, parsed->version);
        const std::string now_ts = CurrentLocalTimestampNow();

        if (!newer) {
            self->app_update_state.Update([&](AppUpdateState& st) {
                st.phase = UpdatePhase::UpToDate;
                st.latest = parsed;
                st.selected_asset = selected_asset;
                st.install_kind = install_kind;
                st.error_message.clear();
                st.checked_at = now_ts;
                st.status_message = "当前已是最新版本 (v" + std::string(kAppVersion) + ")";
            });
            self->ShowToastSafe("当前已是最新版本", toast);
            co_return;
        }

        const std::string tag = parsed->tag_name;
        const std::uint64_t total_sz = selected_asset.has_value() ? selected_asset->size_bytes : 0;
        self->app_update_state.Update([&](AppUpdateState& st) {
            st.phase = UpdatePhase::Available;
            st.latest = std::move(parsed);
            st.selected_asset = std::move(selected_asset);
            st.install_kind = install_kind;
            st.downloaded_bytes = 0;
            st.total_bytes = total_sz;
            st.downloaded_file_path.clear();
            st.sha256_verified = false;
            st.error_message.clear();
            st.checked_at = now_ts;
            st.status_message = "发现新版本 " + tag;
        });
        self->ShowToastSafe("发现新版本 " + tag, toast);
    });
}

void XlingsStore::DownloadAppUpdateAsync(
    TaskScope tasks,
    ToastHandle toast,
    std::shared_ptr<HttpClient> http
) {
    if (skip_initial_refresh) {
        app_update_state.Update([](AppUpdateState& st) {
            st.status_message = "测试模式下跳过联网下载";
            st.error_message.clear();
        });
        return;
    }
    const AppUpdateState cur = app_update_state.Get();
    if (cur.phase == UpdatePhase::Checking ||
        cur.phase == UpdatePhase::Downloading ||
        cur.phase == UpdatePhase::Verifying) {
        toast.Show("已有更新任务正在进行");
        return;
    }
    if (!cur.latest.has_value() || !cur.selected_asset.has_value() ||
        !IsSafeExternalUrl(cur.selected_asset->download_url)) {
        toast.Show("未找到适配当前平台的更新安装包");
        return;
    }
    if (!http) {
        toast.Show("HTTP 网络客户端不可用");
        return;
    }

    const AppReleaseInfo latest = *cur.latest;
    const ReleaseAssetInfo asset = *cur.selected_asset;
    const UpdateInstallKind install_kind = cur.install_kind;
    const std::filesystem::path dir = ResolveUpdateDownloadDir();
    const std::filesystem::path part_path = dir / (asset.name + ".part");
    const std::filesystem::path final_path = dir / asset.name;

    app_update_task_.Cancel();
    active_update_part_path_ = part_path;
    app_update_state.Update([&](AppUpdateState& st) {
        st.phase = UpdatePhase::Downloading;
        st.downloaded_bytes = 0;
        st.total_bytes = asset.size_bytes;
        st.downloaded_file_path.clear();
        st.sha256_verified = false;
        st.error_message.clear();
        st.status_message = "正在下载 " + asset.name + "...";
    });

    auto self = shared_from_this();
    app_update_task_ = tasks.Launch(
        [self, http = std::move(http), toast, latest, asset, install_kind, dir, part_path, final_path]() -> Task<void> {
            auto fail_update = [&](std::string msg) {
                std::error_code rm_ec;
                std::filesystem::remove(part_path, rm_ec);
                self->active_update_part_path_.clear();
                self->app_update_state.Update([&](AppUpdateState& st) {
                    st.phase = UpdatePhase::Error;
                    st.error_message = msg;
                    st.status_message = msg;
                });
                self->ShowToastSafe(std::move(msg), toast);
            };

            std::error_code dir_ec;
            std::filesystem::create_directories(dir, dir_ec);
            if (dir_ec) {
                fail_update("无法创建更新缓存目录: " + dir_ec.message());
                co_return;
            }

            std::string expected_sha256;
            if (!latest.checksums_url.empty()) {
                const auto sum_resp = co_await http->SendAsync(HttpRequest{
                    .url = latest.checksums_url,
                    .headers = {{"user-agent", "Hxlings/" + std::string(kAppVersion)}},
                    .timeout = std::chrono::milliseconds{15000},
                });
                if (!sum_resp.Succeeded() || sum_resp.Value().status_code < 200 ||
                    sum_resp.Value().status_code >= 300) {
                    fail_update("下载 SHA256SUMS.txt 失败，已中止更新以保障完整性");
                    co_return;
                }
                const std::string_view sum_sv(
                    reinterpret_cast<const char*>(sum_resp.Value().body.data()),
                    sum_resp.Value().body.size()
                );
                expected_sha256 = ParseSha256SumsText(sum_sv, asset.name);
                if (expected_sha256.empty()) {
                    fail_update("校验清单中未找到目标文件 " + asset.name + " 的 SHA-256");
                    co_return;
                }
            }

            File part_file(mcpplibs::os::fs::utf8(part_path));
            auto out_opened = co_await part_file.OpenWriteAsync(FileWriteMode::Truncate);
            if (!out_opened.Succeeded()) {
                fail_update("无法打开临时下载文件: " + out_opened.Error().message);
                co_return;
            }
            AsyncOutputStream out_stream = std::move(out_opened).Value();

            auto http_opened = co_await http->SendStreamAsync(HttpRequest{
                .url = asset.download_url,
                .headers = {{"user-agent", "Hxlings/" + std::string(kAppVersion)}},
                .timeout = std::nullopt,
            });
            if (!http_opened.Succeeded()) {
                (void)co_await out_stream.CloseAsync();
                fail_update("下载更新包失败: " + http_opened.Error().message);
                co_return;
            }
            HttpResponseStream http_stream = std::move(http_opened).Value();
            if (http_stream.StatusCode() < 200 || http_stream.StatusCode() >= 300) {
                (void)co_await out_stream.CloseAsync();
                fail_update("更新包下载返回 HTTP " + std::to_string(http_stream.StatusCode()));
                co_return;
            }

            std::uint64_t expected_total = asset.size_bytes;
            if (expected_total == 0) {
                for (const auto& hdr : http_stream.Headers()) {
                    if (hdr.name == "content-length" || hdr.name == "Content-Length") {
                        std::uint64_t parsed_len = 0;
                        const auto [ptr, ec] = std::from_chars(
                            hdr.value.data(), hdr.value.data() + hdr.value.size(), parsed_len);
                        if (ec == std::errc{} && ptr == hdr.value.data() + hdr.value.size()) {
                            expected_total = parsed_len;
                        }
                        break;
                    }
                }
            }

            Sha256Hasher hasher;
            std::uint64_t downloaded = 0;
            auto last_ui_tick = std::chrono::steady_clock::now();

            while (true) {
                auto read_res = co_await http_stream.Body().ReadAsync(64u * 1024u);
                if (!read_res.Succeeded()) {
                    (void)co_await out_stream.CloseAsync();
                    fail_update("读取更新数据流失败: " + read_res.Error().message);
                    co_return;
                }
                Bytes chunk = std::move(read_res).Value();
                if (chunk.empty()) {
                    break;
                }
                hasher.Update(chunk);
                downloaded += static_cast<std::uint64_t>(chunk.size());

                auto write_res = co_await out_stream.WriteAsync(std::move(chunk));
                if (!write_res.Succeeded()) {
                    (void)co_await out_stream.CloseAsync();
                    fail_update("写入本地更新文件失败: " + write_res.Error().message);
                    co_return;
                }

                const auto now = std::chrono::steady_clock::now();
                if (now - last_ui_tick >= std::chrono::milliseconds{100}) {
                    last_ui_tick = now;
                    self->app_update_state.Update([downloaded, expected_total](AppUpdateState& st) {
                        st.downloaded_bytes = downloaded;
                        if (expected_total > 0) st.total_bytes = expected_total;
                    });
                }
            }

            auto close_res = co_await out_stream.CloseAsync();
            if (!close_res.Succeeded()) {
                fail_update("落盘关闭下载文件失败: " + close_res.Error().message);
                co_return;
            }

            self->app_update_state.Update([downloaded, expected_total](AppUpdateState& st) {
                st.phase = UpdatePhase::Verifying;
                st.downloaded_bytes = downloaded;
                st.total_bytes = expected_total > 0 ? expected_total : downloaded;
                st.status_message = "正在校验 SHA-256...";
            });

            const std::string actual_sha256 = hasher.FinalHex();
            if (!expected_sha256.empty() && actual_sha256 != expected_sha256) {
                fail_update("SHA-256 校验不匹配（期望 " + expected_sha256.substr(0, 12) +
                            "...，实际 " + actual_sha256.substr(0, 12) + "...）");
                co_return;
            }

            std::error_code mv_ec;
            std::filesystem::remove(final_path, mv_ec);
            mv_ec.clear();
            std::filesystem::rename(part_path, final_path, mv_ec);
            if (mv_ec) {
                fail_update("重命名更新文件失败: " + mv_ec.message());
                co_return;
            }

            self->active_update_part_path_.clear();
            const std::string final_utf8 = mcpplibs::os::fs::utf8(final_path);
            const bool verified = !expected_sha256.empty();
            self->app_update_state.Update([&](AppUpdateState& st) {
                st.phase = UpdatePhase::ReadyToInstall;
                st.downloaded_bytes = downloaded;
                if (st.total_bytes == 0) st.total_bytes = downloaded;
                st.downloaded_file_path = final_utf8;
                st.sha256_verified = verified;
                st.error_message.clear();
                st.status_message = verified
                                        ? ("已下载并验证 SHA-256: " + asset.name)
                                        : ("已下载更新包: " + asset.name);
            });
            self->ShowToastSafe("更新包已下载并校验完成，可点击“立即安装更新”", toast);
        }
    );
}

void XlingsStore::CancelAppUpdate() {
    app_update_task_.Cancel();
    if (!active_update_part_path_.empty()) {
        std::error_code ec;
        std::filesystem::remove(active_update_part_path_, ec);
        active_update_part_path_.clear();
    }
    app_update_state.Update([](AppUpdateState& st) {
        if (st.phase == UpdatePhase::Downloading ||
            st.phase == UpdatePhase::Verifying ||
            st.phase == UpdatePhase::Checking) {
            st.phase = st.latest.has_value() ? UpdatePhase::Available : UpdatePhase::Idle;
            st.downloaded_bytes = 0;
            st.error_message.clear();
            st.status_message = "已取消更新操作";
        }
    });
}

void XlingsStore::ApplyAppUpdateAsync(ToastHandle toast, ApplicationHandle application) {
    if (skip_initial_refresh) {
        app_update_state.Update([](AppUpdateState& st) {
            st.status_message = "测试模式下跳过安装启动";
        });
        return;
    }
    const AppUpdateState snap = app_update_state.Get();
    if (snap.downloaded_file_path.empty()) {
        toast.Show("请先下载更新包");
        return;
    }
    app_update_state.Update([](AppUpdateState& st) {
        st.phase = UpdatePhase::Installing;
        st.status_message = "正在启动更新安装流程...";
    });
    const bool releases_for_relaunch =
        snap.install_kind == UpdateInstallKind::LinuxAppImage ||
        snap.install_kind == UpdateInstallKind::WindowsSetupExe ||
        snap.install_kind == UpdateInstallKind::WindowsMsi;
    if (releases_for_relaunch) {
        ReleaseProcessSingleInstance(false);
    }
    const ApplyUpdateResult res = ApplyDownloadedUpdate(
        mcpplibs::os::fs::from_utf8(snap.downloaded_file_path),
        snap.install_kind
    );
    app_update_state.Update([&](AppUpdateState& st) {
        st.phase = res.ok ? UpdatePhase::ReadyToInstall : UpdatePhase::Error;
        st.status_message = res.install_hint.empty() ? res.message : (res.message + "\n" + res.install_hint);
        st.error_message = res.ok ? std::string{} : res.message;
    });
    ShowToastSafe(res.message, toast);
    if (res.ok && res.should_quit_app) {
        ReleaseProcessSingleInstance(true);
        force_quit_requested = true;
        application.Quit();
    } else if (releases_for_relaunch) {
        (void)AcquireProcessSingleInstance();
    }
}

} // namespace hxlings

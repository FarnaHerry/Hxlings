# Hxlings 已知坑点与避坑清单

1. **非 Composable 文件中的 `"Use"` 字串会触发 HuxerUI `hcg` 误扫描**
   - **现象**：构建报 `hcg` 代码生成或类型检查异常，即使只是注释或标识符中包含 `User`、`UseState` 等词。
   - **规避**：非 UI 文件（`build.mcpp`、`xlings_*.cppm`、`store.cppm`、`shortcuts.cppm`、`app_settings.cppm`、`platform_icon.cppm`）完全禁止出现区分大小写的 `Use` 子串，提交前用 `rg -n "Use" ...` 核验。

2. **修改 `.cppm` 新增 `import` 后报 `module-graph divergence`**
   - **现象**：仅修改既有 `.cppm` 文件头部新增 `import ...` 后直接 `mcpp build`，P1689 扫描与增量构建图声明不一致而失败（`touch build.mcpp` 也不足以清掉旧缓存）。
   - **解决**：执行 `mcpp clean && mcpp build`。

3. **HuxerUI 同级动态子视图重复 `.Key(...)` 引发重组崩溃**
   - **现象**：列表数据含同名项或条件切换同级节点无稳定键时抛出 `HuxerUI sibling views must not use duplicate keys`。
   - **解决**：循环构造的行/卡片统一附加唯一索引与业务主键组合键（如 `.Key("pkg-" + g.key + "-" + std::to_string(i))`）。

4. **`WindowHandle` 无默认构造函数**
   - **现象**：直接在结构体中写 `WindowHandle main_window{};` 无法编译（仅有一个私有/显式服务指针构造器）。
   - **解决**：统一存储为 `std::optional<WindowHandle> main_window;`。

5. **`SystemTrayHandle::OnActivate` 重复注册抛出 `std::logic_error`**
   - **现象**：在每次重组的 `Lifecycle` 中无条件调用 `tray.OnActivate(...)` 会在二次重组时抛异常。
   - **解决**：使用 `store->tray_activate_registered` 布尔守卫确保整个应用生命周期仅注册一次。

6. **`mcpp run` 环境污染子进程 `XLINGS_HOME`**
   - **现象**：通过 `mcpp run` 启动 GUI 时，环境变量中的 `XLINGS_HOME` 与 `PATH` 包含 `~/.mcpp/registry`，若直接继承会导致 `xlings` CLI 操作错环境。
   - **解决**：`src/xlings_process.cppm` 解析 `xlings` 二进制及构建子进程环境时跳过含 `/.mcpp/registry` 或 `\.mcpp\registry` 的条目（可用 `HXLINGS_BIN` 显式覆盖可执行路径）。

7. **非 macOS 链接阶段误引用 `objc::id` 析构符号**
   - **现象**：在普通函数内的 `if constexpr (os::is_macos)` 中直接构造 `mcpplibs::os::macos::objc::id` 或 `autorelease_pool`，某些编译器仍可能生成 Linux/Windows 下未定义的 `objc` 析构符号引用。
   - **解决**：像 `src/platform_icon.cppm` 那样将平台实现封装在 `template <bool Enabled>` + `if constexpr (Enabled)` 中，以 `<os::is_macos>` 等编译期常量实例化。

8. **`UiTest` 下运行非测试逻辑导致异常或写脏用户配置**
   - **现象**：`tests/ui_layout_test.cpp` 中托盘宿主不可用且设置了 `store->skip_initial_refresh = true`。
   - **规避**：涉及磁盘持久化（`SaveAppSettings`）、原生托盘（`tray.Show` / `tray.OnActivate`）及原生图标（`ApplyPlatformWindowIcon`）时均需判断 `!store->skip_initial_refresh`。

9. **Bash `set -o pipefail` 下 `tar -tzf ... | grep -q` 触发 SIGPIPE（141）**
   - **现象**：CI 校验归档内容时 `grep -q` 提前关闭管道导致 `tar` 被 SIGPIPE 终止、流水线失败。
   - **解决**：先用变量捕获完整列表 `tar_list=$(tar -tzf "$tar_out")`，再对变量执行 `grep -q`。

10. **轻量级模式下关闭窗口后的 `ToastHandle` 与 `shared_ptr` 自引用环风险**
    - **现象**：应用级回调直接捕获 `shared_ptr<XlingsStore>` 且挂在 `store->on_tray_state_changed` 上会阻止释放；后台任务结束后向已退役窗口调用 `ToastHandle::Show` 可能触发异常。
    - **解决**：`on_tray_state_changed` 与托盘菜单统一捕获 `std::weak_ptr<XlingsStore>`；`MainShell` 卸载清理 `active_toast` 与 `main_window`，后台任务统一经 `ShowToastSafe` 提示。

11. **`Lifecycle`/`OnShortcut` 依赖捕获大于 16B 的结构体触发 Clang 22 `std::__exception_guard` 二义性**
    - **现象**：将 `huxerui::MotionScheme` 等较大值类型直接作为 `Lifecycle`/`OnShortcut` 变参依赖传入 `std::any` 时，跨 `std` 与 `huxerui` 模块实例化 `_LargeHandler::__create` 会报错 `ambiguous deduction for template arguments of 'std::__exception_guard'`。
    - **解决**：依赖参数仅传入标量/基础类型字段（如 `theme.motion.normal`, `theme.motion.reduced_motion`）走 `std::any` 小对象优化路径。

12. **Linux `mcpp pack --mode vendored` 依赖宿主 `/lib64/libm.so.6` 导致 `GLIBC_2.44` 启动失败**
    - **现象**：Linux 包基于 `xim:*`（如 `libsqlite3.so.0` 要 `GLIBC_2.44`、`libgtk-4.so.1`/`libglib-2.0.so.0` 要 `GLIBC_2.43`）构建；默认 `vendored` 不带 `libc.so.6`/`libm.so.6`/`ld-linux-x86-64.so.2`，安装到宿主 Linux 后因宿主 glibc 更旧而无法启动。
    - **解决**：`.tar.gz` 与 `.AppImage` 使用 `--mode self-contained` 并由 `MCPP_BUNDLE_DIR` 推导 `HUXERUI_RESOURCES_DIR`，且在 `src/xlings_process.cppm` 清理 `GCONV_PATH`、`LOCPATH`、`LD_LIBRARY_PATH` 与 `MCPP_BUNDLE_DIR`；`.deb` 与 `.rpm` 则改用 `--mode system` 声明发行版原生 GTK4 依赖，同时 `build.mcpp` 生成 `glibc_compat_x86_64.c`（`--wrap=sqrtf/atan2f/remainder` 绑定 `GLIBC_2.2.5`）将可执行文件最高符号版本降至 `GLIBC_2.38`。

13. **Linux `--mode self-contained` 找不到宿主 `libGL`/`libGLX_mesa` 退化为 GTK4 Cairo 粉色方块**
    - **现象**：自包含包默认的 `ld-linux-x86-64.so.2 --library-path "$here/lib"` 不读宿主 `/etc/ld.so.cache` 与 `/lib64`，导致 `libepoxy` `dlopen(libEGL/libGL)` 失败（`Not using GL: libEGL not available`），GTK4 退化为 `GskCairoRenderer` 并把 `gtk_snapshot_push_fill` 节点渲染成 `#FF69B4` 粉色矩形。
    - **解决**：将 `--library-path` 修补为 `"$here/lib:/usr/lib/x86_64-linux-gnu:/usr/lib64:/lib/x86_64-linux-gnu:/lib64"`（包内优先、宿主 GPU 库兜底），使 GTK4 正常启用 `GskNglRenderer`（`AppImage` 在 `build.mcpp` 修补，`tar` 在 CI 重打包修补）。

14. **Windows `mcpp.dist.wix` 默认模板缺失 `<MediaTemplate EmbedCab="yes" />` 产出 36 KB 空壳 `.msi`**
    - **现象**：WiX v5 默认 `EmbedCab="no"`，把实际文件压入同目录外部 `cab1.cab`，单独上传 `.msi` 后仅 36 KB 且无法安装。
    - **解决**：由 root `build.mcpp` 生成自定义 `Hxlings-package.wxs`（显式声明 `<MediaTemplate EmbedCab="yes" />`、`app.ico`、`ARPPRODUCTICON` 与开始菜单/桌面快捷方式），并在 Windows CI 校验 `Media.Cabinet` 以 `#` 开头且独立 `msiexec /a`（不带外部 `.cab`）能完整解出二进制与 `resources.bin`。

15. **Windows GUI 进程调用 `mcpplibs::os::process` 拉起控制台 `xlings.exe` 连续弹出终端窗口**
    - **现象**：`hxlings.exe` 为 `windows_subsystem = "windows"`，通过当前 `mcpplibs.os`（无 `CREATE_NO_WINDOW` 选项）调用控制台 `xlings.exe` 时，Windows 为每个子进程弹出可见终端（启动 5 路并发 `RefreshCore` 会连闪一堆终端）。
    - **解决**：`src/xlings_process.cppm` 在 `os::is_windows` 下改由 `kernel32.dll` 动态调用 `CreateProcessW` 并传入 `CREATE_NO_WINDOW`（`0x08000000`）+ `SW_HIDE` + Job Object（`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`）；并发创建管道/可继承句柄区间须加 `spawn_mutex`，防止并发子进程误继承写端导致 `PeekNamedPipe` 阻塞不返回 EOF。

16. **`xlings interface probe_url` 对 `socket_timeout_ms > 3000` 返回 `exit 2`**
    - **现象**：向 `xlings interface probe_url` 传入 `{"url":..., "socket_timeout_ms":5000}`（或 `<= 0`）会直接以 `probe_url failed (exit 2) with no error detail on the stream` 退出，而不传入该字段或传入 `1..3000` 则正常探测并返回 `network_probe` 事件。
    - **解决**：`BuildProbeUrlArgs` 默认省略 `socket_timeout_ms`（沿用 CLI 内建默认值），显式传正数时限制在 `[100, 3000]`；`XlingsClient::ProbeUrl` 在无 `network_probe` 数据时兜底保留 `outcome.error_message` / `exit_code`。

17. **桌面启动器环境缺少 `~/.xlings` 目录导致 `xlings self update --yes` 退出 `127`**
    - **现象**：Linux/Windows 从桌面入口或系统应用菜单启动 GUI 时，继承的 `PATH` 通常仅含 `/usr/local/bin:/usr/bin:/bin`；调用 `xlings self update` 内部会通过 `sh` / `PATH` 查找裸命令 `xlings`，触发 `xlings: command not found` 并返回 `exit 127`。
    - **解决**：`src/xlings_process.cppm` 的 `BuildSanitizedEnvOverrides` 与 `BuildSanitizedSearchPath` 在 Linux/macOS/Windows 全平台把当前执行二进制所在目录及 `$XLINGS_HOME`/`$HOME/.xlings` 的 `subos/current/bin`、`bin` 前置注入子进程 `PATH`（并继续剔除 `.mcpp/registry` 与 `MCPP_BUNDLE_DIR` 前缀）。

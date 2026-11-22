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
    - **解决**：Linux 打包全部使用 `--mode self-contained`；因随附动态链接器启动使 `/proc/self/exe` 指向 `ld-linux-x86-64.so.2`，需在 `PreparePlatformStartup()` 与 `ResolveAppIconPngPath()` 读取 `MCPP_BUNDLE_DIR` 设置 `HUXERUI_RESOURCES_DIR`，并在 `src/xlings_process.cppm` 清理自包含封装器导出的 `GCONV_PATH`、`LD_LIBRARY_PATH` 与 `MCPP_BUNDLE_DIR`。

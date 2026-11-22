---
name: huxerui-cpp-modules
description: >-
  Use this skill when creating or refactoring pure C++23 module files in src/,
  writing HuxerUI composable views/state/dialogs/shortcuts/system-tray logic,
  debugging hcg codegen or P1689 module-graph divergence errors, or extending
  tests/ui_layout_test.cpp layout audits.
---

# HuxerUI & Pure C++23 Modules Development Skill

本指南覆盖 Hxlings 项目在 **纯 C++23 Modules + HuxerUI 0.5.0 + mcpplibs.os** 约束下的日常开发与 UI/任务队列对齐流程，不重复 `AGENTS.md` 中的顶层红线。

## 0. 衔接 HuxerUI 官方 Skill（按需查阅）

修改 UI 或任务队列前，先定位当前依赖检出目录中的 HuxerUI 官方文档：
```bash
find ~/.mcpp/git -path '*/skills/huxerui-app-development/SKILL.md'
```
- UI 布局 / 主题 / 窗口 / 托盘：`references/theme-animation-presentation.md`（含 `UseSystemThemeBrightness()`）与 `references/navigation-and-window.md`（含 `quit_on_last_window_closed`、`OpenWindow`、`SystemTrayHandle`）。
- 异步任务队列 / 生命周期 / 回调：`references/fundamentals.md` 中的 `Lifecycle and tasks` 与 `Callback integration`。

## 1. 纯 C++23 Modules 与跨平台系统调用

1. **禁止头文件与预处理宏**：
   - `src/` 下文件统一使用 `export module ...;`、`import ...;`。
   - 平台差异一律使用 `if constexpr (mcpplibs::os::is_linux / is_macos / is_windows / is_posix)` 处理，而非 `#ifdef`。
   - 涉及平台专属结构体或非跨平台实现（如 `mcpplibs::os::macos::objc`），放入模板函数 `template <bool Enabled>` 中配合 `if constexpr (Enabled)`，避免跨平台链接符号缺失。
   - 不使用系统头文件里的 C ABI 时，通过 `mcpplibs::os::library::loaded(...)` / `open(...)` / `process_symbol(...)` 按函数指针签名动态解析。
2. **文件写入/目录/环境变量**：
   - 使用 `mcpplibs::os::dirs::config()` / `home()` / `temp()` 解析标准路径；持久化统一使用 `mcpplibs::os::fs::write_atomic(...)`，UTF-8 路径转换使用 `mcpplibs::os::fs::utf8(...)`。
3. **非 Composable 文件命名守卫**：
   - 确保非视图文件（尤其新增工具模块与状态文件）全文不包含区分大小写的 `Use` 子串，提交前运行：
     `rg -n "Use" build.mcpp src/xlings_*.cppm src/store.cppm src/shortcuts.cppm src/app_settings.cppm src/platform_icon.cppm`

## 2. HuxerUI 组件与资源约定

1. **组件签名**：凡调用 `UseService`、`UseTheme`、`UseState`、`UseWindow`、`UseDialog`、`UseToast`、`UseApplication`、`UseApplicationTaskScope`、`UseViewHandle` 或 `Lifecycle` 的视图函数必须标注 `[[huxerui::composable]]`。
2. **句柄、任务队列与轻量级生命周期**：
   - `WindowHandle` 无默认构造器，保存到成员时使用 `std::optional<WindowHandle>`，并在 `MainShell` 挂载/卸载 `Lifecycle` 中配对绑定与重置（同步管理 `active_toast`，配合 `ShowToastSafe` 防止窗口销毁后误触发）。
   - `Application` 设置 `.quit_on_last_window_closed = false`，系统托盘（`SystemTrayHandle`）与 `ensure_main_window`（`OpenWindow` 重建）由 `InstallApplication` 应用级钩子持有，禁止用捕获 `shared_ptr<XlingsStore>` 自循环，应使用 `std::weak_ptr`。
   - **任务作用域区分**：跨窗口生命周期的后台刷新与写入使用 `UseApplicationTaskScope()`；仅作用于当前窗口/页面的聚焦、弹窗等调度使用 `UseTaskScope()`（随卸载自动取消）。
   - **串行与可覆盖取消**：串行写操作使用 `WorkerSequence::Run()` + `std::stop_token`（通过 `std::stop_callback` 桥接子进程 `ActiveProcessControl::Cancel()`）；高频可覆盖读请求保存 `TaskHandle`，新请求先调用 `handle.Cancel()`。
   - **三态主题**：使用 `ThemeMode`（`Auto`/`Light`/`Dark`），在 `AppRoot()` 中于 `Auto` 模式下读取 `UseSystemThemeBrightness()`；切勿在组合阶段写入 `State`。
3. **内建编译资源**：`mcpp.toml` 配置 `resources = "resources"` 后，模块系统生成 `app.resources`，在 UI 模块中通过 `import app.resources;` 访问 `app::images::hxlings` 与 `app::images::tray_icon`。

## 3. UI 布局测试（`tests/ui_layout_test.cpp`）扩展流程

1. 构造离线数据使用 `MakeSeededStore()`，须设置 `skip_initial_refresh = true` 并预置已加载标志（`initial_loaded`、`indexes_loaded`、`index_versions_loaded`、`xlings_config_loaded`、`profiles_loaded`）与默认 `InitAppSettings(AppSettings{})`。
2. 新增页面或标签页后，必须在 `AuditAllPagesAtViewport` 中同时覆盖 `1180x760` 与最小窗口尺寸 `880x560`（以及 `1440x900` 抽检），并用 `SafeTap` 切换标签或弹出对话框后调用 `AuditCurrentScreen(...)`。
3. 所有状态修改需确保通过 `SetDarkMode` 等带 `skip_initial_refresh` 保护的方法进行，避免测试污染本机 `~/.config/hxlings/settings.json`。
4. 修改模块 `import` 列表或 `src/*.cppm` 后运行验证：
   ```bash
   mcpp clean && mcpp build && mcpp test
   ```
   确认输出 `UI layout test passed with 0 anomalies.` 与 `2 passed; 0 failed`。

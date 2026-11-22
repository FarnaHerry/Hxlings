# Hxlings 架构记忆

## 1. 模块分层（Pure C++23 Modules）

- **CLI 适配与进程层**：
  - `xlings.types`（`src/xlings_types.cppm`）：核心领域结构体（包、SubOS、索引、配置、Profile、搜索结果等）。
  - `xlings.parser`（`src/xlings_parser.cppm`）：NDJSON / JSON 解析、空白规范化、`SplitPackageSpec`、版本比较及批量清理/更新计划推导。
  - `xlings.process`（`src/xlings_process.cppm`）：基于 `mcpplibs.os` 定位 `xlings` 二进制并流式派生子进程，支持 `ActiveProcessControl` 取消。
  - `xlings.client`（`src/xlings_client.cppm`）：封装所有 `xlings` 读写命令，将进程输出映射为强类型结果或结构化事件。
- **客户端基础设施层**：
  - `hxlings.settings`（`src/app_settings.cppm`）：客户端本地 JSON 配置持久化（`os::dirs::config()/hxlings/settings.json`）。
  - `hxlings.platform_icon`（`src/platform_icon.cppm`）：Linux/macOS/Windows 启动环境适配与应用/窗口图标设置。
  - `hxlings.shortcuts`（`src/shortcuts.cppm`）：macOS（`⌘`）与 Linux/Windows（`Ctrl`）跨平台快捷键映射与帮助弹窗数据。
  - `hxlings.theme`（`src/theme.cppm`）与 `hxlings.icons`（`src/icons.cppm`）：主题色板（含 `BrandPresets()`）、语义状态色与内联矢量图标。
  - `app.resources`（由 `hrc` 自动生成）：基于 `resources/images/` 生成 `app::images::hxlings` 与 `app::images::tray_icon`。
- **状态与视图层**：
  - `hxlings.store`（`src/store.cppm`）：统一应用状态容器 `XlingsStore`（继承 `std::enable_shared_from_this`）与异步任务编排。
  - `hxlings.components`（`src/components.cppm`）、`hxlings.console`（`src/task_console.cppm`）及 6 个页面模块 `hxlings.page.{dashboard,packages,store,subos,indexes,settings}`。
  - `app`（`src/app.cppm`）与入口 `src/main.cpp`（调用 `PreparePlatformStartup()` 后进入 `huxerui::RunApplication()`）。

## 2. 渐进式并发加载与按需懒加载

- **首屏快速启动（`RefreshCore`）**：启动时并发发射 5 组互不依赖的只读查询：
  1. `QueryVersion` + `QueryEnv`：约 10 ms 内返回并立即将 `initial_loaded` 置为 `true`，解除首屏占位。
  2. `QuerySystemStatus`：系统健康告警统计。
  3. `ListInstalledPackages` + `GroupInstalledPackages`：已安装列表；仅在当前处于已安装管理页（`nav == 1`）时按需触发选中包详情查询。
  4. `ListSubos` + `ListSubosShims`：SubOS 列表与 Shims；仅在处于 SubOS 页（`nav == 3`）时触发 `LoadSubosDetail`。
  5. `ListRepos` + `QuerySelfLog(25)`：仓库概览与最近日志。
- **懒加载域（按需后台拉取）**：
  - 已安装详情（`EnsureInstalledDetailLoaded`）与 SubOS 详情（`EnsureSubosDetailLoaded`）分别在切换到 `nav == 1` / `nav == 3` 时按需加载。
  - 索引页面（`nav == 4`）通过 `RefreshIndexes` 拆分轻量仓库/本地 xpkg 查询与耗时较长（实测约 2.36 s）的 `ListIndexVersions()` 远程快照查询，均不阻塞 UI 交互。
  - `xlings` 全局配置（`RefreshXlingsConfig`）与 `Profile` 代际（`RefreshProfiles`）分别在进入设置页标签 1 与标签 2 时按需加载。
- **定向刷新**：各写操作完成后仅调用受影响域的刷新函数（如修改仓库/快照仅调用 `RefreshIndexes(t, true)`，修改 `xlings config` 仅调用 `RefreshXlingsConfig(t, true)`）。

## 3. 配置解耦与托盘/Logo 管线

- **双配置域隔离**：
  - `SettingsPage` 分为 3 个标签页：`Hxlings 应用设置`（Tab 0）| `xlings 全局配置`（Tab 1）| `环境诊断与维护`（Tab 2）。
  - Tab 0 直接写入 `AppSettings`（三态主题 `theme_mode`：`Auto`/`Light`/`Dark` 并通过 `CircularRevealTransition` 实现圆形扩散切换特效、强调色、`tray_enabled`、`close_to_tray`、`lightweight_mode`、快捷键查看入口及 `关于` 版本信息卡片），0 ms 读取且无 CLI 进程开销；Tab 1/2 对应 `xlings` CLI 状态。
- **应用级托盘与轻量级模式**：
  - `Application` 启用 `.quit_on_last_window_closed = false`，托盘呈现与 `OnActivate` 重建窗口控制统一安装在 `InstallApplication` 阶段，通过 `std::weak_ptr<XlingsStore>` 避免循环引用；`MainShell` 卸载清理 `main_window` 与 `active_toast`。
  - 开启“轻量级模式”（依赖托盘开启）后，点击关闭窗口会直接放行原生窗口关闭与 UI/渲染资源销毁，仅保留应用级托盘与后台 `ApplicationTaskScope`；点击托盘时通过 `application.OpenWindow(App, ...)` 秒开恢复。
  - 普通后台写入通过 `XlingsStore::mutation_sequence_`（`WorkerSequence` + `std::stop_token`）串行执行；托盘菜单“退出 Hxlings”与 macOS `⌘Q` 先置位 `store->force_quit_requested = true` 再调用 `application.Quit()`。
- **统一品牌图标资产**：从 `platform/linux/package/hxlings.svg` 生成 `resources/images/hxlings.png`、`resources/images/tray_icon.png`、`platform/windows/app.ico` 与 `platform/macos/AppIcon.icns`，并在标题栏、任务栏/Dock、系统托盘与三端安装包间共用。

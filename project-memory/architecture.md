# Hxlings 架构记忆

## 1. 模块分层（Pure C++23 Modules）

- **CLI 适配与进程层**：
  - `xlings.types`（`src/xlings_types.cppm`）：核心领域结构体（包、SubOS、索引、配置、Profile、搜索结果等）。
  - `xlings.parser`（`src/xlings_parser.cppm` 聚合 `src/xlings_parser_{common,packages,subos}.cppm`）：NDJSON / JSON 解析、空白规范化、`SplitPackageSpec`、版本比较、时间戳格式化及批量清理/更新计划推导。
  - `xlings.process`（`src/xlings_process.cppm`、`src/xlings_process_env.cppm`、`src/xlings_process_win32.cppm`）：基于 `mcpplibs.os` 定位 `xlings` 二进制并流式派生子进程（支持 `ActiveProcessControl` 取消与 Windows `CreateProcessW`/`CREATE_NO_WINDOW` 无闪烁启动，并用 `Win32EnvGuard`/`Win32HandleGuard`/`PidResetGuard` 保证异常安全），并提供跨平台默认浏览器拉起 `OpenExternalUrl`、本地路径打开 `OpenLocalPath` 与脱离式重启 `LaunchDetachedExecutable`。
  - `xlings.client`（`src/xlings_client.cppm`）：封装所有 `xlings` 读写命令，将进程输出映射为强类型结果或结构化事件。
- **LubanOS 与多环境编排层**：
  - `hxlings.luban.types`（`src/luban_types.cppm`）：宿主门控/模式/缺失工具状态、Luban 环境生命周期、官方发行版/桌面目录与蓝图清单（`LubanStackSpec` / `LubanStackPlan` / `SavedLubanStackInfo`）结构体。
  - `hxlings.luban.client`（`src/luban_client.cppm`）：定位 `luban` 与 `luban-os`，实现容错 JSON 解析（剥离前置 `[warn]` 行并解包 `{"result":...}`）、CLI 参数构建、只读查询、流式生命周期命令及 `luban-os` 分离式拉起。
  - `hxlings.luban.stack`（`src/luban_stack.cppm`）：声明式多环境编排引擎（`hxlings.luban-stack/v1`），提供 JSON / TOML 自动解析与往返序列化、合法性校验、与当前环境的最小增量 `Diff` 计划推导（`BuildLubanStackPlan`）、从现有环境导出（`ExportEnvironmentsToStack`）、内置精选模板及本地目录 `os::dirs::config()/hxlings/luban-stacks` 持久化 CRUD。
- **客户端基础设施层**：
  - `hxlings.settings`（`src/app_settings.cppm`）：客户端本地 JSON 配置持久化（`os::dirs::config()/hxlings/settings.json`）。
  - `hxlings.platform_icon`（`src/platform_icon.cppm`）：Linux/macOS/Windows 启动环境适配、应用/窗口图标设置及原生主题同步 `ApplyPlatformThemeMode`（Windows `uxtheme.dll`/`dwmapi.dll`、macOS `[NSApp setAppearance:]`、Linux GTK `gtk-application-prefer-dark-theme`）。
  - `hxlings.single_instance`（`src/single_instance.cppm`）：基于 `mcpplibs.os.ipc` 本地 rendezvous 套接字的跨平台单实例守护与二次启动唤醒协议（Windows 通过 `AllowSetForegroundWindow` 授予主实例前台权限；支持应用退出与自更新重启提前释放）。
  - `hxlings.updater`（`src/app_updater.cppm`）：GitHub Releases 自更新核心（`kAppVersion`、Release/Asset 解析、FIPS 180-4 SHA-256 校验、平台包选型与安装/AppImage 原子替换拉起）。
  - `app.resources`（由 `hrc` 自动生成）：基于 `resources/images/` 生成 `app::images::hxlings` 与 `app::images::tray_icon`。
- **状态层**：
  - `hxlings.store`（`src/store.cppm` 接口及 `src/store_{refresh,packages,subos,luban,system,updater}.cpp` 模块实现单元）：统一应用状态容器 `XlingsStore`（继承 `std::enable_shared_from_this`）与异步任务编排。
- **UI 视图层（`src/ui/`）**：
  - `hxlings.shortcuts`（`src/ui/shortcuts.cppm`）：macOS（`⌘`）与 Linux/Windows（`Ctrl`）跨平台快捷键映射与帮助弹窗数据。
  - `hxlings.theme`（`src/ui/theme.cppm`）与 `hxlings.icons`（`src/ui/icons.cppm`）：主题色板（含 `BrandPresets()`）、语义状态色与内联矢量图标。
  - `hxlings.components`（`src/ui/components.cppm`）、`hxlings.console`（`src/ui/task_console.cppm`）及 7 个页面模块 `hxlings.page.{dashboard,packages,store,subos,luban,indexes,settings}`（其中大型页面已按对话框/标签页/卡片拆分至 `page_*_{dialogs,tabs,cards,env_tab,catalog_studio}.cppm`）。
  - `app`（`src/ui/app.cppm`）与入口 `src/main.cpp`（先通过 `ProcessSingleInstanceScope::AcquireDefault()` 获取单实例控制权，再调用 `PreparePlatformStartup()` 后进入 `huxerui::RunApplication()`）。

## 2. 渐进式并发加载与按需懒加载

- **首屏快速启动（`RefreshCore`）**：启动时并发发射 5 组互不依赖的只读查询：
  1. `QueryVersion` + `QueryEnv`：约 10 ms 内返回并立即将 `initial_loaded` 置为 `true`，解除首屏占位。
  2. `QuerySystemStatus`：系统健康告警统计。
  3. `ListInstalledPackages` + `GroupInstalledPackages`：已安装列表；仅在当前处于已安装管理页（`nav == 1`）时按需触发选中包详情查询。
  4. `ListSubos` + `ListSubosShims`：SubOS 列表与 Shims；仅在处于 SubOS 页（`nav == 3`）时触发 `LoadSubosDetail`。
  5. `ListRepos` + `QuerySelfLog(25)`：仓库概览与最近日志。
- **懒加载域（按需后台拉取）**：
  - 已安装详情（`EnsureInstalledDetailLoaded`）与 SubOS 详情（`EnsureSubosDetailLoaded`）分别在切换到 `nav == 1` / `nav == 3` 时按需加载。
  - `LubanOS`（`nav == 4`）通过 `RefreshLuban`、`LoadLubanDetail` 与 `RefreshSavedLubanStacks` 在进入页面时按需探测宿主后端/环境/官方目录及本地 `~/.config/hxlings/luban-stacks` 蓝图库，`RefreshCore` 绝不触发 `luban` 子进程；多环境蓝图应用（`ApplyLubanStackAsync`）与停止（`StopLubanStackAsync`）经由 `mutation_sequence_` 串行执行并支持实时配置同步（`SyncLubanConfigsForStackAsync`）与 `.tar`/`.luban`/`.qcow2`/`.iso` 导出桥接（含 Podman 导入命令）。
  - 索引页面（`nav == 5`）通过 `RefreshIndexes` 拆分轻量仓库/本地 xpkg 查询与耗时较长（实测约 2.36 s）的 `ListIndexVersions()` 远程快照查询，均不阻塞 UI 交互。
  - `xlings` 全局配置（`RefreshXlingsConfig` / `RefreshConfigThemes`）、网络端点与诊断（`RefreshNetworkEndpoints`）、以及 `Profile` 代际（`RefreshProfiles`）分别在进入设置页（`nav == 6`）标签 1 与标签 2 时按需加载。
  - SubOS 治理面（`LoadSubosGovernance`：合并 `status`/`config`/`ps`/`requests`/`report`/`info`/`doctor` 查询）仅在切换到 `SubOS` 页“策略”/“会话”标签或手动刷新时按 `hash(subos_name)` 懒加载。
- **定向刷新**：各写操作完成后仅调用受影响域的刷新函数（如修改仓库/快照仅调用 `RefreshIndexes(t, true)`，修改 `xlings config` 仅调用 `RefreshXlingsConfig(t, true)`）。

## 3. 配置解耦与托盘/Logo 管线

- **双配置域隔离**：
  - `SettingsPage` 分为 3 个标签页：`Hxlings 应用设置`（Tab 0）| `xlings 全局配置`（Tab 1）| `环境诊断与维护`（Tab 2）。
  - Tab 0 直接写入 `AppSettings`（三态主题 `theme_mode`：`Auto`/`Light`/`Dark` 并通过 `CircularRevealTransition` 实现圆形扩散切换特效，同时经 `ApplyPlatformThemeMode` 同步原生菜单/窗口深浅色；含强调色、`tray_enabled`、`close_to_tray`、`lightweight_mode`、快捷键查看入口及带检查更新/SHA-256 校验与平台安装动作的 `关于` 卡片），0 ms 读取且无 CLI 进程开销；Tab 1/2 对应 `xlings` CLI 状态。
- **应用级托盘与轻量级模式**：
  - `Application` 启用 `.quit_on_last_window_closed = false`，托盘呈现与 `OnActivate` 重建窗口控制统一安装在 `InstallApplication` 阶段，通过 `std::weak_ptr<XlingsStore>` 避免循环引用；`MainShell` 卸载清理 `main_window` 与 `active_toast`。
  - 开启“轻量级模式”（依赖托盘开启）后，点击关闭窗口会直接放行原生窗口关闭与 UI/渲染资源销毁，仅保留应用级托盘与后台 `ApplicationTaskScope`；点击托盘时通过 `application.OpenWindow(App, ...)` 秒开恢复。
  - 普通后台写入通过 `XlingsStore::mutation_sequence_`（`WorkerSequence` + `std::stop_token`）串行执行；托盘菜单“退出 Hxlings”与 macOS `⌘Q` 先置位 `store->force_quit_requested = true` 再调用 `application.Quit()`。
- **统一品牌图标资产**：从 `platform/linux/package/hxlings.svg` 生成 `resources/images/hxlings.png`、`resources/images/tray_icon.png`、`platform/windows/app.ico` 与 `platform/macos/AppIcon.icns`，并在标题栏、任务栏/Dock、系统托盘与三端安装包间共用。

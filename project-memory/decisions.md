# Hxlings 关键设计决策（ADR）

## ADR-001：`src/` 全面采用纯 C++23 Modules（零宏与头文件）
- **背景**：早期 `src/main.cpp` 与平台代码混杂 `#if`、`#include` 及 MSVC `#pragma comment(linker, ...)`，既破坏模块纯洁性，又在多平台构建时增加碎片化。
- **决策**：由 `mcpp.toml` 的 `[targets.hxlings]` 声明 `kind = "app"`、`windows_subsystem = "windows"`、`windows_entry = "main"` 及 `[resources] icon`，并使用 `mcpplibs.os`（`os::process`、`os::library`、`os::dirs`、`os::fs`、`os::macos::objc`）替代原生头文件与条件编译宏。
- **结果**：`src/` 保持 0 条预处理指令；`tests/ui_layout_test.cpp` 仍因 HuxerUI 测试宏/头接口保留 `#include`。

## ADR-002：macOS 开发构建跳过 `bundle:*` 边缘以保证 `mcpp test` 通过
- **背景**：HuxerUI 默认规则在 macOS 上生成的 `bundle:*` 规则引用 `${mcpp.target_file:hxlings}`，单独执行 `mcpp test`（未编译 app 主目标）时会阻塞失败。
- **决策**：在 `build.mcpp` 的 `configure_macos()` 中过滤 `e.id.starts_with("bundle:")`，同时保留 `HuxerUI/` 编排资源部署与 `mcpp pack` 发布阶段 `mcpp::dist::apple` 打包。

## ADR-003：Windows CI 固定使用 `windows-2022` 运行器
- **背景**：`windows-latest` 附带的 MSVC STL 14.51 在 Clang 编译 HuxerUI 时触发 `std::_Find_vectorized` 对 `NodeExtensionHandle` 的 `static_assert` 失败。
- **决策**：`.github/workflows/ci.yml` 中 Windows Job 明确固定 `runs-on: windows-2022`。

## ADR-004：Linux 混合会话默认回退 `GDK_BACKEND=x11`
- **背景**：部分 Linux Wayland 合成器下 `gtk4@4.16.13` 存在 `zwp_linux_dmabuf_feedback_v1` 初始化崩溃问题。
- **决策**：`PreparePlatformStartup()` 在未显式设置 `GDK_BACKEND` 且同时存在 `WAYLAND_DISPLAY` 与 `DISPLAY` 时，通过 `setenv` 将 `GDK_BACKEND` 设为 `x11`（并清除 `EGL_PLATFORM=wayland`），将 `INTEL_FORCE_PROBE` 设为 `*`；用户仍可通过 `GDK_BACKEND=wayland mcpp run` 显式覆盖。

## ADR-005：将 `list_index_versions` 从启动全量刷新拆出并解耦客户端设置
- **背景**：本地基准测试显示串行 `RefreshAll` 总耗时约 3.17 s，其中 `interface list_index_versions` 单项耗时约 2.36 s，拖慢全部首屏状态更新；同时客户端主题配置与 `xlings` 配置混在同一页面，造成误解与冗余刷新。
- **决策**：启动改为 5 路并发 `RefreshCore`（~10 ms 呈现骨架，约 ~165 ms 核心数据就绪）；索引快照、包/SubOS 详情及 `config`/`profile` 均按页面/标签页按需加载；应用外观与托盘配置独立持久化至 `settings.json`。

## ADR-006：macOS 原生窗口快捷键在应用层显式绑定
- **背景**：验证 HuxerUI 0.5.0 `appkit_adapter.mm` 后确认框架未创建默认 `NSApp.mainMenu`，导致系统级 `⌘Q` / `⌘W` / `⌘M` 不会自动生效。
- **决策**：在 `src/app.cppm` 通过 `window.OnShortcut` 在 macOS 下显式注册 `⌘W`、`⌘Q`、`⌘M` 与 `⌃⌘F`；配合托盘关闭拦截机制，`⌘Q` 先设置 `force_quit_requested = true` 后调用 `application.Quit()`。

## ADR-007：任务队列与轻量化托盘模式全面对齐 HuxerUI 0.5.0 官方规范
- **背景**：窗口关闭销毁场景下，托盘控制、提示弹窗（`ToastHandle`）和页面聚焦调度不能绑定在已退役的 `UiWindow` 生命周期上，同时需标准串行队列与可取消只读任务语义。
- **决策**：采用 `quit_on_last_window_closed = false` + `InstallApplication` 应用级托盘与 `OpenWindow` 窗口按需重建实现“轻量级模式”；串行写操作使用 `WorkerSequence::Run()` + `std::stop_token`，可覆盖读请求持有 `TaskHandle::Cancel()`，页面内聚焦与弹窗使用 `UseTaskScope()`。

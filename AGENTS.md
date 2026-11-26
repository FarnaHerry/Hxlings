# Hxlings 仓库工程规范

> **单一事实来源（SSOT）**：所有在本仓库工作的 AI Agent 与开发者均以本文件为最高协作规范；`CLAUDE.md` 通过 `@AGENTS.md` 引用本文件。

## 1. 项目概览

- Hxlings 是基于 C++23 Modules、`mcpp`、`HuxerUI`、`mcpplibs.os` 与 `nlohmann.json` 构建的跨平台（Linux / macOS / Windows）`xlings` 桌面 GUI 管理器。
- 图标与平台打包清单由 `mcpp.toml` 声明，`build.mcpp` 负责接入 HuxerUI 代码生成与三端资源部署。

## 2. 仓库结构地图

- `src/`：纯 C++23 Modules 应用核心（入口 `main.cpp`、`xlings` 适配、`luban` 引擎、状态容器 `store` 与系统能力）
- `src/ui/`：UI 视图与样式模块（`app.cppm`、主题、图标、快捷键、公共组件、控制台与各功能页面）
- `tests/`：解析/集成测试（`xlings_parser_test.cpp`、`xlings_runtime_test.cpp`）与无头布局测试（`ui_layout_pages_test.cpp`、`ui_layout_flows_test.cpp`）
- `platform/`：Linux 打包图标（`hxlings.svg`）、Windows 图标（`app.ico`）与 macOS 图标（`AppIcon.icns`）
- `resources/`：`hrc` 编译并随应用分发的应用及托盘 PNG 图标资源
- `project-memory/`：项目共享记忆目录（架构概览、设计决策 ADR、已验证坑点）
- `project-skills/`：项目级按需加载 Skill 目录（通过 `.agents/skills.json` 注册）
- `.github/workflows/`：三平台 CI 构建测试打包（`ci.yml`）与 Tag 发布（`release.yml`）

## 3. 工程红线

- `src/`（含 `src/ui/`）必须保持 100% 纯 C++23 Modules，禁止出现任何 `#include`、`#if`、`#pragma` 预处理指令（`tests/` 允许 `#include`）。
- UI 与非 UI 源码分离：所有页面、弹窗、主题、图标、快捷键、公共组件、控制台及 `app.cppm` 统一放在 `src/ui/` 下；非 UI 核心、状态容器与平台能力留在 `src/`。
- 单文件行数上限：所有源码与测试文件（`src/`、`src/ui/`、`tests/`、`build.mcpp`）单文件最大 **1000 行**；超过 1000 行时必须按职责拆分为独立模块或实现单元（拆出的 Composable `.cppm` 文件名 stem 须全局唯一，避免 `hcg:<stem>` 冲突）。
- 非 Composable 文件禁止出现区分大小写的 `Use` 子串（含注释与命名），涉及：`build.mcpp`、`src/xlings_*.cppm`、`src/store.cppm`、`src/store_*.cpp`、`src/ui/shortcuts.cppm`、`src/app_settings.cppm`、`src/platform_icon.cppm`。
- 动态渲染的同级视图必须使用全局唯一且稳定的 `.Key(...)`，禁止重复键导致 HuxerUI 重组崩溃。
- 启动采用并发渐进刷新（`RefreshCore`），慢速远程索引查询（`list_index_versions`）与次级页面数据按需懒加载，严禁阻塞首屏渲染。
- 客户端本地配置（`settings.json`）与 `xlings` 全局 CLI 配置解耦；外观、强调色与托盘开关变更禁止触发 `xlings` 子进程。
- 测试隔离：`skip_initial_refresh = true` 时禁止读写用户真实配置目录，禁止调用原生系统托盘或原生窗口图标 API。
- macOS Objective-C 调用与平台私有分支必须在模板化的 `if constexpr` 中隔离，禁止让仅单平台存在的符号泄漏到其他平台链接阶段。
- 禁止覆盖或强推既有发布 Git Tag（如 `v0.1.0`）。

## 4. 日常验证命令

- 纯模块、`hcg` 与单文件行数守卫（均须无任何匹配输出）：
  - `rg -n "^#" src/`
  - `rg -n "Use" build.mcpp src/xlings_*.cppm src/store.cppm src/store_*.cpp src/ui/shortcuts.cppm src/app_settings.cppm src/platform_icon.cppm`
  - `find src tests -type f \( -name '*.cpp' -o -name '*.cppm' -o -name '*.hpp' \) -exec sh -c 'for f; do n=$(wc -l < "$f"); [ "$n" -gt 1000 ] && echo "$f: $n"; done' _ {} +`
- 本地构建与完整测试：
  - `mcpp build && mcpp test`
  - 若增删 `.cppm` 导入后遇到 P1689 模块图分歧报错，先执行 `mcpp clean` 再构建。
- 便携包与资源烟测：
  - `mcpp pack hxlings --release --mode self-contained --pkg-format tar`

## 5. 共享记忆规范（`project-memory/`）

- 需要背景脉络时，先读取 `project-memory/README.md`，再按索引按需读取具体文件，不盲目全量读入。
- 当架构演进、关键设计定型或解决新坑点后，仅将简洁且经实测验证的结论增量回写至对应记忆文件。
- 不在记忆文件中重复本规范已列的红线或 Skill 中的详尽操作手册。

## 6. Skill 按需加载规范（`project-skills/` 与 HuxerUI 官方 Skill）

- **非必要不加载**：日常简单修改无需读取任何 `SKILL.md`；仅当任务明确命中下列触发条件时，才按需读取对应 Skill。
- **修改 UI 前必读**：改动界面组件、样式、主题、窗口或系统托盘前，必须先读取 HuxerUI 官方 UI 相关 Skill（`~/.mcpp/git/*/skills/huxerui-app-development/` 下 `SKILL.md` 与对应 `references/`）及 [huxerui-cpp-modules](project-skills/huxerui-cpp-modules/SKILL.md)。
- **修改任务前必读**：改动异步协程、并发刷新、后台任务队列或取消机制前，必须先读取 HuxerUI 官方任务相关 Skill（`~/.mcpp/git/*/skills/huxerui-app-development/references/fundamentals.md` 的 `Lifecycle and tasks` / `Callback integration`）及对应项目 Skill。

| Skill 名称 | 路径 | 何时加载 |
| --- | --- | --- |
| `huxerui-cpp-modules` | `project-skills/huxerui-cpp-modules/SKILL.md` | 新增/修改 `src/` 或 `src/ui/` 模块、处理 `hcg`/P1689 报错或编写 `tests/ui_layout_*_test.cpp` 用例时 |
| `xlings-cli-integration` | `project-skills/xlings-cli-integration/SKILL.md` | 改动 `xlings` CLI 命令/NDJSON 解析、`XlingsStore` 并发刷新与批量流或排查卡顿时 |
| `cross-platform-packaging-icons` | `project-skills/cross-platform-packaging-icons/SKILL.md` | 改动 `mcpp.toml`/`build.mcpp`、图标生成、托盘/三端窗口图标、安装包格式或 CI/Release 时 |

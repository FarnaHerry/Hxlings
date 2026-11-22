---
name: xlings-cli-integration
description: >-
  Use this skill when adding or modifying xlings CLI commands, NDJSON/JSON
  parsers in src/xlings_parser.cppm or src/xlings_client.cppm, process spawning
  and PATH sanitization in src/xlings_process.cppm, store concurrent/lazy
  refresh flows in src/store.cppm, or parser integration tests.
---

# xlings CLI Integration & Store Refresh Skill

本指南定义 Hxlings 如何高效、非阻塞地调用 `xlings` CLI 并维护领域状态，适用于扩展协议接口、重构刷新流或排查卡顿问题。

## 1. 子进程解析与环境隔离（`src/xlings_process.cppm`）

1. **二进制发现顺序**：`HXLINGS_BIN` 环境变量 -> `PATH`（跳过含 `/.mcpp/registry` 或 `\.mcpp\registry` 的条目）-> `~/.xlings/subos/current/bin/xlings` -> `~/.xlings/bin/xlings` ->（macOS）Homebrew 标准路径。
2. **环境清理**：启动子进程前剔除指向 `.mcpp/registry` 的 `XLINGS_HOME`、`XLINGS_DATA`、`XLINGS_SUBOS`，防止 `mcpp run` 环境下管理错目录；同时剔除自包含包封装器注入的 `MCPP_BUNDLE_DIR`，并在 POSIX 下清理指向包前缀或 `.mcpp/registry` 的 `GCONV_PATH`、`LOCPATH` 与 `LD_LIBRARY_PATH` 条目（避免宿主工具加载包内 glibc 2.44 模块失败）。
3. **可取消流式执行**：长耗时写操作统一通过 `RunProcessStreaming` + `ActiveProcessControl` 传入 `XlingsStore::ExecuteOperation`，在任务控制台支持取消与实时进度反馈。

## 2. 命令耗时特征与分层加载原则

实测单次 CLI 命令耗时特征决定了必须拆分为“并发核心 + 懒加载慢域”：
- **核心快命令（适合启动 `RefreshCore` 5 路并发）**：
  - `interface --version` / `interface env`（~5 ms）：第 1 组并发任务优先返回并置 `initial_loaded = true`。
  - `interface system_status`（~163 ms）：第 2 组。
  - `interface list_packages`（~124 ms）：第 3 组。
  - `interface list_subos` + `interface list_subos_shims`（~84 ms）：第 4 组。
  - `interface list_repos` + `self log --json`（~42 ms）：第 5 组。
- **高耗时或次级按需域（严禁放入启动必等路径）**：
  - `interface list_index_versions`：远程索引请求约 **2.36 s**，独立由 `RefreshIndexes` 后台异步查询，不阻塞同页的仓库列表与本地 xpkg 显示。
  - 包详情（`info_panel` + `list_installed_versions`）、SubOS 详情（`env` + `events`）、`xlings config`、`profile list` 分别只在进入对应页面/标签页时触发按需加载（或仅当该数据已加载过时才在定向写操作后刷新）。

## 3. 写操作后的定向刷新映射

在 `XlingsStore` 新增写操作后，禁止无脑调用全量 `RefreshAll(t)`，按影响域选择最小刷新集：
- 安装 / 卸载 / 批量清理 / 批量更新 / 切换版本 -> `s->RefreshCore(t, true);`
- 索引同步+更新（`UpdatePackagesAsync` / `SyncIndexAndScanUpdatesAsync`）-> `s->RefreshCore(t, true); if (s->indexes_loaded.Get()) s->RefreshIndexes(t, true);`
- SubOS 创建 / 切换 / 删除 -> `s->loaded_subos_detail_name_.clear(); s->RefreshCore(t, true);`
- 仓库 / 索引快照 / 本地 xpkg 变更 -> `s->RefreshIndexes(t, true);`
- `xlings config`（mirror / lang / ui-mode / interactive）-> `s->RefreshXlingsConfig(t, true);`
- `profile commit` -> `s->RefreshProfiles(t, true);`
- `profile rollback` -> `s->RefreshProfiles(t, true); s->RefreshCore(t, true);`
- `doctor --fix` / `self clean`（非 dry-run）/ `self update` -> `s->RefreshCore(t, true); if (s->xlings_config_loaded.Get()) s->RefreshXlingsConfig(t, true);`

## 4. 解析与单测验证清单

1. 新增 JSON/NDJSON 解析或计划生成函数时，必须在 `tests/xlings_parser_test.cpp` 中使用代表性真实 CLI 样本覆盖边界用例（多活跃版本、降级包、未索引报错、跨平台/多行空白规范化）。
2. 若涉及客户端设置序列化（`src/app_settings.cppm`），同步在 `TestAppSettingsSerializationAndPersistence` 覆盖合法/非法/边界与临时目录读写。
3. 验证命令：
   `mcpp test`（确认 `xlings_parser_test ... ok`）。

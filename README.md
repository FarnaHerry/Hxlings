# Hxlings

[![CI](https://github.com/FarnaHerry/Hxlings/actions/workflows/ci.yml/badge.svg)](https://github.com/FarnaHerry/Hxlings/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

基于 `mcpp`（C++23 Modules）与 `HuxerUI`（`huxerui.huxerui`）构建的跨平台（Linux / macOS / Windows）`xlings` 桌面图形管理器。

## 功能概览

- **概览总览**：展示 `xlings` 版本、当前 SubOS、镜像源、关键路径统计、绑定健康告警（一键 `doctor --fix` 修复）、一键进入批量过期版本清理，以及最近操作记录。
- **已安装管理**：按软件聚合多版本列表，支持按名称/多版本/异常过滤、一键切换活跃版本（`use`）、单包/跨包批量清理过期版本（含 `--dry-run` 预演、可释放空间与依赖保护展示）、勾选多版本批量卸载、安装指定可用版本以及依赖计划预览。
- **探索与安装**：基于 `xlings interface search_packages` 检索软件包与分类，支持选择版本与目标 SubOS、预览依赖安装计划（`plan_install`）、实时流式安装。
- **SubOS 环境**：新建/切换/删除 SubOS 子系统，查看活跃 Shims 命令（点击复制）、SubOS 环境变量、在指定 SubOS 内执行命令（支持沙箱模式）以及审计事件。
- **索引与配方**：管理全局索引仓库（添加/更新/移除/同步）、切换或固定历史索引快照版本、注册与移除本地 `.lua` xpkg 配方。
- **设置与诊断**：切换 `mirror` / `lang` / `ui-mode` / `interactive`，运行 `self doctor` 各模式诊断与修复、缓存清理预演与执行、`self update`、`profile` 代际快照提交与回滚，以及浅色/深色主题与强调色切换。
- **底部任务控制台**：所有长耗时操作均通过后台流式执行，支持实时进度条、中断取消、日志展开与一键复制。

## 构建、测试与运行

```bash
# 构建
mcpp build

# 运行单元测试与只读集成测试
mcpp test

# 启动桌面 GUI
mcpp run

# 打包可分发压缩包
mcpp pack hxlings --release
```

## 说明

- 当前基于 `mcpp 2026.11.21.1` 构建，并通过 Git 依赖接入官方 `HuxerUI/HuxerUI`（`main` 分支，0.5.0，已合入完整的新版 `mcpp` 构建契约与内建资源/插件打包支持）。
- 当通过 `mcpp run` 启动时，进程环境中的 `XLINGS_HOME` 会指向 `mcpp` 自身的隔离目录（`~/.mcpp/registry`）。Hxlings 在解析并派生 `xlings` 子进程时会自动跳过 `~/.mcpp/registry`，确保始终管理用户的真实 `xlings` 环境（可通过 `HXLINGS_BIN` 环境变量显式覆盖 `xlings` 路径）。
- 在 Linux Wayland + XWayland 会话中，为避免 `xim:gtk4@4.16.13` 在部分 Wayland 合成器下触发 `zwp_linux_dmabuf_feedback_v1` 初始化崩溃，当 `GDK_BACKEND` 未设置且同时存在 `WAYLAND_DISPLAY` 与 `DISPLAY` 时会默认回退到 `GDK_BACKEND=x11`；如需强制使用 Wayland，可显式运行 `GDK_BACKEND=wayland mcpp run`。

## 开源协议

本项目基于 [MIT License](LICENSE) 开源。

# Hxlings

[![CI](https://github.com/FarnaHerry/Hxlings/actions/workflows/ci.yml/badge.svg)](https://github.com/FarnaHerry/Hxlings/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

基于 `mcpp`（C++23 Modules）与 `HuxerUI`（`huxerui.huxerui`）构建的跨平台（Linux / macOS / Windows）`xlings` 桌面图形管理器。

## 功能概览

- **概览总览**：展示 `xlings` 版本、当前 SubOS、镜像源、关键路径统计、绑定健康告警（一键 `doctor --fix` 修复）、一键进入批量更新软件包与批量过期版本清理，以及最近操作记录。
- **已安装管理**：按软件聚合多版本列表，支持按名称/多版本/异常过滤、一键切换活跃版本（`use`）、全量扫描与批量更新所有软件包（支持新版升级、配方修订重装与跳过未索引包）、单包/跨包批量清理过期版本（含 `--dry-run` 预演、可释放空间与依赖保护展示）、勾选多版本批量卸载、安装指定可用版本以及依赖计划预览。
- **探索与安装**：基于 `xlings interface search_packages` 检索软件包与分类，支持选择版本与目标 SubOS、预览依赖安装计划（`plan_install`）、实时流式安装。
- **SubOS 环境**：新建/切换/删除 SubOS 子系统，查看活跃 Shims 命令（点击复制）、SubOS 环境变量、在指定 SubOS 内执行命令（支持沙箱模式）以及审计事件。
- **索引与配方**：管理全局索引仓库（添加/更新/移除/同步）、切换或固定历史索引快照版本、注册与移除本地 `.lua` xpkg 配方。
- **设置与诊断**：切换 `mirror` / `lang` / `ui-mode` / `interactive`，运行 `self doctor` 各模式诊断与修复、缓存清理预演与执行、`self update`、`profile` 代际快照提交与回滚，以及浅色/深色主题与强调色切换。
- **底部任务控制台**：所有长耗时操作均通过后台流式执行，支持实时进度条、中断取消、日志展开与一键复制。
- **纯 C++23 Modules 与跨平台快捷键**：`src/` 全量采用纯 C++23 Modules（零 `#include` 与预处理宏，Windows 子系统/入口点由 `mcpp.toml` 声明，跨平台进程与环境管理基于 `mcpplibs.os`），支持 macOS（`⌘`）与 Linux/Windows（`Ctrl`）原生快捷键体系。

## 快捷键（macOS / Linux & Windows）

| 功能 | macOS | Linux / Windows |
| --- | --- | --- |
| 切换 6 个主工作区视图 | `⌘1` ~ `⌘6` | `Ctrl+1` ~ `Ctrl+6` |
| 打开设置与诊断 | `⌘,` | `Ctrl+,` |
| 跳转软件商店并聚焦搜索 | `⌘K` | `Ctrl+K` |
| 聚焦当前页搜索 / 筛选框 | `⌘F` | `Ctrl+F` |
| 刷新全部 `xlings` 状态 | `⌘R` / `F5` | `Ctrl+R` / `F5` |
| 打开批量更新 / 批量清理 | `⇧⌘U` / `⇧⌘P` | `Ctrl+Shift+U` / `Ctrl+Shift+P` |
| 展开 / 收起任务日志面板 | `⌘J` | `Ctrl+J` |
| 切换浅色 / 深色主题 | `⇧⌘D` | `Ctrl+Shift+D` |
| 查看快捷键一览 | `⌘/` / `F1` | `Ctrl+/` / `F1` |
| 窗口全屏 / 最小化 / 关闭 | `⌃⌘F` / `⌘M` / `⌘W` | `F11` |

## 构建、测试与运行

```bash
# 构建
mcpp build

# 运行单元测试与只读集成测试
mcpp test

# 启动桌面 GUI
mcpp run

# 打包便携归档或各平台原生安装包
mcpp pack hxlings --release --pkg-format tar        # Linux/macOS 便携归档（Windows 使用 zip）
mcpp pack hxlings --release --pkg-format deb        # Linux DEB 安装包
mcpp pack hxlings --release --pkg-format rpm        # Linux RPM 安装包
mcpp pack hxlings --release --pkg-format appimage   # Linux AppImage
mcpp pack hxlings --release --pkg-format msi        # Windows MSI 安装包
mcpp pack hxlings --release --pkg-format setup      # Windows Burn 安装向导（内含 MSI）
mcpp pack hxlings --release --pkg-format dmg        # macOS DMG 磁盘镜像
```

## 发布产物矩阵（Release）

- **Linux (`x86_64`)**：`.tar.gz` 便携归档、`.deb`、`.rpm`、`.AppImage`（附带桌面图标 [hxlings.svg](platform/linux/package/hxlings.svg) 与 `.desktop` 入口）
- **Windows (`x86_64`)**：`.zip` 便携归档、`.msi`、`-setup.exe`（WiX Burn 安装器）
- **macOS (`arm64`)**：`.tar.gz` 便携归档、`.dmg`（含 `Hxlings.app` 与 `/Applications` 快捷方式）
- **校验文件**：随 Release 一并发布 `SHA256SUMS.txt`

## 说明

- 当前基于 `mcpp 2026.11.21.1` 构建，并通过 Git 依赖接入官方 `HuxerUI/HuxerUI`（`main` 分支，0.5.0，已合入完整的新版 `mcpp` 构建契约与内建资源/插件打包支持）。
- 当通过 `mcpp run` 启动时，进程环境中的 `XLINGS_HOME` 会指向 `mcpp` 自身的隔离目录（`~/.mcpp/registry`）。Hxlings 在解析并派生 `xlings` 子进程时会自动跳过 `~/.mcpp/registry`，确保始终管理用户的真实 `xlings` 环境（可通过 `HXLINGS_BIN` 环境变量显式覆盖 `xlings` 路径）。
- 在 Linux Wayland + XWayland 会话中，为避免 `xim:gtk4@4.16.13` 在部分 Wayland 合成器下触发 `zwp_linux_dmabuf_feedback_v1` 初始化崩溃，当 `GDK_BACKEND` 未设置且同时存在 `WAYLAND_DISPLAY` 与 `DISPLAY` 时会默认回退到 `GDK_BACKEND=x11`；如需强制使用 Wayland，可显式运行 `GDK_BACKEND=wayland mcpp run`。

## 开源协议

本项目基于 [MIT License](LICENSE) 开源。

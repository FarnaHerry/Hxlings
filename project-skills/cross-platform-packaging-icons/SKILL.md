---
name: cross-platform-packaging-icons
description: >-
  Use this skill when regenerating or changing application/tray icons, modifying
  mcpp.toml packaging or resource metadata, updating build.mcpp packaging logic,
  adjusting Linux/macOS/Windows native window icon behavior in
  src/platform_icon.cppm, or editing .github/workflows/ci.yml or release.yml.
---

# Cross-Platform Packaging, Logo Assets & Release Skill

本指南说明 Hxlings 品牌 Logo 资产管线、`mcpp.toml` 声明、三端原生任务栏/Dock 图标适配以及打包与 CI/Release 验证步骤。

## 1. Logo 资产源文件与派生生成

唯一设计源为矢量文件 [hxlings.svg](../../platform/linux/package/hxlings.svg)，修改 Logo 后需同步重新生成 4 个派生文件（使用本机 `python3` + `cairosvg` + `Pillow`）：
- `resources/images/hxlings.png`：256×256 RGBA PNG（供窗口标题栏 `app::images::hxlings`、macOS `NSImage` 及 Linux GTK 主题检索）
- `resources/images/tray_icon.png`：64×64 RGBA PNG（供 `SystemTrayHandle::Show(app::images::tray_icon, ...)`）
- `platform/windows/app.ico`：包含 16、24、32、48、64、128、256 多分辨率 PNG-in-ICO（嵌入 PE 序号 1 资源，并供 MSI / Burn Setup 安装器使用）
- `platform/macos/AppIcon.icns`：`icns` 容器写入 `icp4`(16)、`icp5`(32)、`icp6`(64)、`ic07`(128)、`ic08`(256)、`ic09`(512)、`ic10`(1024) PNG 块（`huxerui.rules` 自动打包进 `Hxlings.app/Contents/Resources/AppIcon.icns`）

校验生成的资产：
```bash
file resources/images/hxlings.png resources/images/tray_icon.png platform/windows/app.ico platform/macos/AppIcon.icns
```

## 2. `mcpp.toml` 与三端运行时图标契约

1. **`mcpp.toml` 关键字段**：
   - `[package.metadata.huxerui]`：`name = "Hxlings"`、`id = "io.github.farnaherry.hxlings"`、`resources = "resources"`（触发 `hrc` 编译与 `app.resources` 模块导出）。
   - `[resources]`：`icon = "platform/windows/app.ico"`（嵌入 Windows PE 序号 1 图标）。
   - `[targets.hxlings]`：`kind = "app"`、`main = "src/main.cpp"`、`windows_subsystem = "windows"`、`windows_entry = "main"`。
2. **`src/platform_icon.cppm` 运行时行为**：
   - **Linux**：`PreparePlatformStartup()` 调用 `g_set_prgname("hxlings")` 并处理混合 Wayland 会话回退；`ApplyPlatformApplicationIcon()` 在 `InstallApplication` 阶段调用 `gtk_icon_theme_add_search_path` 添加 `<resources>/huxerui/app/images` 并调用 `gtk_window_set_default_icon_name("hxlings")`。
   - **macOS**：`ApplyPlatformApplicationIcon()` 通过 `mcpplibs::os::macos::objc` 将 `hxlings.png` 设为 `[NSApplication sharedApplication].applicationIconImage`（弥补无 bundle 直接 `mcpp run` 时的 Dock 图标）。
   - **Windows**：`ApplyPlatformWindowIcon()` 在窗口创建后动态查找当前线程顶层窗口，从 PE 序号 1 通过 `LoadImageW` 读取大小图标并发送 `WM_SETICON`（解决 HuxerUI 默认 `WNDCLASSEXW` 未设置 `hIcon`/`hIconSm` 的问题）。

## 3. 多平台打包、CI 检查与安全释放原则

1. **打包产物矩阵**：
   - Linux（`.deb` / `.rpm` 使用 `mcpp pack hxlings --release --mode system --pkg-format <fmt>`）：直接依赖发行版原生库（`deb` 声明 `libgtk-4-1 (>= 4.14)` / `libepoxy0 (>= 1.5)` / `libsoup-3.0-0 (>= 3.0)` 等，`rpm` 声明 `lib*.so.*()(64bit)`）。[build.mcpp](../../build.mcpp) 通过生成 `glibc_compat_x86_64.c`（`--wrap=sqrtf/atan2f/remainder` 绑定 `GLIBC_2.2.5`）将可执行文件最高符号版本压到 `GLIBC_2.38`。
   - Linux（`.tar.gz` / `.AppImage` 使用 `mcpp pack hxlings --release --mode self-contained --pkg-format <fmt>`）：随附 `lib/ld-linux-x86-64.so.2`、`lib/libc.so.6`、`lib/libm.so.6` 与 `lib/gconv/`；必须在随附加载器 `--library-path` 中将宿主 GPU 搜索路径追加在 `$here/lib` 之后（`AppImage` 由 `build.mcpp` 自动修补 `stage/hxlings` 与 `stage/run.sh`，`tar` 由 CI 重封装修补），否则 `libepoxy` 无法 `dlopen` 宿主 OpenGL 驱动导致 GTK4 退化为 `GskCairoRenderer` 粉色块；同时 `src/platform_icon.cppm` 通过 `MCPP_BUNDLE_DIR` 自动设置 `HUXERUI_RESOURCES_DIR`。
   - Windows：`zip`、`msi`、`setup`（WiX v5 Burn，无需 `--mode`）：
     - `build.mcpp` 在 Windows 下调用 `mcpp::dist::wix::generate` 并传入自定义 `Hxlings-package.wxs`（含 `<MediaTemplate EmbedCab="yes" />`、`app.ico` / `ARPPRODUCTICON`、开始菜单与条件桌面快捷方式 `CREATE_DESKTOP_SHORTCUT = 1`）。
     - `platform/windows/package/` 为独立 HuxerUI 安装器 UI 子包（`hxlings-installer.exe`，使用 `xim:wix@5.0.2-1` 的 `balutil.lib` / `dutil.lib` / `mbanative.dll` 与 10 种语言资源）；`HXLINGS_WINDOWS_INSTALLER_DIR` 将其产物注入 `Hxlings-bundle.wxs` 的 `<BootstrapperApplication>` + `HxlingsInstallerPayloads`，替代 WiX 默认界面。
   - macOS：`tar`、`dmg`（无需 `--mode`）
2. **CI 归档检查要点（`.github/workflows/ci.yml`）**：
   - `set -o pipefail` 下禁止 `tar -tzf ... | grep -q`（会触发 SIGPIPE 141），应先存入变量再过滤检查。
   - Linux `tar`/`deb`、macOS `tar`/`dmg`、Windows `msi` 文件表必须同时包含 `huxerui/resources.bin`、`app/images/hxlings.png`、`app/images/tray_icon.png`。
   - macOS CI 门禁：使用 `ruby -c Casks/hxlings.rb` 校验 Cask 语法；对 `.tar.gz` 内 `bin/hxlings` 执行 `codesign --verify --strict`；只读挂载 `.dmg` 验证 `Hxlings.app`、`Applications` 软链、`Info.plist`、`AppIcon.icns`、`resources.bin` 与 `codesign --verify --deep --strict`。
   - Windows `.msi` 与 `-setup.exe` 门禁：校验 `.msi >= 2MB`、`Media.Cabinet` 以 `#` 开头、快捷方式与 `ARPPRODUCTICON` 齐全，并将孤立 `.msi` 拷入空目录执行 `msiexec /a` 验证可解出 `hxlings.exe` + 运行时 DLL + `resources.bin`；对 `-setup.exe` 执行 `wix burn extract <bundle> -out ... -outba ...` 验证内置 `hxlings-installer.exe`、`mbanative.dll`、CRT DLL 与安装器资源且不含 `WixStandardBootstrapperApplication`。
   - Linux `.deb`/`.rpm` 原生依赖门禁：校验不打包 `libc.so.6`/`libgtk-4.so.1`、声明了发行版依赖，且在 `ubuntu-24.04` 实装 `.deb` 并确认 `readelf -V` 无 `GLIBC_2.39+`、`/usr/bin/ldd` 全部正常解析。
   - Linux 自包含 `.tar.gz`/`.AppImage` 门禁：校验 `lib/ld-linux-x86-64.so.2`、`lib/libc.so.6`、`lib/libm.so.6`、`lib/gconv/`、`MCPP_BUNDLE_DIR` 与 `/usr/lib/x86_64-linux-gnu:/usr/lib64` 均存在，且随附链接器 `--list` 全量解析无 `not found`。
3. **本地打包烟测与发布**：
   ```bash
   mcpp pack hxlings --release --mode self-contained --pkg-format tar --output /tmp/hxlings-check.tar.gz
   tar -tzf /tmp/hxlings-check.tar.gz | grep -E 'resources\.bin|app/images/(hxlings|tray_icon)\.png|ld-linux-x86-64\.so\.2'
   rm -f /tmp/hxlings-check.tar.gz
   ```
   - `.github/workflows/release.yml` 由 `v*` Tag 触发并校验全部 9 个平台产物 + 生成 `SHA256SUMS.txt`，并自动附加 macOS Ad-hoc Gatekeeper 放行说明（发版升级版本号时同步更新 `Casks/hxlings.rb` 的 `version` 与 `sha256`）；切勿强制改写既有已发布 Tag。

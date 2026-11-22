export module hxlings.shortcuts;

import std;
import mcpplibs.os;
import huxerui;

export namespace hxlings {

inline constexpr bool kIsMacOs = mcpplibs::os::is_macos;

constexpr huxerui::KeyModifiers PrimaryModifier() noexcept {
    if constexpr (kIsMacOs) {
        return {.meta = true};
    } else {
        return {.control = true};
    }
}

constexpr huxerui::KeyModifiers PrimaryShiftModifier() noexcept {
    if constexpr (kIsMacOs) {
        return {.shift = true, .meta = true};
    } else {
        return {.shift = true, .control = true};
    }
}

inline std::string FormatShortcut(std::string_view key, bool shift = false) {
    if constexpr (kIsMacOs) {
        return std::string(shift ? "⇧⌘" : "⌘") + std::string(key);
    } else {
        return std::string(shift ? "Ctrl+Shift+" : "Ctrl+") + std::string(key);
    }
}

struct ShortcutDescriptor {
    std::string keys;
    std::string group;
    std::string description;
};

inline std::vector<ShortcutDescriptor> AppShortcutCatalog() {
    std::vector<ShortcutDescriptor> list{
        {FormatShortcut("1") + " ~ " + FormatShortcut("6"), "页面导航", "切换 6 个主工作区视图"},
        {FormatShortcut(","), "页面导航", "打开「设置与诊断」"},
        {FormatShortcut("K"), "搜索与操作", "跳转到「探索与安装」并聚焦搜索框"},
        {FormatShortcut("F"), "搜索与操作", "聚焦当前页面筛选框 / 打开软件搜索"},
        {FormatShortcut("R") + " / F5", "搜索与操作", "刷新全部 xlings 状态"},
        {FormatShortcut("U", true), "搜索与操作", "跳转到已安装页并打开批量更新"},
        {FormatShortcut("P", true), "搜索与操作", "跳转到已安装页并打开批量清理"},
        {FormatShortcut("J"), "视图与控制台", "展开或收起底部任务日志面板"},
        {FormatShortcut("D", true), "视图与控制台", "切换浅色 / 深色主题"},
        {FormatShortcut("/") + " / F1", "视图与控制台", "打开快捷键一览"},
        {"Esc", "视图与控制台", "关闭当前弹窗或返回"},
    };
    if constexpr (kIsMacOs) {
        list.push_back({"⌃⌘F", "窗口管理 (macOS)", "进入 / 退出原生全屏"});
        list.push_back({"⌘M", "窗口管理 (macOS)", "最小化窗口"});
        list.push_back({"⌘W / ⌘Q", "窗口管理 (macOS)", "关闭窗口"});
    } else {
        list.push_back({"F11", "窗口管理", "进入 / 退出全屏"});
    }
    return list;
}

} // namespace hxlings

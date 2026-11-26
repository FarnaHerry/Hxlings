export module hxlings.page.luban;

import std;
import huxerui;
import hxlings.theme;
import hxlings.icons;
import hxlings.store;
import hxlings.components;
import hxlings.luban.types;
import hxlings.luban.stack;

import hxlings.page.luban.dialogs;
import hxlings.page.luban.env_tab;
import hxlings.page.luban.studio;

using namespace huxerui;


export namespace hxlings {

[[huxerui::composable]]
View LubanPage() {
    const auto store = UseService<XlingsStore>();
    const TaskScope tasks = UseApplicationTaskScope();
    const TaskScope ui_tasks = UseTaskScope();
    const ToastHandle toast = UseToast();
    const DialogHandle dialog = UseDialog();
    const ThemeSpec& theme = UseTheme();

    const ViewHandle filter_handle = UseViewHandle();

    const std::size_t tab_idx = store->luban_tab.Get();
    const bool luban_avail = store->luban_available.Get();
    const bool luban_os_avail = store->luban_os_available.Get();
    const bool loading_luban = store->loading_luban.Get();

    Lifecycle([store, tasks] {
        if (!store->skip_initial_refresh && store->active_nav.Get() == 4) {
            store->RefreshLuban(tasks, false);
        }
    }, store->active_nav.Get());

    Lifecycle([store, ui_tasks, filter_handle, tab_idx] {
        if (store->active_nav.Get() == 4 && tab_idx == 0 && store->ConsumeFocusSearchNonce()) {
            ui_tasks.Post([filter_handle] {
                (void)filter_handle.RequestFocus(true);
            });
        }
    }, store->focus_search_nonce.Get(), tab_idx);

    View header = Row {
        Column {
            Text("LubanOS 隔离系统", TextRole::Title)
                .Style(TextStyle{
                    .font = Font::System(18.0F).WithWeight(FontWeight::Bold),
                    .foreground = theme.colors.on_surface,
                }),
            Text("多环境沙箱、发行版与桌面目录、声明式蓝图编排", TextRole::Label)
                .Style(TextStyle{.foreground = theme.colors.on_surface_variant}),
        }.With(Spacing(2.0F), Grow(1.0F)),
        Flow {
            luban_os_avail
                ? ActionButton(
                      "打开 LubanOS",
                      ButtonTone::Primary,
                      [store, toast] { store->LaunchLubanOsApp(toast); },
                      true,
                      icons::ExternalLinkIcon(),
                      true
                  )
                : ActionButton(
                      "安装 LubanOS",
                      ButtonTone::Secondary,
                      [store, tasks, toast] { store->InstallLubanComponentAsync(tasks, toast, "xim:luban-os"); },
                      true,
                      icons::DownloadIcon(),
                      true
                  ),
            RefreshButton(
                "刷新 Luban",
                [store, tasks] { store->RefreshLuban(tasks, true); },
                loading_luban
            ).Key("luban-refresh-btn"),
        }.With(Spacing(8.0F)),
    }.With(
        Padding(EdgeInsets::Symmetric(20.0F, 10.0F)),
        Spacing(12.0F),
        Background(theme.colors.surface),
        CrossAlign(CrossAxisAlignment::Center)
    );

    View tab_bar = Row {
        Tabs({"环境与机器", "发行版与桌面目录", "蓝图编排"}, tab_idx)
            .OnChanged([store](std::size_t idx) { store->luban_tab = idx; }),
        Spacer{}.With(Grow(1.0F)),
    }.With(
        Padding(EdgeInsets::Symmetric(20.0F, 6.0F)),
        Background(theme.colors.surface),
        CrossAlign(CrossAxisAlignment::Center)
    );

    View content = View{};
    if (!luban_avail && !loading_luban && tab_idx < 2) {
        content = LubanNotInstalledView(store, tasks, toast);
    } else if (tab_idx == 0) {
        content = LubanEnvironmentsTabView(store, tasks, toast, dialog, filter_handle);
    } else if (tab_idx == 1) {
        content = LubanCatalogTabView(store, tasks, toast, dialog);
    } else {
        content = LubanStacksStudioTabView(store, tasks, toast, dialog);
    }

    return Column {
        std::move(header),
        Divider(),
        std::move(tab_bar),
        Divider(),
        std::move(content),
    }.With(Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch));
}

} // namespace hxlings
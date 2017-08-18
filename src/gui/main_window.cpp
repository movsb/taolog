#include "stdafx.h"

#include "misc/config.h"
#include "misc/utils.h"
#include "misc/event_system.hpp"
#include "misc/basic_async.h"

#include "../res/resource.h"

#include "main_window.h"

namespace taolog {

static HWND g_logger_hwnd;
static UINT g_logger_message;

void DoEtwLog(void* log)
{
    ::SendMessage(g_logger_hwnd, g_logger_message, LoggerMessage::LogEtwMsg, LPARAM(log));
}

void MainWindow::get_metas(WindowMeta * metas)
{
    __super::get_metas(metas);
    metas->style |= WS_CLIPCHILDREN;
}

LPCTSTR MainWindow::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<Window title="ETW Log Viewer" size="850,600">
    <Resource>
        <Font name="default" face="微软雅黑" size="12"/>
        <Font name="12" face="微软雅黑" size="12"/>
    </Resource>
    <Root>
        <Vertical name="wrapper" padding="5,5,5,5">
            <Horizontal name="toolbar" height="30" padding="0,1,0,4">
                <Button name="start-logging" text="开始记录" width="60" style="tabstop" padding="0,0,5,0" />
                <Button name="clear-logging" text="清空记录" width="60" style="tabstop" padding="0,0,5,0"/>
                <Button name="module-manager" text="模块管理" width="60" style="tabstop" padding="0,0,5,0"/>
                <Button name="filter-result" text="结果过滤" width="60" style="tabstop" />
                <Control name="span" minwidth="30"/>
                <Label name="select-project-label" text="模块：" width="38" style="centerimage"/>
                <ComboBox name="select-project" style="tabstop" height="200" width="100" padding="0,0,4,0"/>
                <Label text="过滤：" width="38" style="centerimage"/>
                <ComboBox name="select-filter" style="tabstop" height="200" width="100" padding="0,0,4,0"/>
                <Label text="查找：" width="38" style="centerimage"/>
                <ComboBox name="s-filter" style="tabstop" height="200" width="64" padding="0,0,4,0"/>
                <TextBox name="s" width="80" style="tabstop" exstyle="clientedge"/>
                <Control width="10" />
                <Button name="tools" text="菜单" width="60" style="tabstop"/>
            </Horizontal>
            <ListView name="lv" style="showselalways,ownerdata,tabstop" exstyle="clientedge,doublebuffer,headerdragdrop"/>
        </Vertical>
    </Root>
</Window>
)tw";
    return json;
}

LRESULT MainWindow::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
    case WM_CREATE:         return _on_create();
    case WM_CLOSE:          return _on_close();
    case kDoLog:            return _on_log(LoggerMessage::Value(wparam), lparam);
    case kLogDbgViewRaw:    return _log_raw_dbg(int(wparam), (std::string*)lparam);
    case WM_INITMENUPOPUP:  return _on_init_popupmenu(HMENU(wparam));
    case WM_NCACTIVATE:
    {
        if(wparam == FALSE && _tipwnd->showing()) {
            return TRUE;
        }
        break;
    }
    case WM_SIZE :
    {
        int width = GET_X_LPARAM(lparam);
        bool show = width > 600;
        _btn_start->set_visible(show);
        _btn_clear->set_visible(show);
        _btn_modules->set_visible(show);
        _btn_filter->set_visible(show);
        _root->find<taowin::Control>(L"span")->set_visible(show);
        break;
    }
    }
    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT MainWindow::control_message(taowin::SystemControl* ctl, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    // TODO static!!
    static bool mi = false;

    if(umsg == WM_MOUSEMOVE) {
        if(!mi) {
            taowin::set_track_mouse(ctl);
            mi = true;
        }
    }
    else if(umsg == WM_MOUSELEAVE) {
        mi = false;
    }
    else if(umsg == WM_MOUSEHOVER) {
        mi = false;

        POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};

        if(ctl == _listview) {
            const wchar_t* s;
            bool ctrl_pressed = !!(::GetAsyncKeyState(VK_CONTROL) & 0x8000);

            // Preview all log items When Ctrl-Key is pressed
            if(!ctrl_pressed) {
                if(_listview->showtip_needed(pt, &s)) {
                    _tipwnd->popup(s);
                }
            }
            else {
                LVHITTESTINFO hti;
                hti.pt = pt;
                if(_listview->subitem_hittest(&hti) != -1) {
                    const auto& log = *(*_current_filter)[hti.iItem];
                    // TODO this lambda is duplicated
                    auto str = log.to_tip([&](int i) {
                        return i >= 0 && i <= (int)_columns.size()
                            ? _columns[i].name.c_str()
                            : L"";
                    });
                    _tipwnd->format(str);
                }
            }

            // 如果交给默认处理，会出现自动选中鼠标所在行的奇怪现象
            return 0;
        }
        else if(ctl == _edt_search) {
            auto tips = 
L"请输入待搜索的文本。\n"
"\n"
"搜索上下文为：当前过滤器、当前搜索列。\n"
"\n"
"   1、搜索文本若以 ~ 开头，执行正则表达式搜索；\n"
"   2、搜索文本若以 ` 开头，执行 LUA 脚本搜索；\n"
"   3、其它情况执行不区分大小写的普通文本搜索。\n"
"\n"
"\bn"
"快捷键：\bn"
"    Ctrl + F             \bw130-   聚焦搜索框\bn"
"    Enter                \bw130-   执行搜索\bn"
"    F3                   \bw130-   搜索下一个\bn"
"    Shift + F3           \bw130-   搜索上一个\bn"
"    Ctrl + Enter         \bw130-   添加过滤器（临时）\bn"
"    Ctrl + Shift + Enter \bw130-   添加过滤器（固定）\bn"
;
            _tipwnd->format(tips);
            return 0;
        }
    }

    return __super::control_message(ctl, umsg, wparam, lparam);
}

LRESULT MainWindow::on_menu(const taowin::MenuIDs& m)
{
    if(m[0] == L"lv") {
        if(m[1] == L"top")              { _listview->go_top(); }
        else if(m[1] == L"bot")         { _listview->go_bottom(); }
        else if(m[1] == L"clear")       { g_evtsys.trigger(L"log:clear"); }
        else if(m[1] == L"full")        { g_evtsys.trigger(L"log:fullscreen"); }
        else if(m[1] == L"copy")        { g_evtsys.trigger(L"log:copy"); }
        else if(m[1] == L"filters")     { g_evtsys.trigger(L"filter:set", (void*)std::stoi(m[2])); }
        else if(m[1] == L"projects")    { g_evtsys.trigger(L"project:set", (void*)std::stoi(m[2]), true); }
        else if(m[1] == L"column")      {
            auto visible = _listview->is_header_visible();
            if(!visible) _listview->show_header(1);
            _on_select_column();
            if(!visible) _listview->show_header(0);
        }
    }
    else if(m[0] == L"tools") {
        auto exec = [](const std::wstring& path, const std::wstring& args = std::wstring())
        {
            class ShellExecuteInBackgroud : public AsyncTask
            {
            public:
                ShellExecuteInBackgroud(const std::wstring& path, const std::wstring& args)
                    : _path(path)
                    , _args(args)
                { }

            protected:
                virtual int doit() override
                {
                    return (int)::ShellExecute(nullptr, L"open", _path.c_str(), _args.c_str(), nullptr, SW_SHOWNORMAL);
                }

                virtual int done() override
                {
                    delete this;
                    return 0;
                }

            protected:
                std::wstring _path;
                std::wstring _args;
            };

            g_async.AddTask(new ShellExecuteInBackgroud(path, args));
        };

        if(m[1] == L"guid") {
            UUID uuid;
            // http://stackoverflow.com/a/1327160/3628322
            if(::UuidCreate(&uuid) == RPC_S_OK) {
                // #ifdef TAOLOG_ENABLED
                // // {630514B5-7B96-4B74-9DB6-66BD621F9386}
                // static const GUID providerGuid = 
                // { 0x630514b5, 0x7b96, 0x4b74, { 0x9d, 0xb6, 0x66, 0xbd, 0x62, 0x1f, 0x93, 0x86 } };

                // TaoLogger g_taoLogger(providerGuid);
                // #endif

                wchar_t buf[1024];
                _snwprintf(buf, std::size(buf),
                    L"#ifdef TAOLOG_ENABLED\n"
                    "// {%08X-%04X-%04X-%04X-%02X%02X%02X%02X%02X%02X}\n"
                    "static const GUID providerGuid = \n"
                    "{ 0x%08X, 0x%04X, 0x%04X, { 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X } };\n"
                    "TaoLogger g_taoLogger(providerGuid);\n"
                    "#endif\n",
                    uuid.Data1, uuid.Data2, uuid.Data3,
                    (uuid.Data4[0] << 8) + uuid.Data4[1], uuid.Data4[2], uuid.Data4[3], uuid.Data4[4], uuid.Data4[5], uuid.Data4[6], uuid.Data4[7],
                    uuid.Data1, uuid.Data2, uuid.Data3,
                    uuid.Data4[0], uuid.Data4[1], uuid.Data4[2], uuid.Data4[3], uuid.Data4[4], uuid.Data4[5], uuid.Data4[6], uuid.Data4[7]
                );

                utils::set_clipboard_text(buf);
                msgbox(buf, 0, L"已复制到剪贴板");
            }
            else {
                msgbox(L"创建失败。", MB_ICONERROR);
            }
        }
        else if(m[1] == L"export") {
            _export2file();
        }
        else if(m[1] == L"json_visual") {
            auto jv = new JsonVisual;
            jv->create(this);
            jv->show();
        }
        else if(m[1] == L"lua_console") {
            auto lc = new LuaConsoleWindow;
            lc->create();
            lc->show();
        }
        else if(m[1] == L"calc" || m[1] == L"notepad" || m[1] == L"regedit" || m[1] == L"Control" || m[1] == L"cmd"
            || m[1] == L"mstsc"
            )
        {
            exec(m[1]);
        }
        else if(m[1][0] == '_') {
            int i = std::stoi(m[1].c_str() + 1);
            const auto& tool = g_config->arr("tools")[i];
            auto path = g_config.ws(tool["path"].string_value());
            auto args = g_config.ws(tool["args"].string_value());
            exec(path, args);
        }
        else if(m[1] == L"topmost") {
            bool totop = !(::GetWindowLongPtr(_hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST);
            _set_top_most(totop);
            _tools_menu->find_sib(L"topmost")->check(totop);
            _config["topmost"] = totop;
        }
        else if(m[1] == L"colors") {
            auto set = [&](int i, bool fg) {
                auto& colors = _config.obj("listview").arr("colors").as_arr();
                for(auto& jc : colors) {
                    auto& c = JsonWrapper(jc).as_obj();
                    if(c["level"] == i) {
                        char buf[12];
                        unsigned char* p = fg ? (unsigned char*)&_colors[i].fg : (unsigned char*)&_colors[i].bg;
                        sprintf(&buf[0], "%d,%d,%d", p[0], p[1], p[2]);
                        if(fg) c["fgc"] = buf;
                        else c["bgc"] = buf;
                        _listview->redraw_items(0, _listview->get_item_count());
                        break;
                    }
                }
            };

            (new ListviewColor(&_colors, &_level_maps, set))->domodal(this);
        }
    }

    return 0;
}

LRESULT MainWindow::on_accel(int id)
{
    switch(id)
    {
    case IDR_ACCEL_MAINWINDOW_SEARCH:
        _edt_search->focus();
        _edt_search->set_sel(0, -1);
        break;
    }

    return 0;
}

LRESULT MainWindow::on_notify(HWND hwnd, taowin::Control * pc, int code, NMHDR * hdr)
{
    if (pc == _listview) {
        if (code == NM_CUSTOMDRAW) {
            return _on_custom_draw_listview(hdr);
        }
        else if (code == NM_DBLCLK) {
            auto nmlv = reinterpret_cast<NMITEMACTIVATE*>(hdr);
            if (nmlv->iItem != -1) {
                _view_detail(nmlv->iItem);
            }
            return 0;
        }
        else if(code == NM_RCLICK) {
            bool empty = !_listview->get_item_count();
            bool hassel = !empty && _listview->get_selected_count() > 0;
            _lvmenu->find_sib(L"clear")->enable(!empty);
            _lvmenu->find_sib(L"copy")->enable(hassel);
            _lvmenu->track();
            return 0;
        }
        else if (code == LVN_KEYDOWN) {
            auto nmlv = reinterpret_cast<NMLVKEYDOWN*>(hdr);
            if (nmlv->wVKey == VK_F11) {
                g_evtsys.trigger(L"log:fullscreen");
                return 0;
            }
            else if (nmlv->wVKey == VK_F3) {
                _do_search(false);
            }
            else if(nmlv->wVKey == 'C') {
                if(::GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                    _copy_selected_item();
                    return 0;
                }
            }
        }
        else if (code == LVN_ITEMCHANGED) {
            int i = _listview->get_next_item(-1, LVNI_SELECTED);
            if (i != -1) {
                _listview->redraw_items(_searcher.last(), _searcher.last());
                _searcher.last(i);
            }
        }
    }
    else if (pc == _cbo_search_filter) {
        if (code == CBN_SELCHANGE) {
            _edt_search->set_sel(0, -1);
            _edt_search->focus();
        }
    }
    else if(pc == _cbo_sel_flt) {
        if(code == CBN_SELCHANGE) {
            auto f = (EventContainer*)_cbo_sel_flt->get_cur_data();
            g_evtsys.trigger(L"filter:set", f, false);
            return 0;
        }
    }
    else if(pc == _cbo_prj) {
        if(code == CBN_SELCHANGE) {
            auto m = (ModuleEntry*)_cbo_prj->get_cur_data();
            g_evtsys.trigger(L"project:set", m, false);
            return 0;
        }
    }
    return 0;
}

bool MainWindow::filter_special_key(int vk)
{
    if (vk == VK_RETURN && ::GetFocus() == _edt_search->hwnd()) {
        _listview->redraw_items(_searcher.last(), _searcher.last());
        _searcher.last(-1);

        if(_do_search(true)) {
            // 如果有搜索结果，并且按住了CTRL键，则自动创建新的过滤器
            if(::GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                // 当前选择的列（绝对索引）
                int col = (int)_cbo_search_filter->get_cur_data();
                if(col == -1) {
                    msgbox(L"新建过滤器不能指定为 <全部> 列。", MB_ICONINFORMATION);
                }
                else {
                    auto& c         = _columns.showing(col);
                    auto& name      = _searcher.s();
                    auto& colname   = c.name;
                    auto& value     = _searcher.s();
                    auto p = new EventContainer(_searcher.s(), c.index, colname, -1, L"", value, true);
                    // 按下了 Shift 键？按下则是固定添加，否则是临时添加
                    p->is_tmp = !(::GetAsyncKeyState(VK_SHIFT) & 0x8000);
                    g_evtsys.trigger(L"filter:new", p);
                    g_evtsys.trigger(L"filter:set", p);
                }
            }
        }
        else {
            _edt_search->focus();
        }

        return true;
    }

    return __super::filter_special_key(vk);
}

bool MainWindow::filter_message(MSG* msg)
{
    return (_accels && ::TranslateAccelerator(_hwnd, _accels, msg)) || __super::filter_message(msg);
}

taowin::SystemControl* MainWindow::filter_control(HWND hwnd)
{
    if(_listview && hwnd == _listview->get_header())
        return _listview;
    return nullptr;
}

bool MainWindow::_start()
{
    std::wstring session;
    auto& etwobj = _config.obj("etw").as_obj();
    auto it= etwobj.find("session");
    if(it == etwobj.cend() || it->second.string_value().empty()) {
        session = L"taolog-session";
    }
    else {
        session = g_config.ws(it->second.string_value());
    }

    if (!_controller.start(session.c_str())) {
        msgbox(taowin::last_error(), MB_ICONERROR);
        return false;
    }

    g_logger_hwnd = _hwnd;
    g_logger_message = kDoLog;

    if (!_consumer.start(session.c_str())) {
        _controller.stop();
        msgbox(taowin::last_error(), MB_ICONERROR);
        return false;
    }

    int opend = 0;
    for (auto& mod : _modules) {
        if (mod->enable) {
            if (!_controller.enable(mod->guid, true, mod->level)) {
                msgbox(taowin::last_error(), MB_ICONERROR, L"无法开启模块：" + mod->name);
            }
            else {
                opend++;
            }
        }
    }

    if (opend == 0) {
        msgbox(L"没有模块或没有模块开启记录。", MB_ICONEXCLAMATION);
        _controller.stop();
        _consumer.stop();
        return false;
    }

    return true;
}

bool MainWindow::_stop()
{
    if(isetw()) {
        _controller.stop();
        _consumer.stop();
    }
    else if(isdbg()) {
        _dbgview.uninit();
    }

    return true;
}

void MainWindow::_init_control_variables()
{
    _btn_start          = _root->find<taowin::Button>(L"start-logging");
    _btn_clear          = _root->find<taowin::Button>(L"clear-logging");
    _btn_modules        = _root->find<taowin::Button>(L"module-manager");
    _btn_filter         = _root->find<taowin::Button>(L"filter-result");
    _edt_search         = _root->find<taowin::TextBox>(L"s");
    _cbo_search_filter  = _root->find<taowin::ComboBox>(L"s-filter");
    _cbo_sel_flt        = _root->find<taowin::ComboBox>(L"select-filter");
    _cbo_prj            = _root->find<taowin::ComboBox>(L"select-project");
    _btn_tools          = _root->find<taowin::Button>(L"tools");
    _listview           = _root->find<taowin::ListView>(L"lv");
}

void MainWindow::_init_control_events()
{
    _btn_start->on_click([this] {
        if(!_controller.started()) {
            if(_start()) {
                _btn_start->set_text(L"停止记录");
            }
        }
        else {
            _stop();
            _btn_start->set_text(L"开启记录");
        }
    });

    _btn_modules->on_click([this] { _manage_modules(); });
    _btn_filter->on_click([this] { _show_filters(); });
    _btn_clear->on_click([this] { _clear_results(); });
    _btn_tools->on_click([this] { _tools_menu->track(); });
    _listview->on_header_rclick([this] { _on_select_column(); return 0; });
    auto on_drag_column = [this](NMHDR* hdr) { _on_drag_column(hdr); return 0; };
    _listview->on_header_divider_dblclick(on_drag_column);
    _listview->on_header_end_track(on_drag_column);
    _listview->on_header_end_drag([this] {
        // 真正的结果要等到此函数返回之后才能
        // 拿得到，所以异步调用
        async_call([&] {
            _on_drop_column();
        });
        return FALSE; // allow drop
    });
}

void MainWindow::_init_listview()
{
    // 表头栏
    if(_config.has_arr("columns")) {
        auto add_col = [&](json11::Json jsoncol) {
            auto& c = JsonWrapper(jsoncol).as_obj();
            _columns.push(
                g_config.ws(c["name"].string_value()).c_str(),
                c["show"].bool_value(),
                std::min(std::max(c["width"].int_value(), 30), 1000), 
                c["id"].string_value().c_str()
            );
        };

        auto cols = _config.arr("columns");

        for(auto& jsoncol : cols.as_arr()) {
            add_col(jsoncol);
        }
    }
    else {
        _columns.push(L"编号", false,  50, "id"   );
        _columns.push(L"时间", true,   86, "time" );
        _columns.push(L"进程", false,  50, "pid"  );
        _columns.push(L"线程", false,  50, "tid"  );
        _columns.push(L"模块", true,  100, "proj" );
        _columns.push(L"文件", true,  140, "file" );
        _columns.push(L"函数", true,  100, "func" );
        _columns.push(L"行号", true,   50, "line" );
        _columns.push(L"等级", false, 100, "level");
        _columns.push(L"日志", true,  300, "log"  );

        // 初次使用时初始化配置文件
        if(!_config.has_arr("columns")) {
            auto& columns = _config.arr("columns").as_arr();

            for(auto& col : _columns.all()) {
                columns.push_back(col);
            }
        }
    }

    if(isdbg()) {
        int idx[] = {3,4,5,6,7,8};
        for(auto i : idx) {
            _columns[i].valid = false;
        }
    }

    // 第一列始终显示
    _columns[0].valid = true;
    _columns[0].show = true;

    _columns.update();

    _columns.for_each(ColumnManager::ColumnFlags::Showing, [&](int i, Column& c) {
        _listview->insert_column(c.name.c_str(), c.width, i);
    });

    if(_config.obj("listview").has_arr("column_orders")) {
        auto& orders = _config.obj("listview").arr("column_orders").as_arr();
        auto o = std::make_unique<int[]>(orders.size());
        for(int i = 0; i < (int)orders.size(); i++) {
            o.get()[i] = orders[i].int_value();
        }

        _listview->set_column_order((int)orders.size(), o.get());
    }

    // 列表颜色栏
    JsonWrapper config_listview = _config.obj("listview");
    if(config_listview.has_arr("colors")) {
        for(auto& color : config_listview.arr("colors").as_arr()) {
            auto& co = JsonWrapper(color).as_obj();

            int level = co["level"].int_value();

            unsigned int fgc[3], bgc[3];
            (void)::sscanf(co["fgc"].string_value().c_str(), "%d,%d,%d", &fgc[0], &fgc[1], &fgc[2]);
            (void)::sscanf(co["bgc"].string_value().c_str(), "%d,%d,%d", &bgc[0], &bgc[1], &bgc[2]);

            COLORREF crfg = RGB(fgc[0] & 0xff, fgc[1] & 0xff, fgc[2] & 0xff);
            COLORREF crbg = RGB(bgc[0] & 0xff, bgc[1] & 0xff, bgc[2] & 0xff);

            _colors.try_emplace(level, crfg, crbg);
        }
    }
    else {
        _colors.try_emplace(TRACE_LEVEL_VERBOSE,     RGB(  0,   0,   0), RGB(255, 255, 255));
        _colors.try_emplace(TRACE_LEVEL_INFORMATION, RGB(  0,   0,   0), RGB(255, 255, 255));
        _colors.try_emplace(TRACE_LEVEL_WARNING,     RGB(255, 128,   0), RGB(255, 255, 255));
        _colors.try_emplace(TRACE_LEVEL_ERROR,       RGB(255,   0,   0), RGB(255, 255, 255));
        _colors.try_emplace(TRACE_LEVEL_CRITICAL,    RGB(255, 255, 255), RGB(255,   0,   0));

        auto& colors = config_listview.arr("colors").as_arr();

        for(auto& pair : _colors) {
            auto fgp = (unsigned char*)(&pair.second.fg + 1);
            auto bgp = (unsigned char*)(&pair.second.bg + 1);

            char buf[2][12];
            sprintf(&buf[0][0], "%d,%d,%d", fgp[-4], fgp[-3], fgp[-2]);
            sprintf(&buf[1][0], "%d,%d,%d", bgp[-4], bgp[-3], bgp[-2]);

            colors.push_back(json11::Json::object {
                {"level",   pair.first},
                {"fgc",     buf[0]},
                {"bgc",     buf[1]},
            });
        }
    }

    // ListView 菜单
    std::wstring menustr = LR"(<MenuTree id="lv">
        <item id="clear" text="清空" />
        <sep />
        <item id="copy" text="复制" />
        <sep />
    )";

    if(isetw()) {
        menustr += LR"(<popup id="projects" text="模块"></popup>)";
    }

    menustr += LR"(
        <popup id="filters" text="过滤器"></popup>
        <sep />
        <item id="top" text="顶部" />
        <item id="bot" text="底部" />
        <sep />
        <item id="full" text="最大化" />
        <item id="column" text="选择列" />
    )";

    menustr += LR"(</MenuTree>)";

    _lvmenu = taowin::PopupMenu::create(menustr.c_str());
    add_menu(_lvmenu);

    // subclass it
    subclass_control(_listview);
}

void MainWindow::_init_config()
{
    // the gui main
    auto windows = g_config->obj("windows");

    // main window config
    _config = windows.obj("main");

    // 窗口标题
    {
        std::wstring tt(L"Log Viewer");
        if(_config.has_str("title")) {
            tt = g_config.ws(_config.str("title").as_str());
        }

        ::SetWindowText(_hwnd, tt.c_str());
    }

    // 窗口置顶与否
    _set_top_most(_config["topmost"].bool_value());
}

void MainWindow::_init_projects()
{
    if(isetw()) {
        // the modules
        auto modules = g_config->arr("modules");
        for(auto& mod : modules.as_arr()) {
            if(mod.is_object()) {
                if(auto m = ModuleEntry::from_json(mod)) {
                    _modules.push_back(m);
                    _projects[m] = EventPair();
                }
                else {
                    msgbox(L"无效模块配置。", MB_ICONERROR);
                }
            }
        }

        // 这样的话，当前过滤器就一直不为空了，少作些判断
        _projects[nullptr] = EventPair();
    }
    else {
        _projects[nullptr] = EventPair();
    }

    _module_source.set_source(&_modules);
    _cbo_prj->set_source(&_module_source);
}

void MainWindow::_init_project_events()
{
    g_evtsys.attach(L"project:set", [&] {
        auto m = static_cast<ModuleEntry*>(g_evtsys[0].ptr_value());
        _current_project = m;
        _events = &_projects[m].first;
        _filters = &_projects[m].second;
        _cbo_prj->set_cur_sel(m);
        g_evtsys.trigger(L"filter:set", _events, true);
        _filter_source.set_source(_events, _filters);
        _cbo_sel_flt->set_source(&_filter_source);
        _cbo_sel_flt->reload();
        _cbo_sel_flt->set_cur_sel(_events);
    });

    g_evtsys.attach(L"project:new", [&] {
        auto m = static_cast<ModuleEntry*>(g_evtsys[0].ptr_value());
        _projects[m] = EventPair();
        _cbo_prj->reload(true);
    });

    g_evtsys.attach(L"project:del", [&] {
        auto& items = *reinterpret_cast<std::vector<int>*>(g_evtsys[0].ptr_value());
        auto items_reverse = std::vector<int>(items.rbegin(), items.rend());
        for(auto item : items_reverse) {
            auto m = _modules[item];
            _modules.erase(_modules.cbegin() + item);
            _projects.erase(m);
            if(m == _current_project) {
                g_evtsys.trigger(L"project:set", nullptr);
            }
            delete m;
        }
        _cbo_prj->reload(true);
    });
}

void MainWindow::_init_filters()
{
    auto guid2mod = [&](const GUID& guid) {
        ModuleEntry* p = nullptr;
        for(auto& m : _modules) {
            if(::IsEqualGUID(m->guid, guid)) {
                p = m;
                break;
            }
        }
        return p;
    };

    auto name = isetw() ? "filters(etwlog)" : "filters(dbgview)";
    if(g_config->has_obj(name)) {
        auto& guids2filters = g_config->obj(name).as_obj();
        for(auto& pair : guids2filters) {
            GUID guid;
            if(SUCCEEDED(::CLSIDFromString(g_config.ws(pair.first).c_str(), &guid))) {
                auto& filters = pair.second.array_items();
                for(auto& f : filters) {
                    try {
                        if(auto fp = EventContainer::from_json(f)) {
                            ModuleEntry* mod = isdbg() ? nullptr : guid2mod(guid);
                            if(isdbg() && mod == nullptr || isetw() && mod != nullptr) {
                                _projects[mod].second.emplace_back(fp);
                            }
                        }
                    }
                    catch(const std::wstring& err) {
                        msgbox(err, MB_ICONERROR, L"无法构造过滤器");
                    }
                }
            }
        }
    }
}

void MainWindow::_init_menus()
{
    // 工具菜单
    _tools_menu = taowin::PopupMenu::create(LR"(
<MenuTree id="tools">
    <item id="topmost"       text="窗口置顶" />
    <item id="colors"        text="颜色配置" />
    <sep />
    <item id="guid"          text="创建日志实例" />
    <item id="export"        text="导出日志" />
    <sep />
    <item id="json_visual"   text="JSON 可视化" />
    <item id="lua_console"   text="LUA 控制台" />
    <sep />
    <item id="calc"          text="计算器" />
    <item id="notepad"       text="记事本" />
    <item id="cmd"           text="命令提示符" />
    <item id="regedit"       text="注册表" />
    <item id="control"       text="控制面板" />
    <item id="mstsc"         text="远程桌面" />
</MenuTree>
)");
    add_menu(_tools_menu);

    // 自定义工具
    const auto& tools = g_config->arr("tools").as_arr();
    if(!tools.empty()) {
        _tools_menu->insert_sep(L"", L"");
        int i = 0;
        for(auto& tool : tools) {
            _tools_menu->insert_str(L"", L"_" + std::to_wstring(i), g_config.ws(tool["name"].string_value()), true, L"", false);
            i++;
        }
    }
}

void MainWindow::_init_filter_events()
{
    g_evtsys.attach(L"filter:new", [&] {
        auto filter = static_cast<EventContainer*>(g_evtsys[0].ptr_value());
        _filters->push_back(filter);
        _current_filter->filter_results(filter);
        _cbo_sel_flt->reload(true);
    });

    // 先放这里吧……
    _event_source.SetColConv([this](int i) {
        return _columns.showing(i).index;
    });

    g_evtsys.attach(L"filter:set", [&] {
        auto p = static_cast<EventContainer*>(g_evtsys[0].ptr_value());
        if(!p) p = _events;
        bool eq = _current_filter == p;
        if(!eq) {
            _current_filter = p;
            _event_source.SetEvents(p);
            _listview->set_source(&_event_source);
        }
        if(p != _cbo_sel_flt->get_cur_data()) {
            _cbo_sel_flt->set_cur_sel(p);
        }
    });
    
    g_evtsys.attach(L"filter:del", [&] {
        int i = g_evtsys[0].int_value();
        auto it = _filters->begin() + i;

        if (*it == _current_filter) {
            _current_filter = _events;
            _event_source.SetEvents(_current_filter);
        }

        delete *it;
        _filters->erase(it);
        
        _cbo_sel_flt->reload(true);
    });

    g_evtsys.attach(L"filter:set", [&] {
        _searcher.invalid();
    });
}

void MainWindow::_init_logger_events()
{
    g_evtsys.attach(L"log:clear", [&] { _clear_results(); });

    g_evtsys.attach(L"log:fullscreen", [&] {
        auto toolbar = _root->find<taowin::Container>(L"toolbar");
        auto wrapper = _root->find<taowin::Vertical>(L"wrapper");
        bool full = toolbar->is_visible();
        toolbar->set_visible(!full);
        wrapper->set_attr(L"padding", full ? L"0,0,0,0" : L"5,5,5,5");
        wrapper->need_update();
        _listview->show_header(!full);
        _lvmenu->find_sib(L"full")->check(full);
    });

    g_evtsys.attach(L"log:copy", [&] {
        _copy_selected_item();
    });
}

void MainWindow::_view_detail(int i)
{
    auto get_column_name = [&](int i) {
        return i >= 0 && i <= (int)_columns.size()
            ? _columns[i].name.c_str()
            : L"";
    };

    auto evt = (*_current_filter)[i];
    auto& cr = _colors[evt->level];
    auto detail_window = new EventDetail(evt, get_column_name, cr.fg, cr.bg);
    detail_window->create(this);
    detail_window->show();
}

void MainWindow::_manage_modules()
{
    auto on_toggle_enable = [&](ModuleEntry* mod, bool enable, std::wstring* err) {
        if (!_controller.started())
            return true;

        if (!_controller.enable(mod->guid, enable, mod->level)) {
            *err = taowin::last_error();
            return false;
        }

        return true;
    };

    auto on_get_is_open = [&]() {
        return _controller.started();
    };

    auto mgr = new ModuleManager(_modules, _level_maps);
    mgr->on_toggle_enable(on_toggle_enable);
    mgr->on_get_is_open(on_get_is_open);
    mgr->domodal(this);
}

void MainWindow::_show_filters()
{
    if(isetw() && !_current_project) {
        msgbox(L"没有模块可用（增加或选择模块）。", MB_ICONEXCLAMATION);
        return;
    }

    auto get_base = [&](std::vector<std::wstring>* bases, int* def) {
        _columns.for_each(ColumnManager::ColumnFlags::Available, [&](int i, Column& c) {
            bases->push_back(c.name);
        });

        *def = (int)bases->size() - 1;
    };

    auto ongetvalues = [&](int baseindex, ResultFilter::IntStrPairs* values, bool* editable, taowin::ComboBox::OnDraw* ondraw) {
        values->clear();
        *editable = false;
        *ondraw = nullptr;

        const auto& id = _columns.avail(baseindex).id;

        if (id == "level") {
            for (auto& pair : _level_maps) {
                values->emplace_back(pair.first, pair.second.cmt2.c_str());
            }
        }
        else if(id == "proj") {
            auto& v = *values;
            for(auto& m : _modules) {
                // if(m->enable) {
                    // 这个索引没使用
                    // 比较的是 value_name 直接相等
                    values->emplace_back((int)m, m->name.c_str());
                // }
            }
        }
    };

    auto onok = [&](const std::wstring& name,
        int field_index, const std::wstring& field_name,
        int value_index, const std::wstring& value_name,
        const std::wstring& value_input)
    {
        auto real_index = _columns.avail(field_index).index;
        auto filter = new EventContainer(name, real_index, field_name, value_index, value_name, value_input, false);
        try {
            filter->enable_filter(true);
            g_evtsys.trigger(L"filter:new", filter);
        }
        catch(...) {
            delete filter;
            throw;
        }
    };

    auto dlg = new ResultFilter(*_filters, get_base, _current_project, _current_filter, ongetvalues, onok);
    dlg->domodal(this);
}

bool MainWindow::_do_search(bool first)
{
    if(first) {
        auto s = _edt_search->get_text();
        if(s.empty()) { return false; }

        // 搜索哪一列：-1：全部
        std::vector<int> cols;
        int col = (int)_cbo_search_filter->get_cur_data();
        if(col == -1) {
            _columns.for_each(ColumnManager::ColumnFlags::Showing, [&](int i, Column& c) {
                cols.emplace_back(c.index);
            });
        }
        else {
            cols.emplace_back(_columns.showing(col).index);
        }

        // 重置搜索引擎参数
        try {
            _searcher.reset(_current_filter, cols, s);
        }
        catch(const std::wstring& err) {
            msgbox(err, MB_ICONERROR, L"错误");
            return false;
        }
    }

    // 是向前还是向后搜索
    bool forward = !(::GetAsyncKeyState(VK_SHIFT) & 0x8000 && _searcher.last() != -1);

    // 执行搜索
    int last = _searcher.last();
    int next;
    try {
        next = _searcher.next(forward);
    }
    catch(const std::wstring& err) {
        msgbox(err, MB_ICONERROR, L"错误");
        return false;
    }

    DBG(L"搜索结果列: %d", next);
    if (next == -1) {
        msgbox(std::wstring(L"没有") + (forward ? L"下" : L"上") + L"一个了。", MB_ICONINFORMATION);
        _listview->focus();
        return false;
    }

    _listview->focus();
    _listview->ensure_visible(next);
    _listview->redraw_items(last, last);
    _listview->redraw_items(next, next);

    return true;
}

void MainWindow::_clear_results()
{
    if(_results_exporting.count(_current_project) && _results_exporting[_current_project]) {
        msgbox(L"当前模块的日志正在被导出，不能被清空，请稍候再试。", MB_ICONEXCLAMATION);
        return;
    }

    // 需要先关闭引用了日志记录的某某些（因为当前的日志记录没有引用计数功能）

    // 包括：查看详情窗口
    // 但它目前不会修改事件记录（仅初始化使用，所以先不管它）

    // 各事件过滤器应该清空了（它们只是引用）
    for (auto& f : *_filters)
        f->clear();

    // 主事件拥有日志事件，由它删除
    for(auto& e : _events->events()) {
        e->~LogDataUI();
        _log_pool.destroy(e);
    }

    _events->clear();

    // 更新界面
    _listview->update_source();
}

void MainWindow::_set_top_most(bool top)
{
    ::SetWindowPos(_hwnd, top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

// 主界面搜索栏
void MainWindow::_update_search_filter()
{
    // 保留当前选中的项（如果有的话）
    // 保存的是真实索引
    int cur_real_index = -1;

    if (_cbo_search_filter->get_cur_sel() != -1) {
        int ud = (int)_cbo_search_filter->get_cur_data();
        if (ud != -1) {
            cur_real_index = ud;
        }
    }

    // 重置内容
    _cbo_search_filter->clear();
    _cbo_search_filter->add_string(L"全部", (void*)-1);

    std::vector<const wchar_t*> strs {L"全部"};

    // 只添加已经显示的列
    int new_cur = 0;
    _columns.for_each(ColumnManager::ColumnFlags::Showing, [&](int i, Column& c) {
        _cbo_search_filter->add_string(c.name.c_str(), (void*)i);
        strs.push_back(c.name.c_str());
        if(c.index == cur_real_index) {
            new_cur = i + 1;
        }
    });

    if(cur_real_index == -1) {
        new_cur = _cbo_search_filter->get_count() - 1;
    }

    // 保持选中原来的项
    _cbo_search_filter->set_cur_sel(new_cur);
}

void MainWindow::_export2file()
{
    if(_current_filter->size() == 0)
        return;

    class ExportResult : public AsyncTask
    {
    public:
        ExportResult(ModuleEntry* mod, LogDataUIPtr logs[], int n, const std::wstring& file)
            : _m(mod)
            , _logs(logs)
            , _n(n)
            , _f(file)
        { 
        }

        void ondone(std::function<void(ModuleEntry* m, int ret)> f)
        {
            _ondone = f;
        }

        void onupdate(std::function<void(double ps)> f)
        {
            _onupdate = f;
        }

    protected:
        virtual int doit() override
        {
            std::ofstream file(_f, std::ios::binary|std::ios::trunc);
            if(!file.is_open()) {
                return -1;
            }

            std::wostringstream s;

            s <<
LR"(<!doctype html>
<html>
<head>
<meta charset="utf-8" />
<style>
table, td {
    border: 1px solid gray;
    border-collapse: collapse;
}

td {
    padding: 4px;
}

td:nth-child(10) {
    white-space: pre;
}
</style>
</head>
<body>
<table>
)";

            set_ps(0.);

            for(int i = 0; i < _n; ++i) {
                auto& log = *_logs[i];
                log.to_html_tr(s);
                if(i % 16 == 0) {
                    set_ps((double)i / _n * 100);
                }
            }

            set_ps(100.);

            s <<
LR"(
</table>
</body>
</html>
)";

            auto us = g_config.us(s.str());
            file << us;
            file.close();
            return 0;
        }

        virtual int done() override
        {
            _ondone(_m, get_ret());
            delete[] _logs;
            delete this;
            return 0;
        }

        virtual void update(double ps) override
        {
            _onupdate(ps);
        }

    protected:
        ModuleEntry* _m;
        LogDataUIPtr* _logs;
        int           _n;
        std::wstring _f;
        std::function<void(ModuleEntry*, int)>  _ondone;
        std::function<void(double)> _onupdate;
    };

    class ExportProgressDialog: public taowin::WindowCreator
    {
    public:
        ExportProgressDialog()
            : _done(false)
        { }

        void set_ps(double ps)
        {
            wchar_t buf[128];
            _swprintf(buf, L"%.2lf%%", ps);
            auto lbl = _root->find<taowin::Label>(L"progress");
            lbl->set_text(buf);
        }

        void done()
        {
            _done = true;
            msgbox(L"导出成功。", MB_ICONINFORMATION);
            close();
        }

        void fail()
        {
            _done = false;
            auto lbl = _root->find<taowin::Label>(L"progress");
            lbl->set_text(L"失败！");
            msgbox(L"导出失败！", MB_ICONERROR);
            close(1);
        }

    protected:
        virtual void get_metas(WindowMeta* metas) override
        {
            __super::get_metas(metas);
            metas->exstyle |= WS_EX_TOOLWINDOW;
            metas->exstyle &= ~WS_EX_APPWINDOW;
        }

        virtual LPCTSTR get_skin_xml() const override
        {
            LPCTSTR json = LR"tw(
                <Window title="正在导出日志..." size="250,100">
                    <Resource>
                        <Font name="default" face="微软雅黑" size="12"/>
                    </Resource>
                    <Root>
                        <Vertical padding="5,5,5,5">
                            <Control />
                            <Vertical height="24">
                                <Horizontal>
                                    <Control />
                                    <Horizontal width="100">
                                        <Label text="进度：" width="40" />
                                        <Label name="progress" />
                                    </Horizontal>
                                    <Control />
                                </Horizontal>
                            </Vertical>
                            <Control />
                        </Vertical>
                    </Root>
                </Window>
                )tw";
            return json;
        }

        virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override
        {
            if(umsg == WM_CLOSE) {
                if(!_done && wparam == 0) {
                    return 0;
                }
            }

            return __super::handle_message(umsg, wparam, lparam);
        }

        virtual void on_final_message() override {
            __super::on_final_message();
            delete this;
        }

    private:
        bool _done;
    };

    std::wstring file([&] {
        OPENFILENAME ofn = {0};
        ofn.lStructSize = sizeof(ofn);
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_ENABLESIZING | OFN_EXPLORER | OFN_NOREADONLYRETURN 
            | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        ofn.hInstance = ::GetModuleHandle(nullptr);
        ofn.hwndOwner = _hwnd;
        
        wchar_t buf[1024];
        buf[0] = L'\0';
        ofn.lpstrFile = buf;
        ofn.nMaxFile = _countof(buf);
        ofn.lpstrFilter = L"页面文件（*.html）\0*.html\0";
        ofn.lpstrDefExt = L"html";

        GetSaveFileName(&ofn);
        return std::move(std::wstring(buf));
    }());

    if(file.empty()) {
        return;
    }

    // std::vector 在扩容后指针数组会变无效
    // 所以这里直接复制指针（并确保此时不能清空日志）
    auto n = _current_filter->size();
    auto logs = new LogDataUIPtr[n];
    auto src = reinterpret_cast<LogDataUIPtr*>(&_current_filter->events()[0]);
    assert(sizeof(LogDataUIPtr) == sizeof(LogDataUI*));
    ::memcpy(logs, src, sizeof(LogDataUIPtr) * n);

    auto dlg = new ExportProgressDialog;
    dlg->create(this);
    dlg->show();

    auto task = new ExportResult(_current_project, logs, n, file);

    // TODO 费解，这里为什么要按值传
    task->ondone([&, dlg](ModuleEntry* m, int ret) {
        if(ret == 0)
            dlg->done();
        else
            dlg->fail();

        _results_exporting.erase(m);
    });

    task->onupdate([&, dlg](double ps) {
        dlg->set_ps(ps);
    });

    g_async.AddTask(task);

    _results_exporting.emplace(_current_project, true);
}

void MainWindow::_copy_selected_item()
{
    int i = _listview->get_next_item(-1, LVNI_SELECTED);
    if(i != -1) {
        // TODO 这个代码与查看详细时的代码是重复的
        auto get_column_name = [&](int i) {
            return i >= 0 && i <= (int)_columns.size()
                ? _columns[i].name.c_str()
                : L"";
        };

        auto log = (*_current_filter)[i];
        auto strlog = log->to_string(get_column_name);

        utils::set_clipboard_text(strlog);
    }
}

void MainWindow::_save_filters()
{
    auto name = isetw() ? "filters(etwlog)" : "filters(dbgview)";
    auto guid2filters = g_config->obj(name);

    guid2filters.as_obj().clear();

    for(auto& pair : _projects) {
        auto m = pair.first;
        if(isetw() && m || isdbg()) {
            auto guid = g_config.us(m ? m->guid_str : L"{00000000-0000-0000-0000-000000000000}");
            auto& filters = guid2filters.arr(guid.c_str()).as_arr();
            for(auto f : pair.second.second) {
                if(!f->is_tmp) {
                    filters.emplace_back(*f);
                }
            }
        }
    }
}

LRESULT MainWindow::_on_create()
{
    _init_control_variables();
    _init_control_events();

    _tipwnd->create();
    _tipwnd->set_font(_mgr.get_font(L"default"));

    _lua = luaL_newstate();
    luaL_openlibs(_lua);
    logdata_openlua(_lua);

    g_evtsys.attach(L"get_lua", [&] {
        auto lua = reinterpret_cast<lua_State**>(g_evtsys[0].ptr_value());
        *lua = _lua;
    });

    if(isdbg()) {
        _btn_start->set_visible(false);
        _btn_modules->set_visible(false);
        _cbo_prj->set_visible(false);
        _root->find<taowin::Control>(L"select-project-label")->set_visible(false);
    }

    _accels = ::LoadAccelerators(nullptr, MAKEINTRESOURCE(IDR_ACCELERATOR_MAINWINDOW));

    _init_config();

    _init_projects();
    _init_project_events();

    _init_filters();
    _init_filter_events();

    _init_logger_events();

    _init_listview();

    _init_menus();

    g_evtsys.trigger(L"project:set", isetw() && !_modules.empty() ? _modules[0] : nullptr, true);

    _level_maps.try_emplace(TRACE_LEVEL_VERBOSE,     L"Verbose",      L"详细(Verbose)"    );
    _level_maps.try_emplace(TRACE_LEVEL_INFORMATION, L"Information",  L"信息(Information)");
    _level_maps.try_emplace(TRACE_LEVEL_WARNING,     L"Warning",      L"警告(Warning)"    );
    _level_maps.try_emplace(TRACE_LEVEL_ERROR,       L"Error",        L"错误(Error)"      );
    _level_maps.try_emplace(TRACE_LEVEL_CRITICAL,    L"Critical",     L"严重(Critical)"   );

    _update_search_filter();

    if(isdbg()) {
        async_call([&] {
            auto fnGetFields = [&](std::vector<std::wstring>* fields, int* def) {
                _columns.for_each(ColumnManager::ColumnFlags::Available, [&](int i, Column& c) {
                    fields->emplace_back(c.name);
                });

                *def = (int)fields->size() - 1;
            };

            std::vector<EventContainer*> filters_added;

            auto fnGetValues = [&](int idx, ResultFilter::IntStrPairs* values, bool* editable, taowin::ComboBox::OnDraw* ondraw) {
                values->clear();
                *editable = false;
                *ondraw = nullptr;

                auto& v = *values;
                auto& c = _columns.avail(idx);

                // 如果是过滤日志的话，则添加已备份
                // 的过滤器规则到候选列表供选择
                if(c.id == "log") {
                    filters_added.clear();
                    for(auto fit = _filters->crbegin(), end = _filters->crend(); fit != end; ++fit) {
                        auto f = *fit;
                        if(_columns[f->field_index].id == "log") {
                            v.emplace_back(int(f), f->value_input.c_str());
                            filters_added.emplace_back(f);
                        }
                    }
                    *editable = true;
                    *ondraw = [&] (taowin::ComboBox* that, DRAWITEMSTRUCT* dis, int i, bool selected) {
                        const auto& str = filters_added[i]->name;
                        taowin::Rect rc(dis->rcItem);
                        COLORREF bgcolor = selected ? ::GetSysColor(COLOR_HIGHLIGHT) : RGB(255, 255, 255);
                        COLORREF fgcolor = selected ? RGB(255, 255, 255) : RGB(0, 0, 0);
                        HBRUSH hBrush = ::CreateSolidBrush(bgcolor);
                        ::FillRect(dis->hDC, &rc, hBrush);
                        rc.deflate(3, 0);
                        auto old_fg_color = ::SetTextColor(dis->hDC, fgcolor);
                        auto old_bg_color = ::SetBkColor(dis->hDC, bgcolor);
                        taowin::Rect rcText(rc);
                        rcText.right = rc.left + 90;
                        ::DrawText(dis->hDC, str.c_str(), str.size(), &rcText, 0);
                        if(rc.width() > 100) {
                            taowin::Rect rc(rc);
                            rc.left += 100;
                            auto& str = filters_added[i]->value_input;
                            ::SetTextColor(dis->hDC, RGB(192, 192, 192));
                            ::DrawText(dis->hDC, str.c_str(), str.size(), &rc, 0);
                        }
                        if(selected) ::DrawFocusRect(dis->hDC, &dis->rcItem);
                        ::SetTextColor(dis->hDC, old_fg_color);
                        ::SetBkColor(dis->hDC, old_bg_color);
                        ::DeleteBrush(hBrush);
                    };
                }
            };

            auto onnew = [&](const std::wstring& name,
                int field_index, const std::wstring& field_name,
                int value_index, const std::wstring& value_name,
                const std::wstring& value_input)
            {
                auto& c = _columns.avail(field_index);

                // 选择了已经存在的过滤器
                if(c.id == "log" && value_index != -1) {
                    auto f = reinterpret_cast<EventContainer*>(value_index);
                    async_call([f] { g_evtsys.trigger(L"filter:set", f); });
                }
                else {
                    auto filter = new EventContainer(name, c.index, field_name, value_index, value_name, value_input, false);
                    try {
                        filter->enable_filter(true);
                        g_evtsys.trigger(L"filter:new", filter);
                        g_evtsys.trigger(L"filter:set", filter);
                    }
                    catch(...) {
                        delete filter;
                        throw;
                    }
                }
            };

            AddNewFilter dlg(fnGetFields, fnGetValues, onnew);
            dlg.domodal(this);

            if(!_dbgview.init([&](DWORD pid, const char* str) {
                if(::IsWindowEnabled(_hwnd)) {
                    auto p = (LogDataUI*)::SendMessage(_hwnd, kDoLog, LoggerMessage::AllocLogUI, 0);
                    p = LogDataUI::from_dbgview(pid, str, p);
                    ::PostMessage(_hwnd, kDoLog, LoggerMessage::LogDbgMsg, LPARAM(p));
                }
                else {
                    auto s = new std::string(str);
                    ::PostMessage(_hwnd, kLogDbgViewRaw, WPARAM(pid), LPARAM(s));
                }
            }))
            {
                async_call([&]() {
                    msgbox(L"无法启动 DebugView 日志，当前可能有其它的 DebugView 日志查看器正在运行。", MB_ICONINFORMATION);
                    close();
                });
            }
        });
    }

    subclass_control(_edt_search);

    return 0;
}

LRESULT MainWindow::_on_close()
{
    if(!_results_exporting.empty()) {
        msgbox(L"目前尚有日志正在被导出，窗口不能被关闭。", MB_ICONEXCLAMATION);
        return 0;
    }

    if(_current_filter->size() && msgbox(L"确定关闭窗口？", MB_ICONQUESTION | MB_OKCANCEL) == IDCANCEL) {
        return 0;
    }

    _save_filters();

    _stop();

    ::DestroyWindow(_tipwnd->hwnd());

    lua_close(_lua);

    DestroyWindow(_hwnd);

    return 0;
}

LRESULT MainWindow::_on_log(LoggerMessage::Value msg, LPARAM lParam)
{
    if(msg == LoggerMessage::LogDbgMsg || msg == LoggerMessage::LogEtwMsg) {
        LogDataUI* logui;

        if(_logsystype == LogSysType::EventTracing) {
            auto logdata = reinterpret_cast<LogData*>(lParam);
            logui = LogDataUI::from_logdata(logdata, _log_pool.alloc());
        }
        else if(_logsystype == LogSysType::DebugView) {
            logui = reinterpret_cast<LogDataUI*>(lParam);
        }
        else {
            (logui=nullptr);
            assert(0);
        }

        LogDataUIPtr item(logui);

        ModuleEntry* m = isetw() ? _module_from_guid(item->guid) : nullptr;

        if(isetw()) {
            // 项目名称 & 项目根目录
            static std::wstring unknown_project(L"<unknown>");
            item->strProject = m ? &m->name : &unknown_project;

            const std::wstring* root = m ? &m->root : nullptr;

            // 相对路径
            item->offset_of_file = 0;
            if(*item->file && root) {
                if(::_wcsnicmp(item->file, root->c_str(), root->size()) == 0) {
                    item->offset_of_file = (int)root->size();
                }
            }
        }
        else if(isdbg()) {
            item->offset_of_file = 0;
        }

        auto& pair = _projects[m];
        auto& events = pair.first;
        auto& filters = pair.second;

        // 日志编号
        _snwprintf(item->id, _countof(item->id), L"%llu", (unsigned long long)events.size() + 1);

        // 字符串形式的日志等级
        item->strLevel = &_level_maps[item->level].cmt1;

        //////////////////////////////////////////////////////////////////////////

        // 判断一下当前过滤器是否添加了此事件
        // 如果没有添加，就不必要刷新列表控件了
        auto old_size = _current_filter->size();

        // 全部事件容器
        events.add(item);

        // 带过滤的事件容器（指针复用）
        if(!filters.empty()) {
            for(auto& f : filters) {
                if(!f->is_lua()) {
                    f->add(item);
                }
                else {
                    try {
                        f->add(item);
                    }
                    catch(const std::wstring& err) {
                        msgbox(err, MB_ICONERROR, L"执行脚本时错误");
                        f->enable_filter(false);
                    }
                }
            }
        }

        if(_current_filter->size() > old_size) {
            // 如果当前内容为空或焦点行是最后一行，则自动滚屏到最后
            int count = (int)_current_filter->size();
            int sic_flag = LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL;
            bool is_last_focused = count <= 1 || (_listview->get_item_state(count - 2, LVIS_FOCUSED) & LVIS_FOCUSED);

            if(is_last_focused) {
                sic_flag &= ~LVSICF_NOSCROLL;
            }

            _listview->set_item_count(count, sic_flag);

            if(is_last_focused) {
                _listview->set_item_state(-1, LVIS_FOCUSED | LVIS_SELECTED, 0);
                _listview->ensure_visible(count - 1);
                _listview->set_item_state(count - 1, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
            }
        }
    }
    else if(msg == LoggerMessage::AllocLogUI) {
        return LRESULT(_log_pool.alloc());
    }

    return 0;
}

LRESULT MainWindow::_log_raw_dbg(int pid, std::string* s)
{
    auto log = LogDataUI::from_dbgview(pid, s->c_str(), _log_pool.alloc());
    _on_log(LoggerMessage::LogDbgMsg, LPARAM(log));
    delete s;
    return 0;
}

LRESULT MainWindow::_on_custom_draw_listview(NMHDR * hdr)
{
    LRESULT lr = CDRF_DODEFAULT;

    auto lvcd = (LPNMLVCUSTOMDRAW)hdr;

    switch (lvcd->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        lr = CDRF_NOTIFYITEMDRAW;
        break;

    case CDDS_ITEM | CDDS_PREPAINT:
        if (_searcher.last() != -1 && (int)lvcd->nmcd.dwItemSpec == _searcher.last()) {
            lr = CDRF_NOTIFYSUBITEMDRAW;
            break;
        }
        else {
            LogDataUI& log = *(*_current_filter)[lvcd->nmcd.dwItemSpec];
            lvcd->clrText = _colors[log.level].fg;
            lvcd->clrTextBk = _colors[log.level].bg;
            lr = CDRF_NEWFONT;
        }
        break;

    case CDDS_ITEM|CDDS_SUBITEM|CDDS_PREPAINT:
    {
        bool hl = _searcher.match_cols()[_columns.showing(lvcd->iSubItem).index];

        if(hl) {
            lvcd->clrTextBk = RGB(0, 0, 255);
            lvcd->clrText = RGB(255, 255, 255);
        }
        else {
            LogDataUI& log = *(*_current_filter)[lvcd->nmcd.dwItemSpec];
            lvcd->clrText = _colors[log.level].fg;
            lvcd->clrTextBk = _colors[log.level].bg;
        }

        lr = CDRF_NEWFONT;
        break;
    }
    }

    return lr;
}

LRESULT MainWindow::_on_select_column()
{
    auto colsel = new ColumnSelection(_columns);

    colsel->OnToggle([&](int i) {
        auto& c = _columns.avail(i);

        if(!c.show) {
            int li;
            _columns.show(i, &li);
            _listview->insert_column(c.name.c_str(), c.width, li);
        }
        else {
            int li;
            _columns.hide(false, i, &li);
            _listview->delete_column(li);
        }

        c.show = !c.show;

        auto& columns = _config.arr("columns").as_arr();
        JsonWrapper(columns[c.index]).as_obj()["show"] = c.show;
    });

    colsel->domodal(this);

    _update_search_filter();

    return 0;
}

LRESULT MainWindow::_on_drag_column(NMHDR* hdr)
{
    auto nmhdr = (NMHEADER*)hdr;

    HDITEM hdi;
    if(!nmhdr->pitem) {
        // 仅获取宽度字段，不要使用其它值
        hdi.mask = HDI_WIDTH;
        Header_GetItem(hdr->hwndFrom, nmhdr->iItem, &hdi);
        nmhdr->pitem = &hdi;
    }

    auto& item = nmhdr->pitem;
    auto& col = _columns.showing(nmhdr->iItem);

    col.show = item->cxy != 0;
    if (item->cxy) col.width = item->cxy;

    if(!col.show) {
        int i = nmhdr->iItem;
        async_call([i, this]() {
            ListView_DeleteColumn(_listview->hwnd(), i);
            _columns.hide(true, i, nullptr);
        });
    }

    auto& columns = _config.arr("columns").as_arr();
    auto& colobj = JsonWrapper(columns[col.index]).as_obj();
    colobj["show"] = col.show;
    colobj["width"] = col.width;

    _update_search_filter();

    return 0;
}

// 拖动列表头后记住表头列的顺序
void MainWindow::_on_drop_column()
{
    // 只是当前真正的列数（因为有些可能已经删除）
    // 这个列数必然与 columns 中 show(visible) 的数量一致
    auto n = _listview->get_column_count();
    auto orders = std::make_unique<int[]>(n);
    if(_listview->get_column_order(n, orders.get())) {
        json11::Json::array order;

        order.reserve(n);

        for(int i = 0; i < n; i++) {
            order.push_back(orders.get()[i]);
        }

        auto& order_config = _config.obj("listview").arr("column_orders").as_arr();
        order_config = std::move(order);
    }
}

LRESULT MainWindow::_on_contextmenu(HWND hSender, int x, int y)
{
    return 0;
}

LRESULT MainWindow::_on_init_popupmenu(HMENU hPopup)
{
    taowin::MenuItem* sib;

    if (sib = _lvmenu->match_popup(L"filters", hPopup)){
        sib->clear();
        
        sib->insert_str(L"", std::to_wstring((int)_events), L"全部", true, L"", false);

        if(!_filters->empty()) sib->insert_sep(L"", L"");

        for(auto& f : *_filters) {
            sib->insert_str(L"", std::to_wstring((int)f), f->name, true, L"", false);
        }
    }
    else if(sib = _lvmenu->match_popup(L"projects", hPopup)) {
        sib->clear();
        for(auto& m : _modules) {
            sib->insert_str(L"", std::to_wstring((int)m), m->name, true, L"", false);
        }
    }

    return 0;
}

ModuleEntry* MainWindow::_module_from_guid(const GUID& guid)
{
    auto it = _guid2mod.find(guid);

    if(it == _guid2mod.cend()) {
        bool found = false;
        for (auto& item : _modules) {
            if (::IsEqualGUID(item->guid, guid)) {
                _guid2mod[guid] = item;
                it = _guid2mod.find(guid);
                found = true;
                break;
            }
        }
        if(!found) return nullptr;
    }

    return it->second;
}

}

#include "stdafx.h"

#include "misc/config.h"
#include "misc/utils.h"
#include "misc/event_system.hpp"

#include "../res/resource.h"

#include "main_window.h"

namespace taoetw {

static HWND g_logger_hwnd;
static UINT g_logger_message;

void DoEtwLog(void* log)
{
    ::SendMessage(g_logger_hwnd, g_logger_message, LoggerMessage::LogEtwMsg, LPARAM(log));
}

LPCTSTR MainWindow::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="ETW Log Viewer" size="900,650">
    <res>
        <font name="default" face="微软雅黑" size="12"/>
        <font name="12" face="微软雅黑" size="12"/>
    </res>
    <root>
        <vertical padding="5,5,5,5">
            <horizontal name="toolbar" height="30" padding="0,1,0,4">
                <button name="start-logging" text="开始记录" width="60" style="tabstop"/>
                <control width="5" />
                <button name="clear-logging" text="清空记录" width="60" style="tabstop"/>
                <control width="5" />
                <button name="module-manager" text="模块管理" width="60" style="tabstop"/>
                <control width="5" />
                <button name="filter-result" text="结果过滤" width="60" style="tabstop"/>
                <control width="5" />
                <button name="export-to-file" text="导出日志" width="60" style="tabstop"/>
                <control minwidth="30"/>
                <label name="select-project-label" text="模块：" width="38" style="centerimage"/>
                <combobox name="select-project" style="tabstop" height="200" width="64" padding="0,0,4,0"/>
                <label text="过滤：" width="38" style="centerimage"/>
                <combobox name="select-filter" style="tabstop" height="200" width="64" padding="0,0,4,0"/>
                <label text="查找：" width="38" style="centerimage"/>
                <combobox name="s-filter" style="tabstop" height="200" width="64" padding="0,0,4,0"/>
                <edit name="s" width="80" style="tabstop" exstyle="clientedge"/>
                <control width="10" />
                <button name="color-settings" text="颜色配置" width="60" style="tabstop"/>
                <control width="5" />
                <button name="topmost" text="窗口置顶" width="60" style="tabstop"/>
            </horizontal>
            <listview name="lv" style="showselalways,ownerdata,tabstop" exstyle="clientedge,doublebuffer,headerdragdrop"/>
        </vertical>
    </root>
</window>
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
    // case WM_CONTEXTMENU:    return _on_contextmenu(HWND(wparam), GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
    case WM_INITMENUPOPUP:  return _on_init_popupmenu(HMENU(wparam));
    case WM_NCACTIVATE:
    {
        if(wparam == FALSE && _tipwnd->showing()) {
            return TRUE;
        }
        break;
    }
    }
    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT MainWindow::control_message(taowin::syscontrol* ctl, UINT umsg, WPARAM wparam, LPARAM lparam)
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
            if(_listview->showtip_needed(pt, &s)) {
                _tipwnd->popup(s);
            }

            // 如果交给默认处理，会出现自动选中鼠标所在行的奇怪现象
            return 0;
        }
        else if(ctl == _edt_search) {
            auto tips = 
LR"(请输入待搜索的文本。

搜索时执行不区分大小写的普通文本搜索。
搜索上下文为：当前过滤器、当前搜索列。

\b快捷键：
\b    Ctrl + F        \w100-   聚焦搜索框\w100
\b    Enter           \w100-   执行搜索\w100
\b    F3              \w100-   搜索下一个\w100
\b    Shift + F3      \w100-   搜索上一个\w100
\b    Ctrl + Enter    \w100-   添加过滤器\w100
)";
            _tipwnd->format(tips);
            return 0;
        }
    }

    return __super::control_message(ctl, umsg, wparam, lparam);
}

LRESULT MainWindow::on_menu(const taowin::MenuIds& m)
{
    if(m[0] == L"lv") {
        if(m[1] == L"top")              { _listview->go_top(); }
        else if(m[1] == L"bot")         { _listview->go_bottom(); }
        else if(m[1] == L"clear")       { g_evtsys.trigger(L"log:clear"); }
        else if(m[1] == L"full")        { g_evtsys.trigger(L"log:fullscreen"); }
        else if(m[1] == L"copy")        { g_evtsys.trigger(L"log:copy"); }
        else if(m[1] == L"filters")     { g_evtsys.trigger(L"filter:set", (void*)std::stoi(m[2])); }
        else if(m[1] == L"projects")    { g_evtsys.trigger(L"project:set", (void*)std::stoi(m[2]), true); }
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

LRESULT MainWindow::on_notify(HWND hwnd, taowin::control * pc, int code, NMHDR * hdr)
{
    if (!pc) {
        if (hwnd == _listview->get_header()) {
            if (code == NM_RCLICK) {
                return _on_select_column();
            }
            else if (code == HDN_ENDTRACK || code == HDN_DIVIDERDBLCLICK) {
                return _on_drag_column(hdr);
            }
            else if(code == HDN_ENDDRAG) {
                // 真正的结果要等到此函数返回之后才能
                // 拿得到，所以异步调用
                async_call([&] {
                    _on_drop_column();
                });
                return FALSE; // allow drop
            }
        }

        return 0;
    }
    else if (pc == _listview) {
        if (code == LVN_GETDISPINFO) {
            return _on_get_dispinfo(hdr);
        }
        else if (code == NM_CUSTOMDRAW) {
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
            _lvmenu.track();
            return 0;
        }
        else if (code == LVN_KEYDOWN) {
            auto nmlv = reinterpret_cast<NMLVKEYDOWN*>(hdr);
            if (nmlv->wVKey == VK_F11) {
                g_evtsys.trigger(L"log:fullscreen");
                return 0;
            }
            else if (nmlv->wVKey == VK_F3) {
                _do_search(_last_search_string, _last_search_line, -1);
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
                _listview->redraw_items(_last_search_line, _last_search_line);
                _last_search_line = i;
            }
        }
    }
    else if (pc == _btn_start) {
        if(code == BN_CLICKED) {
            if(!_controller.started()) {
                if(_start()) {
                    _btn_start->set_text(L"停止记录");
                }
            }
            else {
                _stop();
                _btn_start->set_text(L"开启记录");
            }
        }
    }
    else if (pc == _btn_modules) {
        if(code == BN_CLICKED) {
            _manage_modules();
        }
    }
    else if (pc == _btn_filter) {
        if(code == BN_CLICKED) {
            _show_filters();
        }
    }
    else if (pc == _btn_topmost) {
        if(code == BN_CLICKED) {
            bool totop = !(::GetWindowLongPtr(_hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST);
            _set_top_most(totop);
            _config["topmost"] = totop;
        }
    }
    else if (pc == _btn_clear) {
        if(code == BN_CLICKED) {
            _clear_results();
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
    else if(pc == _btn_colors) {
        if(code == BN_CLICKED) {
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
    else if(pc == _btn_export2file) {
        if(code == BN_CLICKED) {
            _export2file();
            return 0;
        }
    }

    return 0;
}

bool MainWindow::filter_special_key(int vk)
{
    if (vk == VK_RETURN && ::GetFocus() == _edt_search->hwnd()) {
        _listview->redraw_items(_last_search_line, _last_search_line);
        _last_search_line = -1;
        _last_search_string = _edt_search->get_text();

        if (!_last_search_string.empty()) {
            if(_do_search(_last_search_string, _last_search_line, -1)) {
                // 如果有搜索结果，并且按住了CTRL键，则自动创建新的过滤器
                if(::GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                    // 当前选择的列（绝对索引）
                    int col = (int)_cbo_search_filter->get_cur_data();
                    if(col == -1) {
                        msgbox(L"新建过滤器不能指定为 <全部> 列。", MB_ICONINFORMATION);
                    }
                    else {
                        auto& c         = _columns.showing(col);
                        auto& name      = _last_search_string;
                        auto& colname   = c.name;
                        auto& value     = _last_search_string;
                        auto p = new EventContainer(_last_search_string, c.index, colname, -1, L"", value);
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
    }

    return __super::filter_special_key(vk);
}

bool MainWindow::filter_message(MSG* msg)
{
    return (_accels && ::TranslateAccelerator(_hwnd, _accels, msg)) || __super::filter_message(msg);
}

bool MainWindow::_start()
{
    std::wstring session;
    auto& etwobj = _config.obj("etw").as_obj();
    auto it= etwobj.find("session");
    if(it == etwobj.cend() || it->second.string_value().empty()) {
        session = L"taoetw-session";
        etwobj["session"] = g_config.us(session);
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

void MainWindow::_init_listview()
{
    _listview = _root->find<taowin::listview>(L"lv");

    // 表头栏
    if(_config.has_arr("columns")) {
        auto add_col = [&](json11::Json jsoncol) {
            auto& c = JsonWrapper(jsoncol).as_obj();
            _columns.push(
                g_config.ws(c["name"].string_value()).c_str(),
                c["show"].bool_value(),
                std::max(c["width"].int_value(), 30), 
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
        _columns.push(L"项目", true,  100, "proj" );
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
    std::wstring menustr = LR"(<menutree i="lv">
        <item i="clear" s="清空" />
        <sep />
        <item i="copy" s="复制" />
        <sep />
    )";

    if(isetw()) {
        menustr += LR"(<sub i="projects" s="项目"></sub>)";
    }

    menustr += LR"(
        <sub i="filters" s="过滤器"></sub>
        <sep />
        <item i="top" s="顶部" />
        <item i="bot" s="底部" />
        <sep />
        <item i="full" s="最大化" />
    )";

    menustr += LR"(</menutree>)";

    _lvmenu.create(menustr.c_str());
    add_menu(&_lvmenu);

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
}

void MainWindow::_init_project_events()
{
    g_evtsys.attach(L"project:set", [&] {
        auto m = static_cast<ModuleEntry*>(g_evtsys[0].ptr_value());
        bool bUpdateUI = g_evtsys[1].bool_value();
        _current_project = m;
        _update_project_list(m);
        _events = &_projects[m].first;
        _filters = &_projects[m].second;
        g_evtsys.trigger(L"filter:set", _events, true);
        _update_filter_list(_events);
    });

    g_evtsys.attach(L"project:new", [&] {
        auto m = static_cast<ModuleEntry*>(g_evtsys[0].ptr_value());
        _projects[m] = EventPair();
        _update_project_list(nullptr);
    });

    g_evtsys.attach(L"project:del", [&] {
        auto m = static_cast<ModuleEntry*>(g_evtsys[0].ptr_value());
        _update_project_list(nullptr);
        if(m == _current_project) {
            g_evtsys.trigger(L"project:set", nullptr);
        }
        _projects.erase(m);
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
                    if(auto fp = EventContainer::from_json(f)) {
                        ModuleEntry* mod = isdbg() ? nullptr : guid2mod(guid);
                        if(isdbg() && mod == nullptr || isetw() && mod != nullptr) {
                            _projects[mod].second.emplace_back(fp);
                        }
                    }
                }
            }
        }
    }
}

void MainWindow::_init_filter_events()
{
    g_evtsys.attach(L"filter:new", [&] {
        auto filter = static_cast<EventContainer*>(g_evtsys[0].ptr_value());
        _filters->push_back(filter);
        _current_filter->filter_results(filter);
        _update_filter_list(nullptr);
    });

    g_evtsys.attach(L"filter:set", [&] {
        auto p = static_cast<EventContainer*>(g_evtsys[0].ptr_value());
        if(!p) p = _events;
        bool eq = _current_filter == p;
        _current_filter = p ? p : _events;
        if(!eq) {
            _listview->set_item_count(_current_filter->size(), 0);
            _listview->redraw_items(0, _listview->get_item_count() -1);
        }
        _update_filter_list(p);
    });
    
    g_evtsys.attach(L"filter:del", [&] {
        int i = g_evtsys[0].int_value();
        auto it = _filters->begin() + i;

        if (*it == _current_filter) {
            _current_filter = _events;
            _listview->set_item_count(_current_filter->size(), 0);
            _listview->redraw_items(0, _listview->get_item_count() -1);
        }

        delete *it;
        _filters->erase(it);

        _update_filter_list(nullptr);
    });
}

void MainWindow::_init_logger_events()
{
    g_evtsys.attach(L"log:clear", [&] { _clear_results(); });

    g_evtsys.attach(L"log:fullscreen", [&] {
        auto toolbar = _root->find<taowin::container>(L"toolbar");
        toolbar->set_visible(!toolbar->is_visible());
        _listview->show_header(toolbar->is_visible());
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
        msgbox(L"没有模块可用（增加或选择项目）。", MB_ICONEXCLAMATION);
        return;
    }

    auto get_base = [&](std::vector<std::wstring>* bases, int* def) {
        _columns.for_each(ColumnManager::ColumnFlags::Available, [&](int i, Column& c) {
            bases->push_back(c.name);
        });

        *def = (int)bases->size() - 1;
    };

    auto ongetvalues = [&](int baseindex, ResultFilter::IntStrPairs* values, bool* editable) {
        values->clear();
        *editable = false;

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

    auto onok = [&](const std::wstring& name, int field_index, const std::wstring& field_name, int value_index, const std::wstring& value_name, const std::wstring& value_input) {
        auto real_index = _columns.avail(field_index).index;
        auto filter = new EventContainer(name, real_index, field_name, value_index, value_name, value_input);
        g_evtsys.trigger(L"filter:new", filter);
    };

    auto dlg = new ResultFilter(*_filters, get_base, _current_project, _current_filter, ongetvalues, onok);
    dlg->domodal(this);
}

bool MainWindow::_do_search(const std::wstring& s, int line, int)
{
    // 搜索行/列的判断在后面（因为有提示框）
    if (s.empty()) { return false; }

    // 得到下一搜索行和列
    int dir = ::GetAsyncKeyState(VK_SHIFT) & 0x8000 ? -1 : 1;
    int next_line = dir == 1 ? line + 1 : line - 1;

    // 此行是否有效
    bool valid = false;

    // 搜索哪一列：-1：全部
    int fltcol = (int)_cbo_search_filter->get_cur_data();

    // 重置列匹配结果标记
    for (auto& b : _last_search_matched_cols)
        b = false;

    // 均转换成小写来搜索
    std::wstring needle = s;
    std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);

    // 搜索函数
    auto search_text = [&](std::wstring /* 非引用 */heystack) {
        // 同样转换成小写来被搜索
        std::transform(heystack.begin(), heystack.end(), heystack.begin(), ::tolower);

        return ::wcsstr(heystack.c_str(), needle.c_str()) != nullptr;
    };

    for (; next_line >= 0 && next_line < (int)_current_filter->size();) {
        auto& evt = *(*_current_filter)[next_line];

        // 搜索每一列
        if (fltcol == -1) {
            // 是否至少有一列已经能够匹配
            bool has_col_match = false;

            // 一次性匹配整行
            _columns.for_each(ColumnManager::ColumnFlags::Showing, [&](int i, Column& c) {
                int real_index = c.index;
                if(search_text(evt[real_index])) {
                    _last_search_matched_cols[i] = true;
                    has_col_match = true;
                }
            });

            valid = has_col_match;
        }
        // 仅搜索指定列
        else {
            int real_index = _columns.showing(fltcol).index;
            if (search_text(evt[real_index])) {
                _last_search_matched_cols[fltcol] = true;
                valid = true;
            }
        }

        if (valid) break;

        // 没找到，继续往下搜索
        next_line += dir == 1 ? 1 : -1;
    }

    if (!valid) {
        msgbox(std::wstring(L"没有") + (dir==1 ? L"下" : L"上") + L"一个了。", MB_ICONINFORMATION);
        _listview->focus();
        return false;
    }

    _listview->focus();
    _listview->ensure_visible(next_line);
    _listview->redraw_items(line, line);
    _listview->redraw_items(next_line, next_line);

    _last_search_line = next_line;

    return true;
}

void MainWindow::_clear_results()
{
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
    _listview->set_item_count(0, 0);
}

void MainWindow::_set_top_most(bool top)
{
    _btn_topmost->set_text(top ? L"取消置顶" : L"窗口置顶");
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
    _cbo_search_filter->reset_content();
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
        new_cur = 0;
    }

    // 保持选中原来的项
    _cbo_search_filter->set_cur_sel(new_cur);

    _cbo_search_filter->adjust_droplist_width(strs);
}

void MainWindow::_update_filter_list(EventContainer* p)
{
    // 保留当前选中的项（如果有的话）
    EventContainer* cur = p;
    if(!p && _cbo_sel_flt->get_cur_sel() != -1) {
        void* ud = _cbo_sel_flt->get_cur_data();
        cur = (EventContainer*)ud;
    }

    // 重置内容
    _cbo_sel_flt->reset_content();
    _cbo_sel_flt->add_string(L"全部", _events);

    std::vector<const wchar_t*> strs {L"全部"};
    int new_cur = 0;
    for(size_t i = 0, j = 0; i < _filters->size(); i++) {
        auto flt = (*_filters)[i];
        _cbo_sel_flt->add_string(flt->name.c_str(), (void*)flt);
        strs.push_back(flt->name.c_str());
        j++;
        if(cur == flt) {
            new_cur = j;
        }
    }

    // 保持选中原来的项
    _cbo_sel_flt->set_cur_sel(new_cur);
    _cbo_sel_flt->adjust_droplist_width(strs);
}

void MainWindow::_update_project_list(ModuleEntry* m)
{
    // 保留当前选中的项（如果有的话）
    ModuleEntry* cur = m;
    if(!m && _cbo_prj->get_cur_sel() != -1) {
        void* ud = _cbo_prj->get_cur_data();
        cur = (ModuleEntry*)ud;
    }

    // 重置内容
    _cbo_prj->reset_content();

    // 添加列表
    std::vector<const wchar_t*> strs;
    int new_cur = -1;
    int j = -1;
    for(auto& m : _modules) {
        _cbo_prj->add_string(m->name.c_str(), m);
        strs.push_back(m->name.c_str());
        j++;
        if(cur == m) {
            new_cur = j;
        }
    }

    // 保持选中原来的项
    _cbo_prj->set_cur_sel(new_cur);
    _cbo_prj->adjust_droplist_width(strs);
}

void MainWindow::_export2file()
{
    std::wostringstream s;

    s << LR"(<!doctype html>
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

    for(const auto& log : _current_filter->events()) {
        log->to_html_tr(s);
    }

    s << LR"(
</table>
</body>
</html>
)";

    std::ofstream file(L"export.html", std::ios::binary|std::ios::trunc);
    if(file.is_open()) {
        auto us = g_config.us(s.str());
        file << us;
        file.close();
        msgbox(L"已保存到 export.html。");
    }
    else {
        msgbox(L"没能正确导出。");
    }
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
                filters.emplace_back(*f);
            }
        }
    }
}

LRESULT MainWindow::_on_create()
{
    _tipwnd->create();
    _tipwnd->set_font(_mgr.get_font(L"default"));

    _btn_start          = _root->find<taowin::button>(L"start-logging");
    _btn_clear          = _root->find<taowin::button>(L"clear-logging");
    _btn_modules        = _root->find<taowin::button>(L"module-manager");
    _btn_filter         = _root->find<taowin::button>(L"filter-result");
    _btn_topmost        = _root->find<taowin::button>(L"topmost");
    _edt_search         = _root->find<taowin::edit>(L"s");
    _cbo_search_filter         = _root->find<taowin::combobox>(L"s-filter");
    _btn_colors         = _root->find<taowin::button>(L"color-settings");
    _btn_export2file    = _root->find<taowin::button>(L"export-to-file");
    _cbo_sel_flt        = _root->find<taowin::combobox>(L"select-filter");
    _cbo_prj            = _root->find<taowin::combobox>(L"select-project");

    if(isdbg()) {
        _btn_start->set_visible(false);
        _btn_modules->set_visible(false);
        _cbo_prj->set_visible(false);
        _root->find<taowin::control>(L"select-project-label")->set_visible(false);
    }

    _accels = ::LoadAccelerators(nullptr, MAKEINTRESOURCE(IDR_ACCELERATOR_MAINWINDOW));

    _init_config();

    _init_projects();
    _init_project_events();

    _init_filters();
    _init_filter_events();

    _init_logger_events();

    _init_listview();

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

            std::vector<std::wstring> friendly_names;

            auto fnGetValues = [&](int idx, ResultFilter::IntStrPairs* values, bool* editable) {
                values->clear();
                *editable = false;

                auto& v = *values;
                auto& c = _columns.avail(idx);

                // 如果是过滤日志的话，则添加已备份
                // 的过滤器规则到候选列表供选择
                if(c.id == "log") {
                    friendly_names.clear();
                    for(auto fit = _filters->crbegin(), end = _filters->crend(); fit != end; ++fit) {
                        auto f = *fit;
                        if(_columns[f->field_index].id == "log") {
                            friendly_names.emplace_back(f->name +  L'[' + f->value_input + L']');
                            v.emplace_back(int(f), friendly_names.back().c_str());
                        }
                    }
                    *editable = true;
                }
            };

            AddNewFilter dlg(fnGetFields, fnGetValues);
            if(dlg.domodal(this) == IDOK) {
                auto& c = _columns.avail(dlg.field_index);

                // 选择了已经存在的过滤器
                if(c.id == "log" && dlg.value_index != -1) {
                    auto f = reinterpret_cast<EventContainer*>(dlg.value_index);
                    async_call([f] { g_evtsys.trigger(L"filter:set", f); });
                }
                else {
                    auto filter = new EventContainer(dlg.name, c.index, dlg.field_name, dlg.value_index, dlg.value_name, dlg.value_input);
                    g_evtsys.trigger(L"filter:new", filter);
                    g_evtsys.trigger(L"filter:set", filter);
                }
            }

            if(!_dbgview.init([&](DWORD pid, const char* str) {
                auto p = (LogDataUI*)::SendMessage(_hwnd, kDoLog, LoggerMessage::AllocLogUI, 0);
                p = LogDataUI::from_dbgview(pid, str, p);
                ::PostMessage(_hwnd, kDoLog, LoggerMessage::LogDbgMsg, LPARAM(p));
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
    _save_filters();

    _stop();

    ::DestroyWindow(_tipwnd->hwnd());

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
            item->strProject = m ? m->name : L"<unknown>";

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
                f->add(item);
            }
        }

        if(_current_filter->size() > old_size) {
            // 默认是非自动滚屏到最后一行的
            // 但如果当前焦点行是最后一行，则自动滚屏
            int count = (int)_current_filter->size();
            int sic_flag = LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL;
            bool is_last_focused = count > 1 && (_listview->get_item_state(count - 2, LVIS_FOCUSED) & LVIS_FOCUSED);

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
        if (_last_search_line != -1 && (int)lvcd->nmcd.dwItemSpec == _last_search_line) {
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
        bool hl = _last_search_matched_cols[lvcd->iSubItem];

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

LRESULT MainWindow::_on_get_dispinfo(NMHDR * hdr)
{
    auto pdi = reinterpret_cast<NMLVDISPINFO*>(hdr);
    auto lit = &pdi->item;

    // WTF: 竟然有负数的索引号（拖动鼠标任意乱选会出现）
    if(lit->iItem < 0) {
        lit->pszText = L"<listview bug>";
        return 0;
    }

    auto& evt = *(*_current_filter)[lit->iItem];

    int listview_col = lit->iSubItem;
    int evt_col = _columns.showing(listview_col).index;
    auto field = evt[evt_col];

    lit->pszText = const_cast<LPWSTR>(field);

    return 0;
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
    taowin::menu_manager::sibling* sib;

    if (sib = _lvmenu.match_popup(L"filters", hPopup)){
        _lvmenu.clear_popup(sib);
        
        _lvmenu.insert_str(sib, std::to_wstring((int)_events), L"全部", true);

        if(!_filters->empty()) _lvmenu.insert_sep(sib);

        for(auto& f : *_filters) {
            _lvmenu.insert_str(sib, std::to_wstring((int)f), f->name, true);
        }
    }
    else if(sib = _lvmenu.match_popup(L"projects", hPopup)) {
        _lvmenu.clear_popup(sib);
        for(auto& m : _modules) {
            _lvmenu.insert_str(sib, std::to_wstring((int)m), m->name, true);
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

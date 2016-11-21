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
    ::SendMessage(g_logger_hwnd, g_logger_message, LoggerMessage::LogMsg, LPARAM(log));
}

LPCTSTR MainWindow::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="ETW Log Viewer" size="820,600">
    <res>
        <font name="default" face="΢���ź�" size="12"/>
        <font name="12" face="΢���ź�" size="12"/>
    </res>
    <root>
        <vertical padding="5,5,5,5">
            <horizontal name="toolbar" height="30" padding="0,1,0,4">
                <button name="start-logging" text="��ʼ��¼" width="60" style="tabstop"/>
                <control width="5" />
                <button name="clear-logging" text="��ռ�¼" width="60" style="tabstop"/>
                <control width="5" />
                <button name="module-manager" text="ģ�����" width="60" style="tabstop"/>
                <control width="5" />
                <button name="filter-result" text="�������" width="60" style="tabstop"/>
                <control width="5" />
                <button name="export-to-file" text="������־" width="60" style="tabstop"/>
                <control width="5" />
                <control />
                <label text="���ˣ�" width="38" style="centerimage"/>
                <combobox name="select-filter" style="tabstop" height="400" width="64" padding="0,0,4,0"/>
                <label text="���ң�" width="38" style="centerimage"/>
                <combobox name="s-filter" style="tabstop" height="400" width="64" padding="0,0,4,0"/>
                <edit name="s" width="80" style="tabstop" exstyle="clientedge"/>
                <control width="10" />
                <button name="color-settings" text="��ɫ����" width="60" style="tabstop"/>
                <control width="5" />
                <button name="topmost" text="�����ö�" width="60" style="tabstop"/>
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
    case WM_CREATE: return _on_create();
    case WM_CLOSE:  return _on_close();
    case kDoLog:    return _on_log(LoggerMessage::Value(wparam), lparam);
    case WM_NCACTIVATE:
    {
        if(wparam == FALSE && _tipwnd->showing()) {
            return FALSE;
        }
        break;
    }
    }
    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT MainWindow::control_message(taowin::syscontrol* ctl, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    if(ctl == _listview) {
        static bool mi = false;

        if(umsg == WM_MOUSEMOVE) {
            if(!mi) {
                TRACKMOUSEEVENT tme = {0};
                tme.cbSize = sizeof(tme);
                tme.hwndTrack = _listview->hwnd();
                tme.dwFlags = TME_HOVER | TME_LEAVE;
                tme.dwHoverTime = HOVER_DEFAULT;
                _TrackMouseEvent(&tme);
                mi = true;
            }
        }
        else if(umsg == WM_MOUSELEAVE) {
            mi = false;
        }
        if(umsg == WM_MOUSEHOVER) {
            mi = false;

            LVHITTESTINFO hti;
            hti.pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};

            if(_listview->subitem_hittest(&hti) != -1/* && hti.iSubItem == 9*/) {
                // TODO �������߼�Ӧ����Ҫ����İ�����
                NMLVDISPINFO info;
                info.item.iItem = hti.iItem;
                info.item.iSubItem = hti.iSubItem;
                _on_get_dispinfo((NMHDR*)&info);

                bool need_tip = false;
                auto text = info.item.pszText;

                if(!need_tip) {
                    if(wcschr(text, L'\n')) {
                        need_tip = true;
                    }
                }

                // ����ʼ���ᱨǱ��ʹ����δ��ʼ���ı�������ʵ���ϲ����ܣ�
                int text_width = 0;
                constexpr int text_padding = 20;

                if(!need_tip) {
                    HDC hdc = ::GetDC(_listview->hwnd());
                    HFONT hFont = (HFONT)::SendMessage(_listview->hwnd(), WM_GETFONT, 0, 0);
                    HFONT hOldFont = SelectFont(hdc, hFont);

                    SIZE szText = {0};
                    if(::GetTextExtentPoint32(hdc, text, wcslen(text), &szText)) {
                        int col_width = _columns[hti.iSubItem].width;
                        text_width = szText.cx + text_padding;

                        if(text_width > col_width) {
                            need_tip = true;
                        }
                    }

                    SelectFont(hdc, hOldFont);

                    ::ReleaseDC(_listview->hwnd(), hdc);
                }

                if(!need_tip) {
                    taowin::Rect rcSubItem, rcListView;
                    ::GetClientRect(_listview->hwnd(), &rcListView);

                    if(_listview->get_subitem_rect(0, hti.iSubItem, &rcSubItem)) {
                        if(rcSubItem.left < rcListView.left || rcSubItem.left + text_width > rcListView.right) {
                            need_tip = true;
                        }
                    }
                }

                if(need_tip && !_tipwnd->showing()) {
                    _tipwnd->popup(text, _mgr.get_font(L"default"));
                }
            }

            return 0;
        }
    }
    return __super::control_message(ctl, umsg, wparam, lparam);
}

LRESULT MainWindow::on_menu(int id, bool is_accel)
{
    if(is_accel) {
        switch(id)
        {
        case IDR_ACCEL_MAINWINDOW_SEARCH:
            _edt_search->focus();
            _edt_search->set_sel(0, -1);
            break;
        }
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
            else if (code == HDN_ENDTRACK) {
                return _on_drag_column(hdr);
            }
            else if(code == HDN_ENDDRAG) {
                // �����Ľ��Ҫ�ȵ��˺�������֮�����
                // �õõ��������첽����
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
        else if (code == LVN_KEYDOWN) {
            auto nmlv = reinterpret_cast<NMLVKEYDOWN*>(hdr);
            if (nmlv->wVKey == VK_F11) {
                auto toolbar = _root->find<taowin::container>(L"toolbar");
                toolbar->set_visible(!toolbar->is_visible());
                _listview->show_header(toolbar->is_visible());
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
                    _btn_start->set_text(L"ֹͣ��¼");
                }
            }
            else {
                _stop();
                _btn_start->set_text(L"������¼");
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
    else if (pc == _cbo_filter) {
        if (code == CBN_SELCHANGE) {
            _edt_search->set_sel(0, -1);
            _edt_search->focus();
        }
    }
    else if(pc == _cbo_sel_flt) {
        if(code == CBN_SELCHANGE) {
            _set_current_filter((EventContainer*)_cbo_sel_flt->get_cur_data());
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
                // �����������������Ұ�ס��CTRL�������Զ������µĹ�����
                if(::GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                    // ��ǰѡ�����
                    int col = (int)_cbo_filter->get_cur_data();
                    if(col == -1) {
                        msgbox(L"�½�����������ָ��Ϊ <ȫ��> �С�", MB_ICONINFORMATION);
                    }
                    else {
                        auto& name      = _last_search_string;
                        auto& colname   = _columns[col].name;
                        auto& value     = _last_search_string;
                        auto p = new EventContainer(_last_search_string, col, colname, -1, L"", value);
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
                msgbox(taowin::last_error(), MB_ICONERROR, L"�޷�����ģ�飺" + mod->name);
            }
            else {
                opend++;
            }
        }
    }

    if (opend == 0) {
        msgbox(L"û��ģ�����ü�¼��", MB_ICONEXCLAMATION);
        _controller.stop();
        _consumer.stop();
        return false;
    }

    return true;
}

bool MainWindow::_stop()
{
    _controller.stop();
    _consumer.stop();

    return true;
}

void MainWindow::_init_listview()
{
    _listview = _root->find<taowin::listview>(L"lv");

    // ��ͷ��
    if(_config.has_arr("columns")) {
        auto add_col = [&](json11::Json jsoncol) {
            auto& c = JsonWrapper(jsoncol).as_obj();
            _columns.emplace_back(
                g_config.ws(c["name"].string_value()).c_str(),
                c["show"].bool_value(),
                c["width"].int_value(), 
                c["id"].string_value().c_str(),
                c["li"].int_value()
            );
        };

        auto cols = _config.arr("columns");

        for(auto& jsoncol : cols.as_arr()) {
            add_col(jsoncol);
        }
    }
    else {
        _columns.emplace_back(L"���", false,  50, "id"   ,0);
        _columns.emplace_back(L"ʱ��", true,   86, "time" ,1);
        _columns.emplace_back(L"����", false,  50, "pid"  ,2);
        _columns.emplace_back(L"�߳�", false,  50, "tid"  ,3);
        _columns.emplace_back(L"��Ŀ", true,  100, "proj" ,4);
        _columns.emplace_back(L"�ļ�", true,  140, "file" ,5);
        _columns.emplace_back(L"����", true,  100, "func" ,6);
        _columns.emplace_back(L"�к�", true,   50, "line" ,7);
        _columns.emplace_back(L"�ȼ�", false, 100, "level",8);
        _columns.emplace_back(L"��־", true,  300, "log"  ,9);

        // ����ʹ��ʱ��ʼ�������ļ�
        if(!_config.has_arr("columns")) {
            auto& columns = _config.arr("columns").as_arr();

            for(auto& col : _columns) {
                columns.push_back(col);
            }
        }
    }

    for (int i = 0; i < (int)_columns.size(); i++) {
        auto& col = _columns[i];
        _listview->insert_column(col.name.c_str(), col.show ? col.width : 0, i);
    }

    if(_config.obj("listview").has_arr("column-order")) {
        auto& orders = _config.obj("listview").arr("column-order").as_arr();
        auto o = std::make_unique<int[]>(orders.size());
        for(int i = 0; i < (int)orders.size(); i++) {
            o.get()[i] = orders[i].int_value();
        }

        _listview->set_column_order((int)orders.size(), o.get());
    }

    // �б���ɫ��
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

    // subclass it
    subclass_control(_listview);
}

void MainWindow::_init_config()
{
    // the gui main
    auto windows = g_config->obj("windows");

    // main window config
    _config = windows.obj("main");

    // ���ڱ���
    {
        std::wstring tt(L"ETW Log Viewer");
        if(_config.has_str("title")) {
            tt = g_config.ws(_config.str("title").as_str());
        }

        ::SetWindowText(_hwnd, tt.c_str());
    }

    // �����ö����
    _set_top_most(_config["topmost"].bool_value());

    // the modules
    auto modules = g_config->arr("modules");
    for(auto& mod : modules.as_arr()) {
        if(mod.is_object()) {
            if(auto m = ModuleEntry::from_json(mod)) {
                _modules.push_back(m);
            }
            else {
                msgbox(L"��Чģ�����á�", MB_ICONERROR);
            }
        }
    }
}

void MainWindow::_init_filters()
{
    if(g_config->has_arr("filters")) {
        auto filters = g_config->arr("filters");
        for(auto& fo : filters.as_arr()) {
            if(fo.is_object()) {
                if(auto fp = EventContainer::from_json(fo)) {
                    _filters.emplace_back(fp);
                }
                else {
                    msgbox(L"��Ч�Ĺ��������á�", MB_ICONERROR);
                }
            }
        }
    }
}

void MainWindow::_init_filter_events()
{
    g_evtsys.attach(L"filter:new", [&]() {
        auto p = static_cast<EventContainer*>(g_evtsys[0].ptr_value());
        _filters.push_back(p);
        _current_filter->filter_results(p);
        _update_filter_list(nullptr);
    });

    g_evtsys.attach(L"filter:set", [&]() {
        auto p = static_cast<EventContainer*>(g_evtsys[0].ptr_value());
        if(!p) p = &_events;
        _set_current_filter(p);
        _update_filter_list(p);
    });
    
    g_evtsys.attach(L"filter:del", [&]() {
        int i = g_evtsys[0].int_value();
        auto it = _filters.begin() + i;

        if (*it == _current_filter) {
            _current_filter = &_events;
            _listview->set_item_count(_current_filter->size(), 0);
            _listview->redraw_items(0, _listview->get_item_count() -1);
        }

        delete *it;
        _filters.erase(it);

        _update_filter_list(nullptr);
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
    auto get_base = [&](std::vector<std::wstring>* bases, int* def) {
        for (auto& col : _columns) {
            bases->push_back(col.name);
        }

        *def = (int)_columns.size() - 1;
    };

    auto ondelete = [&](int i) {
    };

    auto ongetvalues = [&](int baseindex, std::unordered_map<int, const wchar_t*>* values) {
        values->clear();

        const auto& id = _columns[baseindex].id;

        if (id == "level") {
            for (auto& pair : _level_maps) {
                (*values)[pair.first] = pair.second.cmt2.c_str();
            }
        }
        else if(id == "proj") {
            auto& v = *values;
            for(auto& m : _modules) {
                // if(m->enable) {
                    v[(int)m] = m->name.c_str();
                // }
            }
        }
    };

    auto dlg = new ResultFilter(_filters, get_base, _current_filter, ongetvalues);
    dlg->domodal(this);
}

bool MainWindow::_do_search(const std::wstring& s, int line, int)
{
    // ������/�е��ж��ں��棨��Ϊ����ʾ��
    if (s.empty()) { return false; }

    // �õ���һ�����к���
    int dir = ::GetAsyncKeyState(VK_SHIFT) & 0x8000 ? -1 : 1;
    int next_line = dir == 1 ? line + 1 : line - 1;

    // �����Ƿ���Ч
    bool valid = false;

    // ������һ�У�-1��ȫ��
    int fltcol = (int)_cbo_filter->get_cur_data();

    // ������ƥ�������
    for (auto& b : _last_search_matched_cols)
        b = false;

    // ��ת����Сд������
    std::wstring needle = s;
    std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);

    // ��������
    auto search_text = [&](std::wstring /* ������ */heystack) {
        // ͬ��ת����Сд��������
        std::transform(heystack.begin(), heystack.end(), heystack.begin(), ::tolower);

        return ::wcsstr(heystack.c_str(), needle.c_str()) != nullptr;
    };

    for (; next_line >= 0 && next_line < (int)_current_filter->size();) {
        auto& evt = *(*_current_filter)[next_line];

        // ����ÿһ��
        if (fltcol == -1) {
            // �Ƿ�������һ���Ѿ��ܹ�ƥ��
            bool has_col_match = false;

            // һ����ƥ������
            for (int i = 0; i < LogDataUI::data_cols; i++) {
                if(search_text(evt[i])) {
                    _last_search_matched_cols[i] = true;
                    has_col_match = true;
                }
            }

            valid = has_col_match;
        }
        // ������ָ����
        else {
            if (search_text(evt[fltcol])) {
                _last_search_matched_cols[fltcol] = true;
                valid = true;
            }
        }

        if (valid) break;

        // û�ҵ���������������
        next_line += dir == 1 ? 1 : -1;
    }

    if (!valid) {
        msgbox(std::wstring(L"û��") + (dir==1 ? L"��" : L"��") + L"һ���ˡ�", MB_ICONINFORMATION);
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
    // ��Ҫ�ȹر���������־��¼��ĳĳЩ����Ϊ��ǰ����־��¼û�����ü������ܣ�

    // �������鿴���鴰��
    // ����Ŀǰ�����޸��¼���¼������ʼ��ʹ�ã������Ȳ�������

    // ���¼�������Ӧ������ˣ�����ֻ�����ã�
    for (auto& f : _filters)
        f->clear();

    // ���¼�ӵ����־�¼�������ɾ��
    // ���� LogDataUIPtr �ľ���ʵ���������Ƿ�ִ�� delete
    // ����� raw pointer ����Ҫ�ֶ�ɾ��
    // ����� smart pointer �򲻱�Ҫ
    // for (auto& evt : _events.events())
    //    delete evt;

    _events.clear();

    // ���½���
    _listview->set_item_count(0, 0);
}

void MainWindow::_set_top_most(bool top)
{
    _btn_topmost->set_text(top ? L"ȡ���ö�" : L"�����ö�");
    ::SetWindowPos(_hwnd, top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

// ������������
void MainWindow::_update_main_filter()
{
    // ������ǰѡ�е������еĻ���
    Column* cur = nullptr;
    if (_cbo_filter->get_cur_sel() != -1) {
        void* ud = _cbo_filter->get_cur_data();
        if ((int)ud != -1) {
            cur = &_columns[(int)ud];
        }
    }

    // ��������
    _cbo_filter->reset_content();
    _cbo_filter->add_string(L"ȫ��", (void*)-1);

    std::vector<const wchar_t*> strs {L"ȫ��"};

    // ֻ�����Ѿ���ʾ����
    int new_cur = 0;
    for (size_t i = 0, j = 0; i < _columns.size(); i++) {
        auto& col = _columns[i];
        if (col.show) {
            _cbo_filter->add_string(col.name.c_str(), (void*)i);
            strs.push_back(col.name.c_str());
            j++;
            if (cur == &col) {
                new_cur = j;
            }
        }
    }

    // ����ѡ��ԭ������
    _cbo_filter->set_cur_sel(new_cur);

    _cbo_filter->adjust_droplist_width(strs);
}

void MainWindow::_update_filter_list(EventContainer* p)
{
    // ������ǰѡ�е������еĻ���
    EventContainer* cur = p;
    if(!p && _cbo_sel_flt->get_cur_sel() != -1) {
        void* ud = _cbo_sel_flt->get_cur_data();
        cur = (EventContainer*)ud;
    }

    // ��������
    _cbo_sel_flt->reset_content();
    _cbo_sel_flt->add_string(L"ȫ��", &_events);

    std::vector<const wchar_t*> strs {L"ȫ��"};
    int new_cur = 0;
    for(size_t i = 0, j = 0; i < _filters.size(); i++) {
        auto flt = _filters[i];
        _cbo_sel_flt->add_string(flt->name.c_str(), (void*)flt);
        strs.push_back(flt->name.c_str());
        j++;
        if(cur == flt) {
            new_cur = j;
        }
    }

    // ����ѡ��ԭ������
    _cbo_sel_flt->set_cur_sel(new_cur);
    _cbo_sel_flt->adjust_droplist_width(strs);
}

void MainWindow::_export2file()
{
    auto escape = [](const wchar_t* s) {
        std::wstring r(s);

        r = std::regex_replace(r, std::wregex(L"&"), L"&amp;");
        r = std::regex_replace(r, std::wregex(L"<"), L"&lt;");
        r = std::regex_replace(r, std::wregex(L">"), L"&gt;");

        return r;
    };

    std::wstringstream s;

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
        s << L"<tr>";

        for(int i = 0; i < log->data_cols; i++) {
            auto p = (*log)[i];
            s << L"<td>" << (log->should_escape[i] ? escape(p).c_str() : p) << L"</td>";
        }

        s << L"</tr>\n";
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
        msgbox(L"�ѱ��浽 export.html��");
    }
    else {
        msgbox(L"û����ȷ������");
    }
}

void MainWindow::_set_current_filter(EventContainer* p)
{
    bool eq = _current_filter == p;

    _current_filter = p ? p : &_events;

    if(!eq) {
        _listview->set_item_count(_current_filter->size(), 0);
        _listview->redraw_items(0, _listview->get_item_count() -1);
    }
}

void MainWindow::_copy_selected_item()
{
    int i = _listview->get_next_item(-1, LVNI_SELECTED);
    if(i != -1) {
        // TODO ���������鿴��ϸʱ�Ĵ������ظ���
        auto get_column_name = [&](int i) {
            return i >= 0 && i <= (int)_columns.size()
                ? _columns[i].name.c_str()
                : L"";
        };

        auto& log = (*_current_filter)[i];
        auto strlog = log->to_string(get_column_name);

        utils::set_clipboard_text(strlog);
    }
}

void MainWindow::_save_filters()
{
    auto& filters = g_config->arr("filters").as_arr();

    filters.clear();

    for(auto& f : _filters) {
        filters.push_back(*f);
    }
}

LRESULT MainWindow::_on_create()
{
    _btn_start          = _root->find<taowin::button>(L"start-logging");
    _btn_clear          = _root->find<taowin::button>(L"clear-logging");
    _btn_modules        = _root->find<taowin::button>(L"module-manager");
    _btn_filter         = _root->find<taowin::button>(L"filter-result");
    _btn_topmost        = _root->find<taowin::button>(L"topmost");
    _edt_search         = _root->find<taowin::edit>(L"s");
    _cbo_filter         = _root->find<taowin::combobox>(L"s-filter");
    _btn_colors         = _root->find<taowin::button>(L"color-settings");
    _btn_export2file    = _root->find<taowin::button>(L"export-to-file");
    _cbo_sel_flt        = _root->find<taowin::combobox>(L"select-filter");

    _accels = ::LoadAccelerators(nullptr, MAKEINTRESOURCE(IDR_ACCELERATOR_MAINWINDOW));

    _init_config();

    _init_listview();

    _init_filters();
    _init_filter_events();

    _current_filter = &_events;
    
    _level_maps.try_emplace(TRACE_LEVEL_VERBOSE,     L"Verbose",      L"��ϸ - TRACE_LEVEL_VERBOSE" );
    _level_maps.try_emplace(TRACE_LEVEL_INFORMATION, L"Information",  L"��Ϣ - TRACE_LEVEL_INFORMATION");
    _level_maps.try_emplace(TRACE_LEVEL_WARNING,     L"Warning",      L"���� - TRACE_LEVEL_WARNING" );
    _level_maps.try_emplace(TRACE_LEVEL_ERROR,       L"Error",        L"���� - TRACE_LEVEL_ERROR" );
    _level_maps.try_emplace(TRACE_LEVEL_CRITICAL,    L"Critical",     L"���� - TRACE_LEVEL_CRITICAL" );

    _update_main_filter();
    _update_filter_list(nullptr);

    return 0;
}

LRESULT MainWindow::_on_close()
{
    _save_filters();

    _stop();

    DestroyWindow(_hwnd);

    return 0;
}

LRESULT MainWindow::_on_log(LoggerMessage::Value msg, LPARAM lParam)
{
    if(msg == LoggerMessage::LogMsg) {
        auto logdata = reinterpret_cast<LogData*>(lParam);
        auto logui = LogDataUI::from_logdata(logdata, _log_pool.alloc());

        LogDataUIPtr item(logui, [&](LogDataUI* p) {
            p->~LogDataUI();
            _log_pool.destroy(p);
        });

        // ��־���
        _snwprintf(item->id, _countof(item->id), L"%llu", (unsigned long long)_events.size() + 1);

        // ��Ŀ���� & ��Ŀ��Ŀ¼
        const std::wstring* root = nullptr;
        _module_from_guid(item->guid, &item->strProject, &root);

        // ���·��
        item->offset_of_file = 0;
        if(*item->file && root) {
            if(::_wcsnicmp(item->file, root->c_str(), root->size()) == 0) {
                item->offset_of_file = (int)root->size();
            }
        }

        // �ַ�����ʽ����־�ȼ�
        item->strLevel = &_level_maps[item->level].cmt1;

        // ȫ���¼�����
        _events.add(item);

        // �ж�һ�µ�ǰ�������Ƿ������˴��¼�
        // ���û�����ӣ��Ͳ���Ҫˢ���б��ؼ���
        bool added_to_current = _current_filter == &_events;

        // �����˵��¼�������ָ�븴�ã�
        if(!_filters.empty()) {
            for(auto& f : _filters) {
                bool added = f->add(item);
                if(f == _current_filter && added && !added_to_current)
                    added_to_current = true;
            }
        }

        if(added_to_current) {
            // Ĭ���Ƿ��Զ����������һ�е�
            // �������ǰ�����������һ�У����Զ�����
            int count = (int)_current_filter->size();
            int sic_flag = LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL;
            bool is_last_focused = count > 1 && (_listview->get_item_state(count - 2, LVIS_FOCUSED) & LVIS_FOCUSED)
                || _listview->get_next_item(-1, LVIS_FOCUSED) == -1;

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
    auto& evt = *(*_current_filter)[pdi->item.iItem];
    auto lit = &pdi->item;

    int listview_col = lit->iSubItem;
    int event_col = _columns[listview_col].log_index;
    auto field = evt[event_col];

    lit->pszText = const_cast<LPWSTR>(field);

    return 0;
}

LRESULT MainWindow::_on_select_column()
{
    auto colsel = new ColumnSelection(_columns);

    colsel->OnToggle([&](int i) {
        auto& col = _columns[i];
        _listview->set_column_width(i, col.show ? col.width : 0);

        auto& columns = _config.arr("columns").as_arr();
        JsonWrapper(columns[i]).as_obj()["show"] = col.show;
    });

    colsel->domodal(this);

    _update_main_filter();

    return 0;
}

LRESULT MainWindow::_on_drag_column(NMHDR* hdr)
{
    auto nmhdr = (NMHEADER*)hdr;
    auto& item = nmhdr->pitem;
    auto& col = _columns[nmhdr->iItem];

    col.show = item->cxy != 0;
    if (item->cxy) col.width = item->cxy;

    auto& columns = _config.arr("columns").as_arr();
    auto& colobj = JsonWrapper(columns[nmhdr->iItem]).as_obj();
    colobj["show"] = col.show;
    colobj["width"] = col.width;

    _update_main_filter();

    return 0;
}

// �϶��б�ͷ���ס��ͷ�е�˳��
void MainWindow::_on_drop_column()
{
    // ֻ�ǵ�ǰ��������������Ϊ��Щ�����Ѿ�ɾ����
    // ���������Ȼ�� columns �� show(visible) ������һ��
    auto n = _listview->get_column_count();
    auto orders = std::make_unique<int[]>(n);
    if(_listview->get_column_order(n, orders.get())) {
        json11::Json::array order;

        order.reserve(n);

        for(int i = 0; i < n; i++) {
            order.push_back(orders.get()[i]);
        }

        auto& order_config = _config.obj("listview").arr("column-order").as_arr();
        order_config = std::move(order);
    }
}

void MainWindow::_module_from_guid(const GUID & guid, std::wstring * name, const std::wstring ** root)
{
    if (!_guid2mod.count(guid)) {
        for (auto& item : _modules) {
            if (::IsEqualGUID(item->guid, guid)) {
                _guid2mod[guid] = item;
                break;
            }
        }
    }

    auto it = _guid2mod.find(guid);

    if (it != _guid2mod.cend()) {
        *name = it->second->name;
        *root = &it->second->root;
    }
    else {
        *name = L"<unknown>";
        *root = nullptr;
    }
}

}
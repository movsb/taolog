
#include <cassert>

#include <string>
#include <unordered_map>
#include <algorithm>

#include <windows.h>

#include "_logdata_define.hpp"

#include <taowin/src/tw_taowin.h>

#include "main_window.h"
#include "config.h"

namespace taoetw {

static HWND g_logger_hwnd;
static UINT g_logger_message;

void DoEtwLog(LogDataUI* log)
{
    ::PostMessage(g_logger_hwnd, g_logger_message, 0, LPARAM(log));
}

LPCTSTR MainWindow::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="ETW Log Viewer" size="820,600">
    <res>
        <font name="default" face="微软雅黑" size="12"/>
        <font name="12" face="微软雅黑" size="12"/>
    </res>
    <root>
        <vertical padding="5,5,5,5">
            <horizontal name="toolbar" height="30" padding="0,1,0,4">
                <button name="start-logging" text="开始记录" width="60" style="tabstop"/>
                <control width="5" />
                <button name="stop-logging" text="停止记录" width="60" style="disabled,tabstop"/>
                <control width="5" />
                <button name="clear-logging" text="清空记录" width="60" style="tabstop"/>
                <control width="5" />
                <button name="module-manager" text="模块管理" width="60" style="tabstop"/>
                <control width="5" />
                <button name="filter-result" text="结果过滤" width="60" style="tabstop"/>
                <control width="5" />
                <control />
                <label text="查找：" width="38" style="centerimage"/>
                <combobox name="s-filter" style="tabstop" height="400" width="64" padding="0,0,4,0"/>
                <edit name="s" width="80" style="tabstop" exstyle="clientedge"/>
                <control width="5" />
                <button name="topmost" text="窗口置顶" width="60" style="tabstop"/>
            </horizontal>
            <listview name="lv" style="showselalways,ownerdata,tabstop" exstyle="clientedge"/>
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
    case kDoLog:    return _on_log((LogDataUI*)lparam);
    }
    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT MainWindow::on_menu(int id, bool is_accel)
{
    /*
    if (id < (int)_menus.subs.size()) {
        _menus.subs[id].onclick();
    }
    */
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
            else if (nmlv->wVKey == L'F') {
                if (::GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                    _edt_search->focus();
                    _edt_search->set_sel(0, -1);
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
        if (_start()) {
            _btn_start->set_enabled(false);
            _btn_stop->set_enabled(true);
        }
    }
    else if (pc == _btn_stop) {
        _stop();
        _btn_start->set_enabled(true);
        _btn_stop->set_enabled(false);
    }
    else if (pc == _btn_modules) {
        _manage_modules();
    }
    else if (pc == _btn_filter) {
        _show_filters();
    }
    else if (pc == _btn_topmost) {
        bool totop = !(::GetWindowLongPtr(_hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST);
        _set_top_most(totop);
        _config["topmost"] = totop;
    }
    else if (pc == _btn_clear) {
        _clear_results();
    }
    else if (pc == _cbo_filter) {
        if (code == CBN_SELCHANGE) {
            _edt_search->set_sel(0, -1);
            _edt_search->focus();
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
            if (!_do_search(_last_search_string, _last_search_line, -1))
                _edt_search->focus();
            return true;
        }
    }

    return __super::filter_special_key(vk);
}

bool MainWindow::_start()
{
    std::wstring session;
    auto& etwobj = _config.obj("etw").as_obj();
    auto it= etwobj.find("session");
    if(it == etwobj.cend() || it->second.string_value().empty()) {
        session = L"taoetw-session";
        // 默认值，可以不用写进去，如有必须手动加上
        // etwobj["session"] = g_config.us(session);
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
        msgbox(L"没有模块启用记录。", MB_ICONEXCLAMATION);
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

    // 表头栏
    if(_config.has_arr("columns")) {
        for(auto& jsoncol : _config.arr("columns").as_arr()) {
            auto& c = JsonWrapper(jsoncol).as_obj();
            _columns.emplace_back(g_config.ws(c["name"].string_value()).c_str(),
                c["show"].bool_value(), c["width"].int_value());
        }
    }
    else {
        _columns.emplace_back(L"编号", false, 50);
        _columns.emplace_back(L"时间", true, 86);
        _columns.emplace_back(L"进程", false, 50);
        _columns.emplace_back(L"线程", false, 50);
        _columns.emplace_back(L"项目", true, 100);
        _columns.emplace_back(L"文件", true, 140);
        _columns.emplace_back(L"函数", true, 100);
        _columns.emplace_back(L"行号", true, 50);
        _columns.emplace_back(L"等级", false, 100);
        _columns.emplace_back(L"日志", true, 300);

        // 初次使用时初始化配置文件
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

    // 列表颜色栏
    JsonWrapper config_listview = _config.obj("listview");
    if(config_listview.has_arr("colors")) {
        for(auto& color : config_listview.arr("colors").as_arr()) {
            auto& co = JsonWrapper(color).as_obj();

            int level = co["level"].int_value();

            unsigned int fgc[3], bgc[3];
            ::sscanf(co["fgc"].string_value().c_str(), "%d,%d,%d", &fgc[0], &fgc[1], &fgc[2]);
            ::sscanf(co["bgc"].string_value().c_str(), "%d,%d,%d", &bgc[0], &bgc[1], &bgc[2]);

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
}

void MainWindow::_init_config()
{
    std::string err;

    // the gui main
    auto windows = g_config->obj("windows");

    // main window config
    _config = windows.obj("main");

    _set_top_most(_config["topmost"].bool_value());

    // the modules
    auto modules = g_config->arr("modules");
    for(auto& mod : modules.as_arr()) {
        if(mod.is_object()) {
            auto name = mod["name"];
            auto root = mod["root"];
            auto enable = mod["enable"];
            auto level = mod["level"];
            auto guidstr = mod["guid"];
            GUID guid = {};

            if((name.is_string() && root.is_string() && enable.is_bool() && level.is_number() && guidstr.is_string())
                && (level >= TRACE_LEVEL_CRITICAL && level <= TRACE_LEVEL_VERBOSE)
                && (!FAILED(::CLSIDFromString(g_config.ws(guidstr.string_value()).c_str(), &guid)))
            )
            {
                auto m = new ModuleEntry;
                m->name = g_config.ws(name.string_value());
                m->root = g_config.ws(root.string_value());
                m->enable = enable.bool_value();
                m->level = (unsigned char)level.int_value();
                m->guid = guid;
                m->guid_str = g_config.ws(guidstr.string_value());

                _modules.push_back(m);
            }
            else {
                msgbox(L"无效模块配置。", MB_ICONERROR);
            }
        }
    }
}

void MainWindow::_view_detail(int i)
{
    auto evt = (*_current_filter)[i];
    auto& cr = _colors[evt->level];
    auto detail_window = new EventDetail(evt, cr.fg, cr.bg);
    detail_window->create();
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
    if (ResultFilter::_this_instance) {
        auto that = ResultFilter::_this_instance;
        if (::IsIconic(*that)) ::ShowWindow(*that, SW_RESTORE);
        ::SetActiveWindow(*that);
        ::SetFocus(*that);
        return;
    }

    auto get_base = [&](std::vector<std::wstring>* bases) {
        for (auto& col : _columns) {
            bases->push_back(col.name);
        }
    };

    auto ondelete = [&](int i) {
        auto it = _filters.begin() + i;

        if (*it == _current_filter) {
            _current_filter = &_events;
            _listview->set_item_count(_current_filter->size(), 0);
            _listview->redraw_items(0, _listview->get_item_count() -1);
        }

        delete *it;
        _filters.erase(it);
    };

    auto onaddnew = [&](EventContainer* p) {
        _filters.push_back(p);
        _events.filter_results(p);
    };

    auto onsetfilter = [&](EventContainer* p) {
        _current_filter = p ? p : &_events;
        _listview->set_item_count(_current_filter->size(), 0);
        _listview->redraw_items(0, _listview->get_item_count() -1);
    };

    auto ongetvalues = [&](int baseindex, std::unordered_map<int, const wchar_t*>* values) {
        values->clear();

        // TODO: 警告：修改列的时候注意这里
        if (baseindex == 8) {

            for (auto& pair : _level_maps) {
                (*values)[pair.first] = pair.second.cmt2.c_str();
            }
        }
    };

    auto dlg = new ResultFilter(_filters, get_base, ondelete, onsetfilter, onaddnew, _current_filter, ongetvalues);
    dlg->create();
    dlg->show();
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
    int fltcol = (int)_cbo_filter->get_cur_data();

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
            for (int i = 0; i < LogDataUI::data_cols; i++) {
                if(search_text(evt[i])) {
                    _last_search_matched_cols[i] = true;
                    has_col_match = true;
                }
            }

            valid = has_col_match;
        }
        // 仅搜索指定列
        else {
            if (search_text(evt[fltcol])) {
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
    for (auto& f : _filters)
        f->clear();

    // 主事件拥有日志事件，由它删除
    // for (auto& evt : _events.events())
    //    delete evt;

    _events.clear();

    // 更新界面
    _listview->set_item_count(0, 0);
}

void MainWindow::_set_top_most(bool top)
{
    _btn_topmost->set_text(top ? L"取消置顶" : L"窗口置顶");
    ::SetWindowPos(_hwnd, top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

// 主界面搜索栏
void MainWindow::_update_main_filter()
{
    // 保留当前选中的项（如果有的话）
    Column* cur = nullptr;
    if (_cbo_filter->get_cur_sel() != -1) {
        void* ud = _cbo_filter->get_cur_data();
        if ((int)ud != -1) {
            cur = &_columns[(int)ud];
        }
    }

    // 重置内容
    _cbo_filter->reset_content();
    _cbo_filter->add_string(L"全部", (void*)-1);

    // 只添加已经显示的列
    int new_cur = 0;
    for (size_t i = 0, j = 0; i < _columns.size(); i++) {
        auto& col = _columns[i];
        if (col.show) {
            _cbo_filter->add_string(col.name.c_str(), (void*)i);
            j++;
            if (cur == &col) {
                new_cur = j;
            }
        }
    }

    // 保持选中原来的项
    _cbo_filter->set_cur_sel(new_cur);
}

LRESULT MainWindow::_on_create()
{
    _btn_start      = _root->find<taowin::button>(L"start-logging");
    _btn_stop       = _root->find<taowin::button>(L"stop-logging");
    _btn_clear      = _root->find<taowin::button>(L"clear-logging");
    _btn_modules    = _root->find<taowin::button>(L"module-manager");
    _btn_filter     = _root->find<taowin::button>(L"filter-result");
    _btn_topmost    = _root->find<taowin::button>(L"topmost");
    _edt_search     = _root->find<taowin::edit>(L"s");
    _cbo_filter     = _root->find<taowin::combobox>(L"s-filter");

    _init_config();

    _init_listview();

    _current_filter = &_events;
    
    _level_maps.try_emplace(TRACE_LEVEL_VERBOSE,     L"Verbose",      L"详细 - TRACE_LEVEL_VERBOSE" );
    _level_maps.try_emplace(TRACE_LEVEL_INFORMATION, L"Information",  L"信息 - TRACE_LEVEL_INFORMATION");
    _level_maps.try_emplace(TRACE_LEVEL_WARNING,     L"Warning",      L"警告 - TRACE_LEVEL_WARNING" );
    _level_maps.try_emplace(TRACE_LEVEL_ERROR,       L"Error",        L"错误 - TRACE_LEVEL_ERROR" );
    _level_maps.try_emplace(TRACE_LEVEL_CRITICAL,    L"Critical",     L"严重 - TRACE_LEVEL_CRITICAL" );

    _update_main_filter();

    return 0;
}

LRESULT MainWindow::_on_close()
{
    DestroyWindow(_hwnd);

    return 0;
}

LRESULT MainWindow::_on_log(LogDataUI* pItem)
{
    LogDataUIPtr item(pItem);

    const std::wstring* root = nullptr;

    _snwprintf(item->id, _countof(item->id), L"%llu", (unsigned long long)_events.size()+1);

    _module_from_guid(item->guid, &item->strProject, &root);

    // 相对路径
    if (*item->file && root) {
        if (::_wcsnicmp(item->file, root->c_str(), root->size()) == 0) {
            item->offset_of_file = (int)root->size();
        }
    }
    else {
        item->offset_of_file = 0;
    }

    item->strLevel = &_level_maps[item->level].cmt1;

    // 全部事件容器
    _events.add(item);

    // 带过滤的事件容器（指针复用）
    if (!_filters.empty()) {
        for (auto& f : _filters) {
            f->add(item);
        }
    }

    _listview->set_item_count(_current_filter->size(), LVSICF_NOINVALIDATEALL|LVSICF_NOSCROLL);

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

    lit->pszText = const_cast<LPWSTR>(evt[lit->iSubItem]);

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

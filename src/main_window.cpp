
#include <cassert>

#include <string>
#include <unordered_map>

#include <windows.h>
#include <evntrace.h>

#include "etwlogger.h"

#include <taowin/src/tw_taowin.h>

#include "main_window.h"

static const wchar_t* g_etw_session = L"taoetw-session";

namespace taoetw {

Consumer* g_Consumer;

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
            <horizontal height="30" padding="0,4,0,4">
                <button name="start-logging" text="开始记录" width="60" />
                <control width="5" />
                <button name="stop-logging" text="停止记录" width="60" style="disabled"/>
                <control width="5" />
                <button name="module-manager" text="模块管理" width="60" />
                <control width="5" />
                <control width="5" />
                <button name="topmost" text="窗口置顶" width="60" />
            </horizontal>
            <listview name="lv" style="singlesel,showselalways,ownerdata" exstyle="clientedge">  </listview>
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
    case kDoLog:    return _on_log((ETWLogger::LogDataUI*)lparam);
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
    else if (pc == _btn_topmost) {
        bool totop = !(::GetWindowLongPtr(_hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST);
        _btn_topmost->set_text(totop ? L"取消置顶" : L"窗口置顶");
        ::SetWindowPos(_hwnd, totop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }

    return 0;
}

bool MainWindow::_start()
{
    if (!_controller.start(g_etw_session)) {
        msgbox(taowin::last_error(), MB_ICONERROR);
        return false;
    }

    _consumer.init(_hwnd, kDoLog);

    if (!_consumer.start(g_etw_session)) {
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

    _columns.emplace_back(L"时间", true, 80);
    _columns.emplace_back(L"进程", true, 50);
    _columns.emplace_back(L"线程", true, 50);
    _columns.emplace_back(L"项目", true, 100);
    _columns.emplace_back(L"文件", true, 200);
    _columns.emplace_back(L"函数", true, 100);
    _columns.emplace_back(L"行号", true, 50);
    _columns.emplace_back(L"日志", true, 300);

    for (int i = 0; i < (int)_columns.size(); i++) {
        auto& col = _columns[i];
        _listview->insert_column(col.name.c_str(), col.width, i);
    }

    _colors.try_emplace(TRACE_LEVEL_INFORMATION, RGB(  0,   0,   0), RGB(255, 255, 255));
    _colors.try_emplace(TRACE_LEVEL_WARNING,     RGB(255, 128,   0), RGB(255, 255, 255));
    _colors.try_emplace(TRACE_LEVEL_ERROR,       RGB(255,   0,   0), RGB(255, 255, 255));
    _colors.try_emplace(TRACE_LEVEL_CRITICAL,    RGB(255, 255, 255), RGB(255,   0,   0));
    _colors.try_emplace(TRACE_LEVEL_VERBOSE,     RGB(  0,   0,   0), RGB(255, 255, 255));
}

void MainWindow::_init_menu()
{
    /*
    MenuEntry menu;

    menu.enable = true;
    menu.name = L"开始记录";
    menu.onclick = [&]() {
        _start();

    };
    _menus[L"start_logging"] = menu;

    menu.enable = true;
    menu.name = L"停止记录";
    menu.onclick = [&]() {
        _stop();
    };
    _menus[L"stop_logging"] = menu;

    menu.enable = true;
    menu.name = L"模块管理";
    menu.onclick = [&]() {
        (new ModuleManager(_modules))->domodal(this);
    };
    _menus[L"module_manager"] = menu;

    HMENU hMenu = ::CreateMenu();
    _menus

    int i = 0;
    for (auto it = _menus.begin(), end = _menus.end(); it != end; ++it, ++i) {
        auto& menu = it->second;
        menu.id = i;
        ::AppendMenu(hMenu, MF_STRING, i, _menus[i].name.c_str());
        ::EnableMenuItem(hMenu, i, MF_BYCOMMAND | (_menus[i].enable ? MF_ENABLED : MF_GRAYED | MF_DISABLED));
    }

    ::SetMenu(_hwnd, hMenu);
    */
}

void MainWindow::_view_detail(int i)
{
    auto evt = _events[i];
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

    auto mgr = new ModuleManager(_modules);
    mgr->on_toggle_enable(on_toggle_enable);
    mgr->domodal(this);
}

LRESULT MainWindow::_on_create()
{
    _btn_start      = _root->find<taowin::button>(L"start-logging");
    _btn_stop       = _root->find<taowin::button>(L"stop-logging");
    _btn_modules    = _root->find<taowin::button>(L"module-manager");
    _btn_topmost    = _root->find<taowin::button>(L"topmost");

    _init_listview();
    _init_menu();

    assert(g_Consumer == nullptr);
    g_Consumer = &_consumer;

    GUID guids[] = {
        { 0x1ca17b9b, 0xe3f4, 0x4fa4, { 0x91, 0xa3, 0xd0, 0x39, 0xa6, 0x26, 0x1f, 0x88 } },
        { 0xb2a2be3c, 0x90f2, 0x4778, { 0x85, 0x72, 0x48, 0x47, 0x9e, 0x8e, 0x0b, 0x2e } },
        { 0xd3fcccc4, 0xeb1c, 0x441e, { 0x97, 0xe4, 0x06, 0x1b, 0x24, 0xb1, 0x7f, 0x26 } },
        { 0xfa5f6043, 0xa918, 0x4de4, { 0x80, 0xbd, 0x15, 0x62, 0x54, 0x7f, 0x1b, 0xa4 } },
        { 0x630514b5, 0x7b96, 0x4b74, { 0x9d, 0xb6, 0x66, 0xbd, 0x62, 0x1f, 0x93, 0x86 } }, // test
    };

    int i = 0;
    for(auto& guid : guids)
        _modules.push_back(new ModuleEntry{ std::to_wstring(i++),L"",i % 2 == 0,0, guid });

    return 0;
}

LRESULT MainWindow::_on_log(ETWLogger::LogDataUI* item)
{
    const std::wstring* root = nullptr;

    _module_from_guid(item->guid, &item->strProject, &root);

    // 相对路径
    if (*item->file && root) {
        if (::wcsnicmp(item->file, root->c_str(), root->size()) == 0) {
            item->offset_of_file = (int)root->size();
        }
    }
    else {
        item->offset_of_file = 0;
    }

    _events.push_back(item);
    _listview->set_item_count(_events.size(), LVSICF_NOINVALIDATEALL);

    return 0;
}

LRESULT MainWindow::_on_custom_draw_listview(NMHDR * hdr)
{
    LRESULT lr = CDRF_DODEFAULT;
    ETWLogger::LogDataUI* log;

    auto lvcd = (LPNMLVCUSTOMDRAW)hdr;

    switch (lvcd->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        lr = CDRF_NOTIFYITEMDRAW;
        break;

    case CDDS_ITEMPREPAINT:
        log = _events[lvcd->nmcd.dwItemSpec];
        lvcd->clrText = _colors[log->level].fg;
        lvcd->clrTextBk = _colors[log->level].bg;
        lr = CDRF_NEWFONT;
        break;
    }

    return lr;
}

LRESULT MainWindow::_on_get_dispinfo(NMHDR * hdr)
{
    auto pdi = reinterpret_cast<NMLVDISPINFO*>(hdr);
    auto evt = _events[pdi->item.iItem];
    auto lit = &pdi->item;

    const TCHAR* value = _T("");

    switch (lit->iSubItem)
    {
    case 0: value = evt->strTime.c_str();               break;
    case 1: value = evt->strPid.c_str();                break;
    case 2: value = evt->strTid.c_str();                break;
    case 3: value = evt->strProject.c_str();            break;
    case 4: value = evt->file + evt->offset_of_file;    break;
    case 5: value = evt->func;                          break;
    case 6: value = evt->strLine.c_str();               break;
    case 7: value = evt->text;                          break;
    }

    lit->pszText = const_cast<TCHAR*>(value);

    return 0;
}

LRESULT MainWindow::_on_select_column()
{
    auto colsel = new ColumnSelection(_columns);

    colsel->OnToggle([&](int i) {
        auto& col = _columns[i];
        _listview->set_column_width(i, col.show ? col.width : 0);
    });

    colsel->domodal(this);

    return 0;
}

LRESULT MainWindow::_on_drag_column(NMHDR* hdr)
{
    auto nmhdr = (NMHEADER*)hdr;
    auto& item = nmhdr->pitem;
    auto& col = _columns[nmhdr->iItem];

    col.show = item->cxy != 0;
    if (item->cxy) col.width = item->cxy;

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

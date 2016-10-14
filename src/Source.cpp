
#include <cassert>

#include <string>
#include <iostream>
#include <thread>
#include <unordered_map>

#include <windows.h>
#include <process.h>
#include <evntrace.h>

#include "etwlogger.h"

#include <taowin/src/tw_taowin.h>

#include "consumer.h"

static HWND g_hWnd;
static const UINT WM_DO_LOG = WM_USER + 0;

static const wchar_t* g_etw_session = L"taoetw-session";

// This GUID defines the event trace class. 	
// {6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}
static const GUID clsGuid = 
{ 0x6e5e5cbc, 0x8acf, 0x4fa6, { 0x98, 0xe4, 0xc, 0x63, 0xa0, 0x75, 0x32, 0x3b } };

namespace taoetw {

Consumer* g_Consumer;

}


class Controller
{
public:
    Controller()
        : _handle(0)
        , _props(nullptr)
    { }

    ~Controller()
    {
        stop();
    }

public:
    void start(const wchar_t* session);
    void enable(const GUID& provider, bool enable_, const char* level);
    void stop();

protected:
    TRACEHANDLE             _handle;
    EVENT_TRACE_PROPERTIES* _props;
    std::wstring            _name;
};

void Controller::start(const wchar_t* session)
{
    _name = session;

    size_t size = sizeof(EVENT_TRACE_PROPERTIES) + (_name.size() + 1)*sizeof(TCHAR);
    _props = (EVENT_TRACE_PROPERTIES*)new char[size]();

restart:
    _props->Wnode.BufferSize = size;
    _props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    _props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    _props->LogFileNameOffset = 0;

    _props->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    _props->LogFileNameOffset = 0;
    _props->FlushTimer = 1;

    wcscpy((wchar_t*)_props + _props->LoggerNameOffset, session);

    if(::StartTrace(&_handle, _name.c_str(), _props) == ERROR_ALREADY_EXISTS) {
        ::ControlTrace(0, _name.c_str(), _props, EVENT_TRACE_CONTROL_STOP);
        goto restart;
    }
}

void Controller::enable(const GUID& provider, bool enable_, const char* level)
{
    ::EnableTrace(enable_, 0, TRACE_LEVEL_INFORMATION, &provider, _handle);
}

void Controller::stop()
{
    ::ControlTrace(0, _name.c_str(), _props, EVENT_TRACE_CONTROL_STOP);
    delete[] _props;
}

//////////////////////////////////////////////////////////////////////////

struct MenuEntry
{
    taowin::string name;
    std::function<void()> onclick;
};

struct ColumnData
{
    struct Column {
        taowin::string name;
        bool show;
        int width;
    };

    std::vector<Column> cols;
};

struct ModuleEntry
{
    std::wstring    name;
    std::wstring    root;
    bool            enable;
    unsigned char   level;
    GUID            guid;
};

class ColumnSelection : public taowin::window_creator
{
private:
    typedef std::function<void(int i)> OnToggleCallback;

    ColumnData& _data;
    OnToggleCallback _on_toggle;

public:
    ColumnSelection(ColumnData& data)
        : _data(data)
    {

    }

    void OnToggle(OnToggleCallback fn)
    {
        _on_toggle = fn;
    }

protected:
    virtual LPCTSTR get_skin_xml() const override
    {
        LPCTSTR json = LR"tw(
<window title="表头" size="512,480">
    <res>
        <font name="default" face="微软雅黑" size="12"/>
        <font name="1" face="微软雅黑" size="12"/>
        <font name="consolas" face="Consolas" size="12"/>
    </res>
    <root>
        <vertical name="container" padding="20,20,20,20">

        </vertical>
    </root>
</window>
)tw";
        return json;
    }

    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override
    {
        switch(umsg) {
        case WM_CREATE:
        {
            auto container = _root->find<taowin::vertical>(L"container");

            for(int i = 0; i < (int)_data.cols.size(); i++) {
                auto check = new taowin::check;
                std::map<taowin::string, taowin::string> attrs;

                attrs[_T("text")] = _data.cols[i].name.c_str();
                attrs[_T("name")] = std::to_wstring(i).c_str();
                attrs[_T("checked")] = _data.cols[i].show ? _T("true") : _T("false");

                check->create(_hwnd, attrs, _mgr);
                ::SetWindowLongPtr(check->hwnd(), GWL_USERDATA, LONG(check));
                container->add(check);
            }

            taowin::Rect rc {0, 0, 200, (int)_data.cols.size() * 20 + 50};
            ::AdjustWindowRectEx(&rc, ::GetWindowLongPtr(_hwnd, GWL_STYLE), !!::GetMenu(_hwnd), ::GetWindowLongPtr(_hwnd, GWL_EXSTYLE));
            ::SetWindowPos(_hwnd, nullptr, 0, 0, rc.width(), rc.height(), SWP_NOMOVE | SWP_NOZORDER);

            return 0;
        }
        }
        return __super::handle_message(umsg, wparam, lparam);
    }

    virtual LRESULT on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr)
    {
        if(!pc) return 0;

        if(pc->name() >= _T("0") && pc->name() < std::to_wstring(_data.cols.size())) {
            if(code == BN_CLICKED) {
                int index = _ttoi(pc->name().c_str());
                auto& col = _data.cols[index];
                col.show = !col.show;
                _on_toggle(index);
                return 0;
            }
        }

        return 0;
    }

    virtual void on_final_message() override
    {
        __super::on_final_message();
        delete this;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////

class ModuleEntryEditor : public taowin::window_creator
{
public:
    typedef std::function<void(ModuleEntry* p)> fnOnOk;
    typedef std::function<bool(const GUID& guid, std::wstring* err)> fnOnCheckGuid;

private:
    ModuleEntry* _mod;

    fnOnOk _onok;
    fnOnCheckGuid _oncheckguid;

    taowin::edit* _name;
    taowin::edit* _guid;
    taowin::edit* _path;
    taowin::combobox* _level;

public:
    ModuleEntryEditor(ModuleEntry* mod, fnOnOk onok, fnOnCheckGuid onguid)
        : _mod(mod)
        , _onok(onok)
        , _oncheckguid(onguid)
    {

    }

protected:
    virtual LPCTSTR get_skin_xml() const override
    {
        LPCTSTR json = LR"tw(
<window title="添加模块" size="350,200">
    <res>
        <font name="default" face="微软雅黑" size="12"/>
        <font name="1" face="微软雅黑" size="12"/>
        <font name="consolas" face="Consolas" size="12"/>
    </res>
    <root>
        <vertical>
            <vertical name="container" padding="10,10,10,10" height="144">
                <horizontal height="30" padding="0,3,0,3">
                    <label style="centerimage" text="名字" width="50"/>
                    <edit name="name" style="tabstop" exstyle="clientedge" style=""/>
                </horizontal>
                <horizontal height="30" padding="0,3,0,3">
                    <label style="centerimage" text="GUID" width="50"/>
                    <edit name="guid" style="tabstop" exstyle="clientedge" />
                </horizontal>
                <horizontal height="30" padding="0,3,0,3">
                    <label style="centerimage" text="目录" width="50"/>
                    <edit name="root" style="tabstop" exstyle="clientedge" />
                </horizontal>
                <horizontal height="30" padding="0,3,0,3">
                    <label style="centerimage" text="等级" width="50"/>
                    <combobox name="level"  height="200"/>
                </horizontal>
            </vertical>
            <horizontal height="40" padding="10,4,10,4">
                <control />
                <button name="ok" text="保存" width="50"/>
                <control width="10" />
                <button name="cancel" text="取消" width="50"/>
            </horizontal>
        </vertical>
    </root>
</window>
)tw";
        return json;
    }

    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override
    {
        switch(umsg) {
        case WM_CREATE:
        {
            ::SetWindowText(_hwnd, !_mod ? L"添加模块" : L"修改模块");

            _name = _root->find<taowin::edit>(L"name");
            _path = _root->find<taowin::edit>(L"root");
            _level= _root->find<taowin::combobox>(L"level");
            _guid = _root->find<taowin::edit>(L"guid");

            std::unordered_map<int, const TCHAR*> levels = {
                {TRACE_LEVEL_INFORMATION,   L"TRACE_LEVEL_INFORMATION"  },
                {TRACE_LEVEL_WARNING,       L"TRACE_LEVEL_WARNING"      },
                {TRACE_LEVEL_ERROR,         L"TRACE_LEVEL_ERROR"        },
                {TRACE_LEVEL_CRITICAL,      L"TRACE_LEVEL_CRITICAL"     },
                {TRACE_LEVEL_VERBOSE,       L"TRACE_LEVEL_VERBOSE"      },
            };

            for(auto& pair : levels) {
                int i = _level->add_string(pair.second);
                _level->set_item_data(i, (void*)pair.first);
            }

            if(!_mod) {
                _level->set_cur_sel(0);
            }
            else {
                _name->set_text(_mod->name.c_str());
                _path->set_text(_mod->root.c_str());

                wchar_t guid[128];
                if(::StringFromGUID2(_mod->guid, &guid[0], _countof(guid))) {
                    _guid->set_text(guid);
                }
                else {
                    _guid->set_text(L"{00000000-0000-0000-0000-000000000000}");
                }

                for(int i = 0, n = _level->get_count();; i++) {
                    if(i < n) {
                        if((int)_level->get_item_data(i) == _mod->level) {
                            _level->set_cur_sel(i);
                            break;
                        }
                    }
                    else {
                        _level->set_cur_sel(0);
                        break;
                    }
                }
            }

            return 0;
        }
        }
        return __super::handle_message(umsg, wparam, lparam);
    }

    virtual LRESULT on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr)
    {
        if(!pc) return 0;

        if(pc->name() == L"ok") {
            if(!_validate_form())
                return 0;

            auto entry = _mod ? _mod : new ModuleEntry;
            entry->name = _name->get_text();
            entry->root = _path->get_text();
            entry->level = (int)_level->get_item_data(_level->get_cur_sel());
            ::CLSIDFromString(_guid->get_text().c_str(), &entry->guid);
            entry->enable = false;

            _onok(entry);

            close();
            return 0;
        }
        else if(pc->name() == L"cancel") {
            close();
            return 0;
        }

        return 0;
    }

    virtual void on_final_message() override
    {
        __super::on_final_message();
        delete this;
    }

protected:
    bool _validate_form()
    {
        if(_name->get_text() == L"") {
            msgbox(L"模块名字不应为空。", MB_ICONERROR);
            return false;
        }

        auto guid = _guid->get_text();
        CLSID clsid;
        if(FAILED(::CLSIDFromString(guid.c_str(), &clsid)) || ::IsEqualGUID(clsid, GUID_NULL)) {
            msgbox(L"无效 GUID 值。", MB_ICONERROR);
            return false;
        }

        std::wstring err;
        if(!_oncheckguid(clsid, &err)) {
            msgbox(err.c_str(), MB_ICONERROR);
            return false;
        }

        return true;
    }
};

//////////////////////////////////////////////////////////////////////////

class ModuleManager : public taowin::window_creator
{
private:
    taowin::listview* _listview;
    std::vector<ModuleEntry*>& _modules;

public:
    ModuleManager(std::vector<ModuleEntry*>& modules)
        : _modules(modules)
    {

    }

protected:
	virtual LPCTSTR get_skin_xml() const override
	{
        LPCTSTR json = LR"tw(
<window title="模块管理" size="300,300">
    <res>
        <font name="default" face="微软雅黑" size="12"/>
        <font name="1" face="微软雅黑" size="12"/>
        <font name="consolas" face="Consolas" size="12"/>
    </res>
    <root>
        <horizontal padding="5,5,5,5">
            <listview name="list" style="showselalways,ownerdata" exstyle="clientedge" />
            <vertical padding="5,5,5,5" width="50">
                <button name="add" text="添加" height="24" />
                <control height="20" />
                <button name="modify" text="修改" style="disabled" height="24"/>
                <control height="5" />
                <button name="enable" text="启用" style="disabled" height="24" />
                <control height="5" />
                <button name="delete" text="删除" style="disabled" height="24"/>
                <control height="5" />
            </vertical>
        </horizontal>
    </root>
</window>
)tw";
		return json;
	}

    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override
    {
        switch(umsg)
        {
        case WM_CREATE:
        {
            _listview = _root->find<taowin::listview>(L"list");

            _listview->insert_column(L"名字", 150, 0);
            _listview->insert_column(L"状态", 50, 1);

            _listview->set_item_count((int)_modules.size(), 0);

            return 0;
        }

        }
        return __super::handle_message(umsg, wparam, lparam);
    }

    virtual LRESULT on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr) override {
        if(!pc) return 0;

        if(pc->name() == _T("list")) {
            if(code == LVN_GETDISPINFO) {
                auto pdi = reinterpret_cast<NMLVDISPINFO*>(hdr);
                auto& mod = _modules[pdi->item.iItem];
                auto lit = &pdi->item;

                const TCHAR* value = _T("");

                switch(lit->iSubItem) {
                case 0: value = mod->name.c_str();                   break;
                case 1: value = mod->enable ? L"已启用" : L"已禁用";  break;
                }

                lit->pszText = const_cast<TCHAR*>(value);

                return 0;
            }
            else if(code == NM_CUSTOMDRAW) {
                LRESULT lr = CDRF_DODEFAULT;
                ModuleEntry* mod;

                auto lvcd = (LPNMLVCUSTOMDRAW)hdr;

                switch(lvcd->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    lr = CDRF_NOTIFYITEMDRAW;
                    break;

                case CDDS_ITEMPREPAINT:
                    mod = _modules[lvcd->nmcd.dwItemSpec];

                    if(mod->enable) {
                        lvcd->clrText = RGB(0, 0, 255);
                        lvcd->clrTextBk = RGB(255, 255, 255);
                    }
                    else {
                        lvcd->clrText = RGB(0, 0, 0);
                        lvcd->clrTextBk = RGB(255, 255, 255);
                    }

                    lr = CDRF_NEWFONT;
                    break;
                }

                return lr;
            }
            else if(code == NM_DBLCLK) {
                auto nmlv = reinterpret_cast<NMITEMACTIVATE*>(hdr);

                if(nmlv->iItem != -1) {
                    _toggle_enable(nmlv->iItem);
                }

                return 0;
            }
            else if(code == LVN_ITEMCHANGED
                || code == LVN_DELETEITEM
                || code == LVN_DELETEALLITEMS
                )
            {
                auto btn_enable = _root->find<taowin::button>(L"enable");
                auto btn_modify = _root->find<taowin::button>(L"modify");
                auto btn_delete = _root->find<taowin::button>(L"delete");

                int sel_count = _listview->get_selected_count();
                int i = _listview->get_next_item(-1, LVNI_SELECTED);

                btn_enable->set_enabled(sel_count != 0);
                btn_modify->set_enabled(sel_count == 1 && i != -1 && !_modules[i]->enable);
                btn_delete->set_enabled(sel_count != 0);

                if(sel_count == 1) {
                    btn_enable->set_text(_modules[i]->enable ? L"禁用" : L"启用");
                }

                return 0;
            }
        }
        else if(pc->name() == L"enable") {
            int index = _listview->get_next_item(-1, LVNI_SELECTED);
            if(index != -1)
                _toggle_enable(index);
            return 0;
        }
        else if(pc->name() == L"add") {
            _add_item();
            return 0;
        }
        else if(pc->name() == L"modify") {
            int index = _listview->get_next_item(-1, LVNI_SELECTED);
            if(index != -1)
                _modify_item(index);
            return 0;
        }
        else if(pc->name() == L"delete") {
            _delete_item();
            return 0;
        }

        return 0;
    }

    virtual void on_final_message() override
    {
        __super::on_final_message();
        delete this;
    }

protected:
    void _toggle_enable(int i)
    {
        _modules[i]->enable = !_modules[i]->enable;
        _listview->redraw_items(i, i);

        _root->find<taowin::button>(L"enable")->set_text(_modules[i]->enable ? L"禁用" : L"启用");
        _root->find<taowin::button>(L"modify")->set_enabled(!_modules[i]->enable);

        // TODO fire event
    }

    void _modify_item(int i)
    {
        auto onok = [&](ModuleEntry* entry) {
            _listview->redraw_items(i, i);
            // TODO fire event
        };

        auto onguid = [&](const GUID& guid, std::wstring* err) {
            if(_has_guid(guid) && !::IsEqualGUID(guid, _modules[i]->guid)) {
                *err = L"此 GUID 已经存在。";
                return false;
            }

            return true;
        };

        (new ModuleEntryEditor(_modules[i], onok, onguid))->domodal(this);
    }

    void _delete_item()
    {
        int count = _listview->get_selected_count();
        const wchar_t* title = count == 1 ? _modules[_listview->get_next_item(-1, LVNI_SELECTED)]->name.c_str() : L"确认";

        if(msgbox((L"确定要删除选中的 " + std::to_wstring(count) + L" 项？").c_str(), MB_OKCANCEL|MB_ICONQUESTION, title) == IDOK) {
            int index = -1;
            std::vector<int> selected;
            while((index = _listview->get_next_item(index, LVNI_SELECTED)) != -1) {
                selected.push_back(index);
            }

            for(auto it = selected.crbegin(); it != selected.crend(); it++) {
                _listview->delete_item(*it);
                _modules.erase(_modules.begin() + *it);
            }
        }

        // TODO fire event
    }

    void _add_item()
    {
        auto onok = [&](ModuleEntry* entry) {
            _modules.push_back(entry);
            _listview->set_item_count((int)_modules.size(), LVSICF_NOINVALIDATEALL);
        };

        auto onguid = [&](const GUID& guid, std::wstring* err) {
            if(_has_guid(guid)) {
                *err = L"此 GUID 已经存在。";
                return false;
            }

            return true;
        };

        (new ModuleEntryEditor(nullptr, onok, onguid))->domodal(this);

        // TODO fire event
    }

    bool _has_guid(const GUID& guid)
    {
        for(auto& mod : _modules) {
            if(::IsEqualGUID(guid, mod->guid)) {
                return true;
            }
        }

        return false;
    }
};

//////////////////////////////////////////////////////////////////////////

class EventDetail : public taowin::window_creator
{
private:
    const ETWLogger::LogDataUI* _log;
    COLORREF _fg, _bg;
    HBRUSH _hbrBk;
    HWND _hText;

public:
    EventDetail(const ETWLogger::LogDataUI* log, COLORREF fg = RGB(0,0,0), COLORREF bg=RGB(255,255,255))
        : _log(log)
        , _fg(fg)
        , _bg(bg)
    {
        _hbrBk = ::CreateSolidBrush(_bg);
    }

    ~EventDetail()
    {
        ::DeleteObject(_hbrBk);
    }

protected:
	virtual LPCTSTR get_skin_xml() const override
	{
        LPCTSTR json = LR"tw(
<window title="详情" size="512,480">
    <res>
        <font name="default" face="微软雅黑" size="12"/>
        <font name="1" face="微软雅黑" size="12"/>
        <font name="consolas" face="Consolas" size="12"/>
    </res>
    <root>
        <vertical padding="5,5,5,5">
            <edit name="text" font="consolas" style="multiline,vscroll,hscroll,readonly" exstyle="clientedge"/>
        </vertical>
    </root>
</window>
)tw";
		return json;
	}

    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override
    {
        switch(umsg)
        {
        case WM_CREATE:
        {
            auto edit = _root->find<taowin::edit>(L"text");
            _hText = edit->hwnd();
            edit->set_text(_log->text);
            return 0;
        }
        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wparam;
            HWND hwnd = (HWND)lparam;

            if(hwnd == _hText) {
                ::SetTextColor(hdc, _fg);
                ::SetBkColor(hdc, _bg);
                return LRESULT(_hbrBk);
            }

            break;
        }

        }
        return __super::handle_message(umsg, wparam, lparam);
    }

    virtual void on_final_message() override
    {
        __super::on_final_message();
        delete this;
    }
};

class TW : public taowin::window_creator
{
private:
    taowin::listview* _listview;
    std::vector<ETWLogger::LogDataUI*> _events;
    struct ItemColor { COLORREF fg; COLORREF bg; };
    std::map<int, ItemColor> _colors;
    ColumnData _cols;
    std::vector<MenuEntry> _menus;
    std::vector<ModuleEntry*> _modules;
    Controller _controller;
    taoetw::Consumer _consumer;

public:
	TW()
	{}

protected:
	virtual LPCTSTR get_skin_xml() const override
	{
        LPCTSTR json = LR"tw(
<window title="ETW Log Viewer" size="820,600">
    <res>
        <font name="default" face="微软雅黑" size="12"/>
        <font name="1" face="微软雅黑" size="12"/>
    </res>
    <root>
        <vertical padding="5,5,5,5">
            <listview name="lv" style="singlesel,showselalways,ownerdata" exstyle="clientedge">  </listview>
        </vertical>
    </root>
</window>
)tw";
		return json;
	}

	virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override
	{
		switch(umsg)
		{
		case WM_CREATE:
		{
            _listview = _root->find<taowin::listview>(L"lv");

            _cols.cols.push_back({L"时间", true, 160 });
            _cols.cols.push_back({L"进程", true, 50  });
            _cols.cols.push_back({L"线程", true, 50, });
            _cols.cols.push_back({L"项目", true, 100,});
            _cols.cols.push_back({L"文件", true, 200,});
            _cols.cols.push_back({L"函数", true, 100,});
            _cols.cols.push_back({L"行号", true, 50, });
            _cols.cols.push_back({L"日志", true, 300,});

            for(int i = 0; i < (int)_cols.cols.size(); i++) {
                auto& col = _cols.cols[i];
                _listview->insert_column(col.name.c_str(), col.width, i);
            }

            _colors[TRACE_LEVEL_INFORMATION]    = {RGB(  0,   0,   0),  RGB(255, 255, 255)};
            _colors[TRACE_LEVEL_WARNING]        = {RGB(255, 128,   0),  RGB(255, 255, 255)};
            _colors[TRACE_LEVEL_ERROR]          = {RGB(255,   0,   0),  RGB(255, 255, 255)};
            _colors[TRACE_LEVEL_CRITICAL]       = {RGB(255, 255, 255),  RGB(255,   0,   0)};
            _colors[TRACE_LEVEL_VERBOSE]        = {RGB(  0,   0,   0),  RGB(255, 255, 255)};

            MenuEntry menu;

            menu.name = L"模块管理";
            menu.onclick = [&]() {
                auto* mgr = new ModuleManager(_modules);
                mgr->domodal(this);
            };

            _menus.push_back(menu);

            HMENU hMenu = ::CreateMenu();
            for(int i = 0; i < (int)_menus.size(); i++)
                ::AppendMenu(hMenu, MF_STRING, i, _menus[i].name.c_str());
            ::SetMenu(_hwnd, hMenu);

            _controller.start(g_etw_session);

            GUID guid = { 0x630514b5, 0x7b96, 0x4b74, { 0x9d, 0xb6, 0x66, 0xbd, 0x62, 0x1f, 0x93, 0x86 } };
            _controller.enable(guid, true, 0);

            taoetw::g_Consumer = &_consumer;

            _consumer.init(_hwnd, WM_USER + 0);
            _consumer.start(g_etw_session);

			return 0;
		}

        case WM_DO_LOG:
        {
            auto item = reinterpret_cast<ETWLogger::LogDataUI*>(lparam);

            const std::wstring* root = nullptr;

            _module_from_guid(item->guid, &item->strProject, &root);

            // 相对路径
            if(*item->file && root) {
                if(::wcsnicmp(item->file, root->c_str(), root->size()) == 0) {
                    item->offset_of_file = (int)root->size();
                }
            }
            else {
                item->offset_of_file = 0;
            }

            _events.push_back(item);
            _listview->set_item_count(_events.size(), LVSICF_NOINVALIDATEALL);

            break;
        }
		}
        return __super::handle_message(umsg, wparam, lparam);
	}


    virtual LRESULT on_menu(int id, bool is_accel = false)
    {
        if(id < (int)_menus.size()) {
            _menus[id].onclick();
        }
        return 0;
    }

    virtual LRESULT on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr) override {
        if(!pc) {
            if(hwnd == _listview->get_header()) {
                if(code == NM_RCLICK) {
                    auto colsel = new ColumnSelection(_cols);

                    colsel->OnToggle([&](int i) {
                        auto& col = _cols.cols[i];
                        _listview->set_column_width(i, col.show ? col.width : 0);
                    });

                    colsel->domodal(this);

                    return 0;
                }
                else if(code == HDN_ENDTRACK) {
                    auto nmhdr = (NMHEADER*)hdr;
                    auto& item = nmhdr->pitem;
                    auto& col = _cols.cols[nmhdr->iItem];

                    col.show = item->cxy != 0;
                    if(item->cxy) col.width = item->cxy;

                    return 0;
                }
            }

            return 0;
        }

        if(pc->name() == _T("lv")) {
            if (code == LVN_GETDISPINFO) {
                auto pdi = reinterpret_cast<NMLVDISPINFO*>(hdr);
                auto evt = _events[pdi->item.iItem];
                auto lit = &pdi->item;

                const TCHAR* value = _T("");

                switch(lit->iSubItem)
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
            else if (code == NM_CUSTOMDRAW) {
                LRESULT lr = CDRF_DODEFAULT;
                ETWLogger::LogDataUI* log;

                auto lvcd = (LPNMLVCUSTOMDRAW)hdr;

                switch(lvcd->nmcd.dwDrawStage)
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
            else if(code == NM_DBLCLK) {
                auto nmlv = reinterpret_cast<NMITEMACTIVATE*>(hdr);

                if(nmlv->iItem != -1) {
                    auto evt = _events[nmlv->iItem];
                    auto& cr = _colors[evt->level];
                    auto detail_window = new EventDetail(evt, cr.fg, cr.bg);
                    detail_window->create();
                    detail_window->show();
                }

                return 0;
            }
        }
        return 0;
    }

private:
    struct GUIDLessComparer {
        bool operator()(const GUID& left, const GUID& right) const {
            return ::memcmp(&left, &right, sizeof(GUID)) < 0;
        }
    };

    std::map<GUID, ModuleEntry*, GUIDLessComparer> _guid2mod;

    void _module_from_guid(const GUID& guid, std::wstring* name, const std::wstring** root)
    {
        if(!_guid2mod.count(guid)) {
            for(auto& item : _modules) {
                if(::IsEqualGUID(item->guid, guid)) {
                    _guid2mod[guid] = item;
                    break;
                }
            }
        }

        auto it = _guid2mod.find(guid);

        if(it != _guid2mod.cend()) {
            *name = it->second->name;
            *root = &it->second->root;
        }
        else {
            *name = L"<unknown>";
            *root = nullptr;
        }
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

int __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdlind, int nShowCmd)
{
    taowin::init();

    TW tw1;
    tw1.create();
    tw1.show();

    g_hWnd = tw1.hwnd();

    taowin::loop_message();

    return 0;
}

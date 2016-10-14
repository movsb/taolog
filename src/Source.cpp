
#include <cassert>

#include <string>
#include <iostream>
#include <thread>

#include <windows.h>
#include <process.h>
#include <evntrace.h>

#define ETW_LOGGER
#include "etwlogger.h"

#include <taowin/src/tw_taowin.h>

static HWND g_hWnd;
static const UINT WM_DO_LOG = WM_USER + 0;

// This GUID defines the event trace class. 	
// {6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}
static const GUID clsGuid = 
{ 0x6e5e5cbc, 0x8acf, 0x4fa6, { 0x98, 0xe4, 0xc, 0x63, 0xa0, 0x75, 0x32, 0x3b } };


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

ULONG __stdcall BufferCallback(EVENT_TRACE_LOGFILE* logfile)
{
    return TRUE;
}
;
void __stdcall ProcessEvents(EVENT_TRACE* pEvent)
{
    if(!pEvent || !IsEqualGUID(pEvent->Header.Guid, clsGuid))
        return;

    const auto& log_data = *(ETWLogger::LogData*)pEvent->MofData;
    auto log_ui = new ETWLogger::LogDataUI;

    ::memcpy(log_ui, &log_data, sizeof(log_data));
    log_ui->pid     = pEvent->Header.ProcessId;
    log_ui->tid     = pEvent->Header.ThreadId;
    log_ui->level   = pEvent->Header.Class.Level;

    log_ui->strPid  = std::to_wstring(log_ui->pid);
    log_ui->strTid  = std::to_wstring(log_ui->tid);
    log_ui->strLine = std::to_wstring(log_ui->line);

    {
        TCHAR buf[1024];
        auto& t = log_ui->time;

        _sntprintf(&buf[0], _countof(buf),
            _T("%d-%02d-%02d %02d:%02d:%02d:%03d"),
            t.wYear, t.wMonth, t.wDay,
            t.wHour, t.wMinute, t.wSecond, t.wMilliseconds
        );

        log_ui->strTime = buf;
    }

    ::PostMessage(g_hWnd, WM_DO_LOG, 0, LPARAM(log_ui));
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
<window title="Event columns" size="512,480">
    <res>
        <font name="default" face="Î¢ÈíÑÅºÚ" size="12"/>
        <font name="1" face="Î¢ÈíÑÅºÚ" size="12"/>
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
<window title="Event detail" size="512,480">
    <res>
        <font name="default" face="Î¢ÈíÑÅºÚ" size="12"/>
        <font name="1" face="Î¢ÈíÑÅºÚ" size="12"/>
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

public:
	TW()
	{}

protected:
	virtual LPCTSTR get_skin_xml() const override
	{
        LPCTSTR json = LR"tw(
<window title="ETW Log Viewer" size="820,600">
    <res>
        <font name="default" face="Î¢ÈíÑÅºÚ" size="12"/>
        <font name="1" face="Î¢ÈíÑÅºÚ" size="12"/>
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

            _cols.cols.push_back({L"Ê±¼ä", true, 160 });
            _cols.cols.push_back({L"½ø³Ì", true, 50  });
            _cols.cols.push_back({L"Ïß³Ì", true, 50, });
            _cols.cols.push_back({L"ÏîÄ¿", true, 100,});
            _cols.cols.push_back({L"ÎÄ¼þ", true, 200,});
            _cols.cols.push_back({L"º¯Êý", true, 100,});
            _cols.cols.push_back({L"ÐÐºÅ", true, 50, });
            _cols.cols.push_back({L"ÈÕÖ¾", true, 300,});

            for(int i = 0; i < (int)_cols.cols.size(); i++) {
                auto& col = _cols.cols[i];
                _listview->insert_column(col.name.c_str(), col.width, i);
            }

            _colors[TRACE_LEVEL_INFORMATION]    = {RGB(  0,   0,   0),  RGB(255, 255, 255)};
            _colors[TRACE_LEVEL_WARNING]        = {RGB(255, 128,   0),  RGB(255, 255, 255)};
            _colors[TRACE_LEVEL_ERROR]          = {RGB(255,   0,   0),  RGB(255, 255, 255)};
            _colors[TRACE_LEVEL_CRITICAL]       = {RGB(255, 255, 255),  RGB(255,   0,   0)};
            _colors[TRACE_LEVEL_VERBOSE]        = {RGB(  0,   0,   0),  RGB(255, 255, 255)};

            HMENU hMenu = ::CreateMenu();

            for(int i = 0; i < (int)_menus.size(); i++) {
                ::AppendMenu(hMenu, MF_STRING, i, _menus[i].name.c_str());
            }

            ::SetMenu(_hwnd, hMenu);

			return 0;
		}
        case WM_CLOSE:
            if(MessageBox(_hwnd, _T("È·ÈÏ¹Ø±Õ£¿"), nullptr, MB_OKCANCEL) != IDOK) {
                return 0;
            }
            break;

        case WM_DO_LOG:
        {
            _events.push_back(reinterpret_cast<ETWLogger::LogDataUI*>(lparam));
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
                case 0: value = evt->strTime.c_str();   break;
                case 1: value = evt->strPid.c_str();    break;
                case 2: value = evt->strTid.c_str();    break;
                case 3: value = _T("");                 break;
                case 4: value = evt->file;              break;
                case 5: value = evt->func;              break;
                case 6: value = evt->strLine.c_str();   break;
                case 7: value = evt->text;              break;
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
};

///////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
    taowin::init();

    TW tw1;
    tw1.create();
    tw1.show();

    g_hWnd = tw1.hwnd();

    std::wcout.imbue(std::locale("chs"));
    Controller controller;
    controller.start(L"xxxxxx");

    GUID guid = { 0x630514b5, 0x7b96, 0x4b74, { 0x9d, 0xb6, 0x66, 0xbd, 0x62, 0x1f, 0x93, 0x86 } };
    controller.enable(guid, true, 0);

    std::thread([]() {
        EVENT_TRACE_LOGFILE logfile {0};
        logfile.LoggerName = L"xxxxxx";
        logfile.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
        logfile.EventCallback = ProcessEvents;
        logfile.BufferCallback = BufferCallback;

        TRACEHANDLE handle = ::OpenTrace(&logfile);
        assert(handle != INVALID_PROCESSTRACE_HANDLE);

        ::ProcessTrace(&handle, 1, nullptr, nullptr);

        ::CloseTrace(handle);
    }).detach();

    taowin::loop_message();

    return 0;
}

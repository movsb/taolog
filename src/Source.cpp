
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
    log_ui->pid = pEvent->Header.ProcessId;
    log_ui->tid = pEvent->Header.ThreadId;

    log_ui->strPid = std::to_wstring(log_ui->pid);
    log_ui->strTid = std::to_wstring(log_ui->tid);
    log_ui->strLine = std::to_wstring(log_ui->line);

    {
        TCHAR buf[1024];
        auto& t = log_ui->time;

        _sntprintf(&buf[0], _countof(buf), _T("%d-%02d-%02d %02d:%02d:%02d:%03d"),
            t.wYear, t.wMonth, t.wDay,
            t.wHour, t.wMinute, t.wSecond, t.wMilliseconds
        );

        log_ui->strTime = buf;
    }

    ::PostMessage(g_hWnd, WM_DO_LOG, 0, LPARAM(log_ui));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
class TW : public taowin::window_creator
{
private:
    taowin::listview* _listview;
    std::vector<ETWLogger::LogDataUI*> _events;

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

            int i = 0;

            _listview->insert_column(L"时间", 160, i++);
            _listview->insert_column(L"进程", 50, i++);
            _listview->insert_column(L"线程", 50, i++);
            _listview->insert_column(L"项目", 100, i++);
            _listview->insert_column(L"文件", 200, i++);
            _listview->insert_column(L"函数", 100, i++);
            _listview->insert_column(L"行号", 50, i++);
            _listview->insert_column(L"日志", 300, i++);

			return 0;
		}
        case WM_CLOSE:
            if(MessageBox(_hwnd, _T("确认关闭？"), nullptr, MB_OKCANCEL) != IDOK) {
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

    virtual LRESULT on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr) override {
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

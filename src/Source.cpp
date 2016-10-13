
#include <cassert>

#include <string>
#include <iostream>

#include <windows.h>
#include <process.h>
#include <evntrace.h>

#define ETW_LOGGER
#include "etwlogger.h"

#include <taowin/src/tw_taowin.h>

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

    const ETWLogger::LogData& log = *(ETWLogger::LogData*)pEvent->MofData;
    std::wcout << log.text << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
class TW : public taowin::window_creator
{
public:
	TW()
	{}

protected:
	virtual LPCTSTR get_skin_xml() const override
	{
        LPCTSTR json = LR"tw(
<window title="taowinÑÝÊ¾´°¿Ú" size="500,300">
    <res>
        <font name="default" face="Î¢ÈíÑÅºÚ" size="12"/>
        <font name="1" face="Î¢ÈíÑÅºÚ" size="12"/>
    </res>
    <root>
        <vertical padding="5,5,5,5">
            <button name="t1" text="È¡Ïû" font="1"/>
            <listview name="lv" style="singlesel" exstyle="clientedge">  </listview>
            <horizontal>
            </horizontal>
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
			//center();
            auto lv = _root->find<taowin::listview>(L"lv");
            lv->insert_column(L"c1", 50, 0);
            lv->insert_column(L"c2", 50, 1);
            lv->insert_column(L"c3", 50, 2);
            lv->insert_item(_T("line1"));
            lv->insert_item(_T("line2"));
            lv->insert_item(_T("line3"));
			return 0;
		}
        case WM_CLOSE:
            if(MessageBox(_hwnd, _T("È·ÈÏ¹Ø±Õ£¿"), nullptr, MB_OKCANCEL) != IDOK) {
                return 0;
            }
		default:
			break;
		}
        return __super::handle_message(umsg, wparam, lparam);
	}

    virtual LRESULT on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr) override {
        if(pc->name() == _T("t1")) {
            if (code == BN_CLICKED) {
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

	try{
		TW tw1;
        tw1.create();
		tw1.show();

        taowin::loop_message();
	}
	catch(LPCTSTR){

	}

    taowin::loop_message();
	
    std::wcout.imbue(std::locale("chs"));
    Controller controller;
    controller.start(L"xxxxxx");

    GUID guid = { 0x630514b5, 0x7b96, 0x4b74, { 0x9d, 0xb6, 0x66, 0xbd, 0x62, 0x1f, 0x93, 0x86 } };
    controller.enable(guid, true, 0);

    EVENT_TRACE_LOGFILE logfile {0};
    logfile.LoggerName = L"xxxxxx";
    logfile.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    logfile.EventCallback = ProcessEvents;
    logfile.BufferCallback = BufferCallback;

    TRACEHANDLE handle = ::OpenTrace(&logfile);
    assert(handle != INVALID_PROCESSTRACE_HANDLE);

    ::ProcessTrace(&handle, 1, nullptr, nullptr);

    ::CloseTrace(handle);

    return 0;
}

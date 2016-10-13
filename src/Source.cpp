#define  _CRT_SECURE_NO_WARNINGS

#include <cassert>

#include <string>
#include <iostream>

#include <windows.h>
#include <process.h>
#include <evntrace.h>

#define ETW_LOGGER
#include "etwlogger.h"

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

int main()
{
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

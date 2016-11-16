#include <cassert>

#include <string>
#include <sstream>
#include <functional>
#include <memory>

#include <tchar.h>
#include <windows.h>
#include <process.h>
#include <wmistr.h>
#include <Evntrace.h>

#include "consumer.h"

namespace taoetw {

// This GUID defines the event trace class. 	
// {6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}
static const GUID g_clsGuid = 
{ 0x6e5e5cbc, 0x8acf, 0x4fa6, { 0x98, 0xe4, 0xc, 0x63, 0xa0, 0x75, 0x32, 0x3b } };

namespace {
    void a2u(const char* a, wchar_t* u, int c) {
        ::MultiByteToWideChar(CP_ACP, 0, a, -1, u, c);
    }
}

bool Consumer::start(const wchar_t * session)
{
    EVENT_TRACE_LOGFILE logfile {0};

    logfile.LoggerName = (LPWSTR)session;
    logfile.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    logfile.EventCallback = ProcessEvents;
    logfile.BufferCallback = BufferCallback;

    _handle = ::OpenTrace(&logfile);

    if (_handle == INVALID_PROCESSTRACE_HANDLE) {
        _handle = 0;
        return false;
    }

    HANDLE thread = (HANDLE)::_beginthreadex(nullptr, 0, ConsumerThread, (void*)_handle, 0, nullptr);
    if (!thread) {
        return false;
    }
    ::CloseHandle(thread);

    return true;
}

bool Consumer::stop()
{
    if (_handle > 0) {
        ::CloseTrace(_handle);
        _handle = 0;
    }

    return true;
}

ULONG Consumer::BufferCallback(EVENT_TRACE_LOGFILE * logfile)
{
    return TRUE;
}

void Consumer::ProcessEvents(EVENT_TRACE * pEvent)
{
    if (!pEvent || !IsEqualGUID(pEvent->Header.Guid, g_clsGuid))
        return;

    const auto log_data = (LogData*)pEvent->MofData;
    auto log_ui = LogDataUI::from_logdata(log_data, DoEtwAlloc());
    DoEtwLog(log_ui);
}

unsigned int Consumer::ConsumerThread(void * ud)
{
    TRACEHANDLE handle = (TRACEHANDLE)ud;
    ::ProcessTrace(&handle, 1, nullptr, nullptr);
    return 0;
}

}

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

    HWND g_loggerWindow;
    bool g_loggerOpen;
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

    CreateLoggerWindow();

    g_loggerOpen = true;

    return true;
}

bool Consumer::stop()
{
    if (_handle > 0) {
        ::CloseTrace(_handle);
        _handle = 0;
    }

    g_loggerOpen = false;

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

    if(g_loggerOpen) {
        DoEtwLog(pEvent->MofData);
    }
}

unsigned int Consumer::ConsumerThread(void * ud)
{
    TRACEHANDLE handle = (TRACEHANDLE)ud;
    ::ProcessTrace(&handle, 1, nullptr, nullptr);
    return 0;
}

LRESULT CALLBACK Consumer::__WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(uMsg == WM_COPYDATA) {
        auto cds = reinterpret_cast<COPYDATASTRUCT*>(lParam);
        if(g_loggerOpen) {
            taoetw::DoEtwLog(cds->lpData);
        }
        return 0;
    }

    return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void Consumer::CreateLoggerWindow()
{
    static bool bRegistered = false;

    if(bRegistered) return;

    WNDCLASSEX wcx = {sizeof(wcx)};
    wcx.lpszClassName = _T("{6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}");
    wcx.lpfnWndProc = __WindowProcedure;
    wcx.hInstance = ::GetModuleHandle(NULL);
    wcx.cbWndExtra = sizeof(void*);

    bRegistered = !!::RegisterClassEx(&wcx);

    g_loggerWindow = ::CreateWindowEx(0, 
        L"{6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}",
        L"{6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}::HostWnd",
        0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
}

void Consumer::DestroyLoggerWindow()
{
    ::DestroyWindow(g_loggerWindow);
    ::UnregisterClass(L"{6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}", ::GetModuleHandle(NULL));
}

}

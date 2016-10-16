#include <process.h>

#include "consumer.h"

namespace taoetw {

// This GUID defines the event trace class. 	
// {6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}
static const GUID g_clsGuid = 
{ 0x6e5e5cbc, 0x8acf, 0x4fa6, { 0x98, 0xe4, 0xc, 0x63, 0xa0, 0x75, 0x32, 0x3b } };

void Consumer::init(HWND hwnd, UINT umsg)
{
    _hwnd = hwnd;
    _umsg = umsg;
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

    const auto& log_data = *(ETWLogger::LogData*)pEvent->MofData;
    auto log_ui = new ETWLogger::LogDataUI;

    ::memcpy(log_ui, &log_data, pEvent->MofLength);
    assert(log_ui->text[log_ui->size - 1] == 0);

    log_ui->pid = pEvent->Header.ProcessId;
    log_ui->tid = pEvent->Header.ThreadId;
    log_ui->level = pEvent->Header.Class.Level;

    log_ui->strPid = std::to_wstring(log_ui->pid);
    log_ui->strTid = std::to_wstring(log_ui->tid);
    log_ui->strLine = std::to_wstring(log_ui->line);

    {
        TCHAR buf[1024];
        auto& t = log_ui->time;

        _sntprintf(&buf[0], _countof(buf),
            _T("%02d:%02d:%02d:%03d"),
            t.wHour, t.wMinute, t.wSecond, t.wMilliseconds
        );

        log_ui->strTime = buf;
    }

    DoEtwLog(log_ui);
}

unsigned int Consumer::ConsumerThread(void * ud)
{
    TRACEHANDLE handle = (TRACEHANDLE)ud;
    ::ProcessTrace(&handle, 1, nullptr, nullptr);
    return 0;
}

}

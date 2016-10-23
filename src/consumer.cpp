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

    const auto& log_data = *(LogData*)pEvent->MofData;
    auto log_ui = new LogDataUI;

    // 日志正文前面的部分都是一样的
    // 除了日志输出方长度固定，接收方长度不固定
    ::memcpy(log_ui, &log_data, sizeof(LogData));

    // 拷贝日志正文（cch包括 '\0'，因而始终大于零
    const wchar_t* pText = (const wchar_t*)((char*)&log_data + sizeof(LogData));
    assert(pText[log_ui->cch - 1] == 0);
    log_ui->strText.assign(pText, log_ui->cch - 1);

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

#pragma once

#include <string>
#include <functional>

#include <windows.h>

#include "etwlogger.h"

namespace taoetw {

class Consumer;

extern void DoEtwLog(ETWLogger::LogDataUI* log);

class Consumer
{
public:
    Consumer()
        : _hwnd(nullptr)
        , _umsg(0)
        , _handle(0)
        // , _thread(nullptr)
    {}

    ~Consumer()
    {
        stop();
    }

public:
    void init(HWND hwnd, UINT umsg);
    bool start(const wchar_t* session);
    bool stop();


protected:
    static ULONG __stdcall BufferCallback(EVENT_TRACE_LOGFILE* logfile);
    static void __stdcall ProcessEvents(EVENT_TRACE* pEvent);
    static unsigned int __stdcall ConsumerThread(void* ud);

protected:
    // HANDLE _thread;
    TRACEHANDLE _handle;
    HWND _hwnd;
    UINT _umsg;
};


} // namespace taoetw

#pragma once

namespace taoetw {

class Consumer;

extern void DoEtwLog(void* log);

class Consumer
{
public:
    Consumer()
        : _handle(0)
    {}

    ~Consumer()
    {
        stop();
    }

public:
    bool start(const wchar_t* session);
    bool stop();


protected:
    static ULONG __stdcall BufferCallback(EVENT_TRACE_LOGFILE* logfile);
    static void __stdcall ProcessEvents(EVENT_TRACE* pEvent);
    static unsigned int __stdcall ConsumerThread(void* ud);

protected:
    TRACEHANDLE _handle;
};


} // namespace taoetw

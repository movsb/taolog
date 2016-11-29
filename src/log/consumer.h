#pragma once

namespace taoetw {

class Consumer;

extern void DoEtwLog(void* log);

class Consumer
{
public:
    Consumer()
        : _handle(0)
    {
        CreateLoggerWindow();
    }

    ~Consumer()
    {
        stop();
        DestroyLoggerWindow();
    }

public:
    bool start(const wchar_t* session);
    bool stop();


protected:
    static ULONG __stdcall BufferCallback(EVENT_TRACE_LOGFILE* logfile);
    static void __stdcall ProcessEvents(EVENT_TRACE* pEvent);
    static unsigned int __stdcall ConsumerThread(void* ud);

    static LRESULT CALLBACK __WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static void CreateLoggerWindow();
    static void DestroyLoggerWindow();

protected:
    TRACEHANDLE _handle;
};


} // namespace taoetw

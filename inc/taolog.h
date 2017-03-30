#pragma once

#ifdef TAOLOG_ENABLED

#pragma warning(push)
#pragma warning(disable: 4996)

#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>
#include <Windows.h>
#include <wmistr.h>
#include <guiddef.h>
#include <Evntrace.h>
#include <process.h>
#include <vector>

// TODO rename
#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define __WFILE__ WIDEN(__FILE__)
#define __WFUNCTION__ WIDEN(__FUNCTION__)

#ifdef _UNICODE
#define __TFILE__ __WFILE__
#define __TFUNCTION__ __WFUNCTION__
#else
#define __TFILE__ __FILE__
#define __TFUNCTION__ __FUNCTION__
#endif

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define TAOLOG_MAX_LOG_SIZE (50*1024)

#if !defined(TAOLOG_METHOD_COPYDATA) && !defined(TAOLOG_METHOD_EVENTTRACING)
  #define TAOLOG_METHOD_EVENTTRACING
#endif

class TaoLogger
{
public:

    enum ETW_LOGGER_FLAG {
        ETW_LOGGER_FLAG_UNICODE = 1,
    };

    #pragma pack(push,1)
    struct LogData
    {
        UCHAR           version;                            // 日志版本
        int             pid;                                // 进程编号
        int             tid;                                // 线程编号
        UCHAR           level;                              // 日志等级
        UINT            flags;                              // 标记位
        GUID            guid;                               // 生成者
        SYSTEMTIME      time;                               // 时间戳
        unsigned int    line;                               // 行号
        unsigned int    cch;                                // 字符数（包含null）
        wchar_t         file[260];                          // 文件
        wchar_t         func[260];                          // 函数
        wchar_t         text[TAOLOG_MAX_LOG_SIZE];          // 日志
    };
    #pragma pack(pop)

protected:
#ifdef TAOLOG_METHOD_COPYDATA
    struct LogDataMsg
    {
        LogData log;
    };
#endif

#ifdef TAOLOG_METHOD_EVENTTRACING
    struct LogDataMsg
    {
        EVENT_TRACE_HEADER hdr;
        LogData log;
    };
#endif

    template<typename T>
    class MemPoolT
    {
        union MyT {
            MyT* next;
            char data[sizeof(T)];
        };

        typedef std::vector<MyT*> TS;

    public:
        MemPoolT()
        {
            _ts.reserve(1024);
        }

        ~MemPoolT()
        {
            clear();
        }

        T* create()
        {
            if(!_root) {
                _root = new MyT;
                _root->next = NULL;
                _ts.push_back(_root);
            }

            T* p = reinterpret_cast<T*>(_root);

            _root = _root->next;
            _using++;

            return new (p) T;
        }

        void destroy(T* p)
        {
            p->~T();

            MyT* myt = reinterpret_cast<MyT*>(p);
            myt->next = _root;
            _root = myt;

            _using--;
        }

        void clear()
        {
            for(TS::const_iterator begin = _ts.begin(), end = _ts.end(); begin != end; ++begin)
                delete *begin;

            _ts.clear();
            _root = NULL;
            _using = 0;
        }

        bool collect()
        {
            bool dirty = false;
            const int limit = 16;

            if(_ts.size() - _using >= limit)
            {
                dirty = true;

                while(_root)
                {
                    MyT* next = _root->next;
                    delete _root;
                    _root = next;
                }
            }

            return dirty;
        }

    protected:
        MyT*    _root;
        TS      _ts;
        size_t  _using;
    };

    struct WndMsg
    {
        enum Value
        {
            __Start = WM_USER,
            Alloc,
            Dealloc,
            Trace,
        };
    };

public:
	TaoLogger(const GUID & providerGuid)
		:m_providerGuid(providerGuid)
		,m_registrationHandle(NULL)
		,m_sessionHandle(NULL)
		,m_traceOn(false)
		,m_enableLevel(0)
		,m_reg(FALSE)
        ,m_hWnd(NULL)
        ,m_Root(NULL)
	{
		// This GUID defines the event trace class. 	
		// {6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}
		const GUID clsGuid = 
		{ 0x6e5e5cbc, 0x8acf, 0x4fa6, { 0x98, 0xe4, 0xc, 0x63, 0xa0, 0x75, 0x32, 0x3b } };

		m_clsGuid = clsGuid;

        m_version = 0x0000;

		RegisterProvider();
        RegisterLoggerWindowClass();

        m_Thread = (HANDLE)::_beginthreadex(NULL, 0, __ThreadProc, this, 0, NULL);
        while(!::IsWindow(m_hWnd))
            ::Sleep(100);
	}

	~TaoLogger()
	{
		UnRegisterProvider();

        ::SendMessage(m_hWnd, WM_CLOSE, 0, 0);

        m_logs.clear();

        ::CloseHandle(m_Thread);
	}

public:
	static ULONG WINAPI ControlCallback(WMIDPREQUESTCODE requestCode, PVOID context, ULONG* /* reserved */, PVOID buffer)
	{
		ULONG ret = ERROR_SUCCESS;	

		TaoLogger * logger = (TaoLogger *)context;
		if(logger != NULL && buffer != NULL)
		{
			switch (requestCode)
			{
			case WMI_ENABLE_EVENTS:  //Enable Provider.
				{
                    logger->m_HostWnd = ::FindWindowEx(HWND_MESSAGE, NULL, _T("{6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}"), _T("{6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}::HostWnd"));

					logger->m_sessionHandle = GetTraceLoggerHandle(buffer);
					if (INVALID_HANDLE_VALUE == (HANDLE)logger->m_sessionHandle)
					{
						break;
					}

					SetLastError(ERROR_SUCCESS);
					logger->m_enableLevel = GetTraceEnableLevel(logger->m_sessionHandle); 
					if (!logger->m_enableLevel)
					{
						DWORD id = GetLastError();
						if (id != ERROR_SUCCESS)
						{
							break;
						} 
						else  // Decide what a zero enable level means to your provider
						{
							logger->m_enableLevel = TRACE_LEVEL_WARNING; 
						}
					}	

					logger->m_traceOn = TRUE;
					break;
				}

			case WMI_DISABLE_EVENTS:  // Disable Provider.
				{
					logger->m_traceOn = FALSE;
                    logger->m_HostWnd = NULL;
					logger->m_sessionHandle = NULL;
					break;
				}

			default:
				{
					ret = ERROR_INVALID_PARAMETER;
					break;
				}
			}
		}			

		return ret;
	}

	void RegisterProvider()
	{
		TRACE_GUID_REGISTRATION eventClassGuids[] = { (LPGUID)&m_clsGuid, NULL};

		ULONG status = RegisterTraceGuids( (WMIDPREQUEST)ControlCallback, this, (LPGUID)&m_providerGuid, 
			sizeof(eventClassGuids)/sizeof(TRACE_GUID_REGISTRATION), eventClassGuids, NULL, NULL, &m_registrationHandle);
		if (ERROR_SUCCESS == status)
		{
			m_reg = TRUE;
		}
	}

	void UnRegisterProvider()
	{
		if(m_reg && m_registrationHandle != NULL)
		{
			::UnregisterTraceGuids(m_registrationHandle);
			m_registrationHandle = NULL;
			m_reg = FALSE;
		}		
	}

	// 判断是否打印日志
	bool IsLog(unsigned char level)
	{
        return m_traceOn && level <= m_enableLevel;
	}

	void WriteEvent(unsigned char level, const TCHAR* file, const TCHAR* function, unsigned int line, const TCHAR* format, ...)
	{
        if (!IsLog(level))
            return;

        int cch;

        LogDataMsg* logmsg = (LogDataMsg*)::SendMessage(m_hWnd, WndMsg::Alloc, 0, 0);

        LogData& data = logmsg->log;

        data.version = m_version;
        data.pid = ::GetCurrentProcessId();
        data.tid = (int)::GetCurrentThreadId();
        data.level = level;

        // the flags
        data.flags = 0;
        data.flags |= sizeof(TCHAR) == sizeof(wchar_t) ? ETW_LOGGER_FLAG_UNICODE : 0;

        // the guid
        data.guid = m_providerGuid;

        // time timestamp
        ::GetLocalTime(&data.time);
        
        // the file
        cch = min(_countof(data.file) - 1, _tcslen(file));
        _tcsncpy((TCHAR*)&data.file[0], file, cch);
        ((TCHAR*)(data.file))[cch] = 0;

        // the function
        cch = min(_countof(data.func) - 1, _tcslen(function));
        _tcsncpy((TCHAR*)&data.func[0], function, cch);
        ((TCHAR*)(data.func))[cch] = 0;

        // the line number
        data.line = line;

        // the text
        va_list va;
        va_start(va, format);
        cch = _vsntprintf((TCHAR*)&data.text[0], _countof(data.text), format, va);
        va_end(va);

        // the text length, in characters, including the null
        if (0 <= cch && cch < _countof(data.text)) {
            data.cch = cch + 1;
        } else {
            data.cch = 1;
            data.text[0] = 0;
        }

#ifdef TAOLOG_METHOD_COPYDATA
        ::PostMessage(m_hWnd, WndMsg::Trace, 0, reinterpret_cast<LPARAM>(logmsg));
#endif

#ifdef TAOLOG_METHOD_EVENTTRACING
        // the header
        EVENT_TRACE_HEADER& hdr = logmsg->hdr;
        memset(&hdr, 0, sizeof(hdr));

        // 还没搞懂 MSDN 上下面这句话的意思
        // On input, the size must be less than the size of the event tracing session's buffer minus 72 (0x48)
        hdr.Size            = (USHORT)(sizeof(hdr) + offsetof(LogData, text) + data.cch * sizeof(data.text[0]));
        hdr.Class.Type      = EVENT_TRACE_TYPE_INFO;
        hdr.Class.Level     = level;
        hdr.Class.Version   = m_version;
        hdr.Guid            = m_clsGuid;
        hdr.Flags           = WNODE_FLAG_TRACED_GUID;

        // Trace it!
        ::TraceEvent(m_sessionHandle, &hdr);

        ::PostMessage(m_hWnd, WndMsg::Dealloc, 0, reinterpret_cast<LPARAM>(logmsg));
#endif
	}

    LRESULT CALLBACK WindowProcedure(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch(uMsg) {
        case WndMsg::Alloc:
        {
            return reinterpret_cast<LRESULT>(m_logs.create());
        }

        case WndMsg::Trace:
        {
            LogDataMsg* logmsg = reinterpret_cast<LogDataMsg*>(lParam);
            COPYDATASTRUCT cds;
            cds.dwData = 0;
            cds.cbData = offsetof(LogData, text) + logmsg->log.cch * sizeof(logmsg->log.text[0]);
            cds.lpData = static_cast<void*>(logmsg);

            ::SendMessage(m_HostWnd, WM_COPYDATA, WPARAM(m_hWnd), LPARAM(&cds));

            m_logs.destroy(logmsg);

            return 0;
        }
        case WndMsg::Dealloc:
        {
            LogDataMsg* logmsg = reinterpret_cast<LogDataMsg*>(lParam);
            m_logs.destroy(logmsg);

            return 0;
        }
        case WM_CREATE:
        {
            ::SetTimer(m_hWnd, 0, 10 * 1000, 0);
            return 0;
        }
        case WM_TIMER:
        {
            if(wParam == 0) {
                bool dirty = m_logs.collect();
                ::KillTimer(m_hWnd, 0);
                ::SetTimer(m_hWnd, 0, dirty ? 10 * 1000 : 10 * 60 * 1000, NULL);
                return 0;
            }

            break;
        }
        }

        return ::DefWindowProc(m_hWnd, uMsg, wParam, lParam);
    }

    static unsigned int __stdcall __ThreadProc(void* ud)
    {
        TaoLogger* logger = reinterpret_cast<TaoLogger*>(ud);

        MSG msg;
        ::PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

        (void)::CreateWindowEx(0, _T("{6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}"), NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, LPVOID(logger));

        while(::GetMessage(&msg, NULL, 0, 0))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        return (int)msg.wParam;
    }

    static LRESULT CALLBACK __WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        TaoLogger* logger = reinterpret_cast<TaoLogger*>(::GetWindowLongPtr(hWnd, 0));

        if(uMsg == WM_NCCREATE) {
            LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
            logger = static_cast<TaoLogger*>(lpcs->lpCreateParams);
            logger->m_hWnd = hWnd;
            ::SetWindowLongPtr(hWnd, 0, reinterpret_cast<LPARAM>(logger));
            return TRUE;
        }

        return logger
            ? logger->WindowProcedure(uMsg, wParam, lParam)
            : ::DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    static void RegisterLoggerWindowClass()
    {
        static bool bRegistered = false;

        if(bRegistered) return;

        WNDCLASSEX wcx = {sizeof(wcx)};
        wcx.lpszClassName = _T("{6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}");
        wcx.lpfnWndProc = __WindowProcedure;
        wcx.hInstance = ::GetModuleHandle(NULL);
        wcx.cbWndExtra = sizeof(void*);

        bRegistered = !!::RegisterClassEx(&wcx);
    }

private:
    MemPoolT<LogDataMsg> m_logs;
    HANDLE m_Thread;
    HWND m_hWnd;
    HWND m_HostWnd;
    LogDataMsg* m_Root;
    UCHAR m_version;
	UCHAR m_enableLevel; 
	volatile bool m_traceOn; 
	BOOL m_reg;
	GUID m_providerGuid;
	GUID m_clsGuid;
	TRACEHANDLE m_registrationHandle;
	TRACEHANDLE m_sessionHandle;
};

extern TaoLogger g_taoLogger;


#define TaoLogMessage (g_taoLogger.WriteEvent)

#define LogVbs(x, ...)                  TaoLogMessage(TRACE_LEVEL_VERBOSE,      __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__)
#define LogLog(x, ...)                  TaoLogMessage(TRACE_LEVEL_INFORMATION,  __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__)
#define LogWrn(x, ...)                  TaoLogMessage(TRACE_LEVEL_WARNING,      __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__)
#define LogErr(x, ...)                  TaoLogMessage(TRACE_LEVEL_ERROR,        __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__)
#define LogFat(x, ...)                  TaoLogMessage(TRACE_LEVEL_CRITICAL,     __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__)

#define LogEnter()                      LogLog(_T("Enter"))
#define LogExit()                       LogLog(_T("Exit"))

// #define LogAll(file, func, line, fmt, ...)   EtwLogMessage(TRACE_LEVEL_INFORMATION, file, func, line, fmt, __VA_ARGS__)

#pragma warning(pop)

#else

#define LogVbs(...)                     ((void)0)
#define LogLog(...)                     ((void)0)
#define LogWrn(...)                     ((void)0)
#define LogErr(...)                     ((void)0)
#define LogFat(...)                     ((void)0)

#define LogEnter()                      ((void)0)
#define LogExit()                       ((void)0)

#endif

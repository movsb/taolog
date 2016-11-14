#pragma once

#ifdef ETW_LOGGER

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
#include <list>

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

#define ETW_LOGGER_MAX_LOG_SIZE (50*1024)

class ETWLogger
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
        wchar_t         text[ETW_LOGGER_MAX_LOG_SIZE];      // 日志
    };
    #pragma pack(pop)

protected:
    union LogDataMsg
    {
        LogDataMsg* next;
        LogData log;
    };

    struct WndMsg
    {
        enum Value
        {
            __Start = WM_USER,
            Alloc,
            Trace,
        };
    };

public:
	ETWLogger(const GUID & providerGuid)
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

	~ETWLogger()
	{
		UnRegisterProvider();

        ::SendMessage(m_hWnd, WM_CLOSE, 0, 0);

        for(const auto& log : m_logs)
            ::operator delete(log);

        m_logs.clear();

        ::CloseHandle(m_Thread);
	}

protected:
    LogDataMsg* Alloc()
    {
        static int n = 0;
        if(!m_Root) {
            m_Root = (LogDataMsg*)::operator new(sizeof(LogDataMsg));
            m_Root->next = NULL;
            m_logs.emplace_back(m_Root);
        }

        LogDataMsg* node = m_Root;
        m_Root = m_Root->next;

        return new (node) LogDataMsg;
    }

    void Dealloc(LogDataMsg* p)
    {
        p->~LogDataMsg();
        p->next = m_Root;
        m_Root = p;
    }

public:
	static ULONG WINAPI ControlCallback(WMIDPREQUESTCODE requestCode, PVOID context, ULONG* /* reserved */, PVOID buffer)
	{
		ULONG ret = ERROR_SUCCESS;	

		ETWLogger * logger = (ETWLogger *)context;
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

        LRESULT lr = ::PostMessage(m_hWnd, WndMsg::Trace, 0, reinterpret_cast<LPARAM>(logmsg));
        lr = lr;
	}

    LRESULT CALLBACK WindowProcedure(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch(uMsg)
        {
        case WndMsg::Alloc:
        {
            return reinterpret_cast<LRESULT>(Alloc());
        }

        case WndMsg::Trace:
        {
            LogDataMsg* logmsg = reinterpret_cast<LogDataMsg*>(lParam);
            COPYDATASTRUCT cds;
            cds.dwData = 0;
            cds.cbData = offsetof(LogData, text) + logmsg->log.cch * sizeof(logmsg->log.text[0]);
            cds.lpData = static_cast<void*>(logmsg);

            ::SendMessage(m_HostWnd, WM_COPYDATA, WPARAM(m_hWnd), LPARAM(&cds));
            Dealloc(logmsg);
            return 0;
        }
        }

        return ::DefWindowProc(m_hWnd, uMsg, wParam, lParam);
    }

    static unsigned int __stdcall __ThreadProc(void* ud)
    {
        ETWLogger* logger = reinterpret_cast<ETWLogger*>(ud);

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
        ETWLogger* logger = reinterpret_cast<ETWLogger*>(::GetWindowLongPtr(hWnd, 0));

        if(uMsg == WM_NCCREATE) {
            LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
            logger = static_cast<ETWLogger*>(lpcs->lpCreateParams);
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
    std::list<LogDataMsg*> m_logs;
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

extern ETWLogger g_etwLogger;


#define EtwLogMessage (g_etwLogger.WriteEvent)

// Abnormal exit or termination events
#define ETW_LEVEL_CRITICAL(x, ...) \
{ \
     EtwLogMessage(TRACE_LEVEL_CRITICAL, __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__); \
}

// Severe error events
#define ETW_LEVEL_ERROR(x, ...) \
{ \
    EtwLogMessage(TRACE_LEVEL_ERROR, __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__); \
}

// Warning events such as allocation failures
#define ETW_LEVEL_WARNING(x, ...) \
{ \
    EtwLogMessage(TRACE_LEVEL_WARNING, __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__); \
}

// Non-error events such as entry or exit events
#define ETW_LEVEL_INFORMATION(x, ...) \
{ \
	EtwLogMessage(TRACE_LEVEL_INFORMATION, __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__); \
}

// Detailed trace events
#define ETW_LEVEL_VERBOSE(x, ...) \
{ \
	EtwLogMessage(TRACE_LEVEL_VERBOSE, __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__); \
}

// winapi last error, level TRACE_LEVEL_WARNING
#define ETW_LAST_ERROR() \
{  																		    \
	DWORD errid = ::GetLastError();										    \
	TCHAR erridBuf[MAX_PATH] = {0};											\
	_stprintf_s(erridBuf, sizeof(erridBuf)/sizeof(TCHAR), _T("errorid=%u"), errid);	\
	ETW_LEVEL_WARNING(erridBuf); 													\
}

#define EtwVbs(x, ...) ETW_LEVEL_VERBOSE(x, __VA_ARGS__)
#define EtwLog(x, ...) ETW_LEVEL_INFORMATION(x, __VA_ARGS__)
#define EtwWrn(x, ...) ETW_LEVEL_WARNING(x, __VA_ARGS__)
#define EtwErr(x, ...) ETW_LEVEL_ERROR(x, __VA_ARGS__)
#define EtwFat(x, ...) ETW_LEVEL_CRITICAL(x, __VA_ARGS__)

#pragma warning(pop)

#else 

#define ETW_LEVEL_CRITICAL(x, ...) 
#define ETW_LEVEL_ERROR(x, ...)
#define ETW_LEVEL_WARNING(x, ...)
#define ETW_LEVEL_INFORMATION(x, ...)
#define ETW_LEVEL_VERBOSE(x, ...)

#define EtwVbs(x, ...)
#define EtwLog(x, ...)
#define EtwWrn(x, ...)
#define EtwErr(x, ...)
#define EtwFat(x, ...)

#define ETW_LAST_ERROR()

#endif

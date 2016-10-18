#pragma once 

#ifdef ETW_LOGGER

#pragma warning(push)
#pragma warning(disable: 4996)

#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>
#include <assert.h>
#include <Windows.h>
#include <wmistr.h>
#include <guiddef.h>
#include <Evntrace.h>
#include <string>

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

#define ETW_LOGGER_MAX_LOG_SIZE (60*1024)

class ETWLogger
{
    typedef std::basic_string<TCHAR> string;

public:
    struct LogData
    {
        GUID guid;              // 生成者 GUID
        SYSTEMTIME time;        // 时间戳
        unsigned int line;      // 行号
        unsigned int size;      // 字符数（包含null）
        wchar_t file[1024];     // 文件
        wchar_t func[1024];     // 函数
        wchar_t text[ETW_LOGGER_MAX_LOG_SIZE];      // 日志
    };

    struct LogDataTrace
    {
        EVENT_TRACE_HEADER hdr;
        LogData data;
    };

public:
	ETWLogger(const GUID & providerGuid)
		:m_providerGuid(providerGuid)
		,m_registrationHandle(NULL)
		,m_sessionHandle(NULL)
		,m_traceOn(FALSE)
		,m_enableLevel(0)
		,m_reg(FALSE)
	{
		::InitializeCriticalSection(&m_cs);

		// This GUID defines the event trace class. 	
		// {6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}
		const GUID clsGuid = 
		{ 0x6e5e5cbc, 0x8acf, 0x4fa6, { 0x98, 0xe4, 0xc, 0x63, 0xa0, 0x75, 0x32, 0x3b } };

		m_clsGuid = clsGuid;

		RegisterProvider();
	}

	~ETWLogger()
	{
		::DeleteCriticalSection(&m_cs);

		UnRegisterProvider();
	}

	void lock()
	{
		::EnterCriticalSection(&m_cs);
	}

	void unlock()
	{
		::LeaveCriticalSection(&m_cs);
	}

	static ULONG WINAPI ControlCallback(WMIDPREQUESTCODE requestCode, PVOID context, ULONG* reserved, PVOID buffer)
	{
		ULONG ret = ERROR_SUCCESS;	

		UNREFERENCED_PARAMETER(reserved);// 仅消除编译等级4警告，没有任何作用

		ETWLogger * logger = (ETWLogger *)context;
		if(logger != NULL && buffer != NULL)
		{
			logger->lock();

			switch (requestCode)
			{
			case WMI_ENABLE_EVENTS:  //Enable Provider.
				{
					logger->m_sessionHandle = GetTraceLoggerHandle(buffer);
					if (INVALID_HANDLE_VALUE == (HANDLE)logger->m_sessionHandle)
					{
						wprintf(L"GetTraceLoggerHandle failed with, %d.\n", ret);
						break;
					}

					SetLastError(ERROR_SUCCESS);
					logger->m_enableLevel = GetTraceEnableLevel(logger->m_sessionHandle); 
					if (!logger->m_enableLevel)
					{
						DWORD id = GetLastError();
						if (id != ERROR_SUCCESS)
						{
							wprintf(L"GetTraceEnableLevel failed with, %d.\n", ret);
							break;
						} 
						else  // Decide what a zero enable level means to your provider
						{
							logger->m_enableLevel = TRACE_LEVEL_WARNING; 
						}
					}	

					wprintf(L"Tracing enabled for Level %u\n", logger->m_enableLevel);

					logger->m_traceOn = TRUE;
					break;
				}

			case WMI_DISABLE_EVENTS:  // Disable Provider.
				{
					logger->m_traceOn = FALSE;
					logger->m_sessionHandle = NULL;
					break;
				}

			default:
				{
					ret = ERROR_INVALID_PARAMETER;
					break;
				}
			}

			logger->unlock();
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
	BOOL IsLog(unsigned char level)
	{
		BOOL ret = FALSE;

		lock();

		if (m_traceOn && level <= m_enableLevel)
		{
			ret = TRUE;
		}

		unlock();

		return ret;
	}

	void WriteEvent(unsigned char level, const TCHAR* file, const TCHAR* function, unsigned int line, const TCHAR* format, ...)
	{
        if (!IsLog(level))
            return;

		lock();	

        int cch;

        LogData& data = m_log.data;

        // the guid
        data.guid = m_providerGuid;
        
        // the file
        cch = min(_countof(data.file) - 1, _tcslen(file));
        _tcsncpy(&data.file[0], file, cch);
        data.file[cch] = 0;

        // the function
        cch = min(_countof(data.func) - 1, _tcslen(function));
        _tcsncpy(&data.func[0], function, cch);
        data.func[cch] = 0;

        // the line number
        data.line = line;

        // the text
        va_list va;
        va_start(va, format);
        cch = _vsntprintf(&data.text[0], _countof(data.text), format, va);
        va_end(va);

        // the text length, in characters, including the null
        if (0 <= cch && cch < _countof(data.text)) {
            data.size = cch + 1;
        } else {
            data.size = 1;
            data.text[0] = 0;
        }

        // time timestamp
        ::GetLocalTime(&data.time);

        // the header
        EVENT_TRACE_HEADER& hdr = m_log.hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.Size            = (USHORT)(sizeof(hdr) + offsetof(LogData, text) + data.size * sizeof(data.text[0]));
        hdr.Flags           = WNODE_FLAG_TRACED_GUID;
        hdr.Guid            = m_clsGuid;
        hdr.Class.Level     = level;

        // Trace it!
        ULONG status = ::TraceEvent(m_sessionHandle, &hdr);
        if (ERROR_SUCCESS != status)
        {
            if (ERROR_INVALID_HANDLE == status)
            {
                m_traceOn = FALSE;
            }
        }

		unlock();
	}

private:
	
	UCHAR m_enableLevel; 
	BOOL m_traceOn; 
	BOOL m_reg;
	GUID m_providerGuid;
	GUID m_clsGuid;
	TRACEHANDLE m_registrationHandle;
	TRACEHANDLE m_sessionHandle;

    LogDataTrace m_log;

	CRITICAL_SECTION m_cs;
};

extern ETWLogger g_etwLogger;


// 大部分应用场景采用栈空间打印日志，当遇到比较大的日志信息时会使用堆空间打印日志
#define LogMessage (g_etwLogger.WriteEvent)

// Abnormal exit or termination events
#define ETW_LEVEL_CRITICAL(x, ...) \
{ \
 LogMessage(TRACE_LEVEL_CRITICAL, __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__); \
}

// Severe error events
#define ETW_LEVEL_ERROR(x, ...) \
{ \
LogMessage(TRACE_LEVEL_ERROR, __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__); \
}

// Warning events such as allocation failures
#define ETW_LEVEL_WARNING(x, ...) \
{ \
LogMessage(TRACE_LEVEL_WARNING, __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__); \
}

// Non-error events such as entry or exit events
#define ETW_LEVEL_INFORMATION(x, ...) \
{ \
	LogMessage(TRACE_LEVEL_INFORMATION, __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__); \
}

// Detailed trace events
#define ETW_LEVEL_VERBOSE(x, ...) \
{ \
	LogMessage(TRACE_LEVEL_VERBOSE, __TFILE__, __TFUNCTION__, __LINE__, x, __VA_ARGS__); \
}

// winapi last error, level TRACE_LEVEL_WARNING
#define ETW_LAST_ERROR() \
{  																		    \
	DWORD errid = ::GetLastError();										    \
	TCHAR erridBuf[MAX_PATH] = {0};											\
	_stprintf_s(erridBuf, sizeof(erridBuf)/sizeof(TCHAR), _T("errorid=%u"), errid);	\
	ETW_LEVEL_WARNING(erridBuf); 													\
}


#else 

#define ETW_LEVEL_CRITICAL(x, ...) 
#define ETW_LEVEL_ERROR(x, ...)
#define ETW_LEVEL_WARNING(x, ...)
#define ETW_LEVEL_INFORMATION(x, ...)
#define ETW_LEVEL_VERBOSE(x, ...)

#define ETW_LAST_ERROR()

#pragma warning(pop)

#endif
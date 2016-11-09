#pragma once

#ifdef ETW_LOGGER_MT
  #define ETW_LOGGER
#endif

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
		,m_traceOn(false)
		,m_enableLevel(0)
		,m_reg(FALSE)
	{
		// This GUID defines the event trace class. 	
		// {6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}
		const GUID clsGuid = 
		{ 0x6e5e5cbc, 0x8acf, 0x4fa6, { 0x98, 0xe4, 0xc, 0x63, 0xa0, 0x75, 0x32, 0x3b } };

		m_clsGuid = clsGuid;

        m_version = 0x0000;

#ifdef ETW_LOGGER_MT
        ::InitializeCriticalSection(&_cs);
#endif

		RegisterProvider();
	}

	~ETWLogger()
	{
#ifdef ETW_LOGGER_MT
        ::DeleteCriticalSection(&_cs);
#endif
		UnRegisterProvider();
	}

#ifdef ETW_LOGGER_MT
    void lock()
    {
        ::EnterCriticalSection(&_cs);
    }

    void unlock()
    {
        ::LeaveCriticalSection(&_cs);
    }
#endif

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

#ifdef ETW_LOGGER_MT
        lock();
#endif

        int cch;

        LogData& data = m_log.data;

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

        // the header
        EVENT_TRACE_HEADER& hdr = m_log.hdr;
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
        ULONG status = ::TraceEvent(m_sessionHandle, &hdr);
        if (ERROR_SUCCESS != status)
        {
            if (ERROR_INVALID_HANDLE == status)
            {
                m_traceOn = FALSE;
            }
        }

#ifdef ETW_LOGGER_MT
        unlock();
#endif
	}

private:
	
    USHORT m_version;
	UCHAR m_enableLevel; 
	volatile bool m_traceOn; 
	BOOL m_reg;
	GUID m_providerGuid;
	GUID m_clsGuid;
	TRACEHANDLE m_registrationHandle;
	TRACEHANDLE m_sessionHandle;

#ifdef ETW_LOGGER_MT
    CRITICAL_SECTION _cs;
#endif

    LogDataTrace m_log;
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

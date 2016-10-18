#pragma once

#include <tchar.h>

#include <string>

#include <Windows.h>
#include <guiddef.h>

namespace taoetw {

#define ETW_LOGGER_MAX_LOG_SIZE (60*1024)

// 一定要跟 etwlogger.h 中的定义一样
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

// 界面需要使用更多的数据
struct LogDataUI : LogData
{
    typedef std::basic_string<TCHAR> string;

    unsigned int pid;       // 进程标识
    unsigned int tid;       // 线程标识
    unsigned char level;    // 日志等级

    TCHAR id[22];
    string strTime;
    string strLine;
    string strPid;
    string strTid;

    string strProject;

    int offset_of_file;

    void* strLevel;
};



}

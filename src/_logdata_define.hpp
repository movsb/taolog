#pragma once

#include <tchar.h>

#include <string>

#include <Windows.h>
#include <guiddef.h>

namespace taoetw {

#define ETW_LOGGER_MAX_LOG_SIZE (60*1024)

// 一定要跟 etwlogger.h 中的定义一样
#pragma pack(push,1)
struct LogData
{
    GUID guid;              // 生成者 GUID
    SYSTEMTIME time;        // 时间戳
    unsigned int line;      // 行号
    unsigned int cch;      // 字符数（包含null）
    wchar_t file[1024];     // 文件
    wchar_t func[1024];     // 函数
    wchar_t text[ETW_LOGGER_MAX_LOG_SIZE];      // 日志
};
#pragma pack(pop)

// 界面需要使用更多的数据
struct LogDataUI : LogData
{
    typedef std::basic_string<TCHAR> string;

    static constexpr int data_cols = 10;

    string to_string() const
    {
        TCHAR tmp[128];
        string str = text;

        str += L"\r\n\r\n" + string(80, L'-') + L"\r\n\r\n";
        _swprintf(tmp, L"编号：%s\r\n进程：%d\r\n线程：%d\r\n", id, pid, tid);
        str += tmp;

        auto& t = time;
        _swprintf(tmp, L"时间：%d-%02d-%02d %02d:%02d:%02d:%03d\r\n",
            t.wYear, t.wMonth, t.wDay,
            t.wHour, t.wMinute, t.wSecond, t.wMilliseconds
        );
        str += tmp;

        str += L"项目：" + strProject + L"\r\n";
        str += string(L"文件：") + file + L"\r\n";
        str += string(L"函数：") + func + L"\r\n";
        str += L"行号：" + strLine + L"\r\n";
        str += L"等级：" + std::to_wstring(level) + L"\r\n";

        return std::move(str);
    }

    // 返回列数
    int size() const
    {
        return data_cols;
    }

    inline const wchar_t* operator[](int i)
    {
        // 返回 "" 而不是 null，以防错误
        if (i<0 || i > data_cols - 1)
            return L"";

        const wchar_t* value = L"";
        const LogDataUI* evt = this;

        switch(i)
        {
        case 0: value = evt->id;                            break;
        case 1: value = evt->strTime.c_str();               break;
        case 2: value = evt->strPid.c_str();                break;
        case 3: value = evt->strTid.c_str();                break;
        case 4: value = evt->strProject.c_str();            break;
        case 5: value = evt->file + evt->offset_of_file;    break;
        case 6: value = evt->func;                          break;
        case 7: value = evt->strLine.c_str();               break;
        case 8: value = evt->strLevel->c_str();             break;
        case 9: value = evt->text;                          break;
        }

        return value;
    }
    
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

    string* strLevel;
};



}

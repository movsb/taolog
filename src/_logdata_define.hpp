#pragma once

namespace taoetw {

// #define ETW_LOGGER_MAX_LOG_SIZE (50*1024)

enum class ETW_LOGGER_FLAG {
    ETW_LOGGER_FLAG_UNICODE = 1,
};


// 一定要跟 etwlogger.h 中的定义整体一致
// 除：
//      没有最后一个元素
//      字符集可能不一样
#pragma pack(push,1)
struct LogData
{
    UINT            flags;          // 日志相关的标记位
    GUID            guid;           // 生成者 GUID
    SYSTEMTIME      time;           // 时间戳
    unsigned int    line;           // 行号
    unsigned int    cch;            // 字符数（包含null）
    wchar_t         file[260];     // 文件
    wchar_t         func[260];     // 函数
};
#pragma pack(pop)

// 界面需要使用更多的数据
struct LogDataUI : LogData
{
    typedef std::basic_string<TCHAR> string;
    typedef std::basic_stringstream<TCHAR> stringstream;
    typedef std::function<const wchar_t*(int i)> fnGetColumnName;

    static constexpr int data_cols = 10;

    static constexpr bool should_escape[data_cols] = {false,false,false,false,true,true,false,false,false,true};

    string to_string(fnGetColumnName get_column_name) const
    {
        TCHAR tmp[128];
        stringstream ss;

        auto gc = get_column_name;

        ss << strText;
        ss << L"\r\n\r\n" << string(50, L'-') << L"\r\n\r\n";
        ss << gc(0) << L"：" << id << L"\r\n";
        ss << gc(2) << L"：" << pid << L"\r\n";
        ss << gc(3) << L"：" << tid << L"\r\n";

        auto& t = time;
        _snwprintf(tmp, _countof(tmp), L"%s：%d-%02d-%02d %02d:%02d:%02d:%03d\r\n",
            gc(1),
            t.wYear, t.wMonth, t.wDay,
            t.wHour, t.wMinute, t.wSecond, t.wMilliseconds
        );

        ss << tmp;

        ss << gc(4) << L"：" << strProject   << L"\r\n";
        ss << gc(5) << L"：" << file         << L"\r\n";
        ss << gc(6) << L"：" << func         << L"\r\n";
        ss << gc(7) << L"：" << strLine      << L"\r\n";
        ss << gc(8) << L"：" << *strLevel    << L"\r\n";

        return std::move(ss.str());
    }

    inline const wchar_t* operator[](int i)
    {
        // 返回 "" 而不是 null，以防错误
        if (i<0 || i > data_cols - 1)
            return L"";

        const wchar_t* value = L"";

        switch(i)
        {
        case 0: value = id;                            break;
        case 1: value = strTime.c_str();               break;
        case 2: value = strPid.c_str();                break;
        case 3: value = strTid.c_str();                break;
        case 4: value = strProject.c_str();            break;
        case 5: value = file + offset_of_file;         break;
        case 6: value = func;                          break;
        case 7: value = strLine.c_str();               break;
        case 8: value = strLevel->c_str();             break;
        case 9: value = strText.c_str();               break;
        }

        return value;
    }

    USHORT          version;    // 日志版本
    string          strText;    // 日志，这个比较特殊，和原结构体并不同
    
    unsigned int    pid;        // 进程标识
    unsigned int    tid;        // 线程标识
    unsigned char   level;      // 日志等级

    TCHAR id[22];
    string strTime;
    string strLine;
    string strPid;
    string strTid;

    string strProject;

    int offset_of_file;

    string* strLevel;
};

typedef std::shared_ptr<LogDataUI> LogDataUIPtr;

}

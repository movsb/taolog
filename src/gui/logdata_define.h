#pragma once

namespace taolog {

// #define ETW_LOGGER_MAX_LOG_SIZE (50*1024)

enum class ETW_LOGGER_FLAG {
    ETW_LOGGER_FLAG_UNICODE = 1,
    ETW_LOGGER_FLAG_DBGVIEW = 2,
};


// 一定要跟 etwlogger.h 中的定义整体一致
// 除：
//      没有最后一个元素
//      字符集可能不一样
#pragma pack(push,1)
struct LogData
{
    UCHAR           version;        // 日志版本
    int             pid;            // 进程编号
    int             tid;            // 线程编号
    UCHAR           level;          // 日志等级
    UINT            flags;          // 日志相关的标记位
    GUID            guid;           // 生成者 GUID
    SYSTEMTIME      time;           // 时间戳
    unsigned int    line;           // 行号
    unsigned int    cch;            // 字符数（包含null）
    wchar_t         file[260];      // 文件
    wchar_t         func[260];      // 函数
};
#pragma pack(pop)

// 界面需要使用更多的数据
struct LogDataUI : LogData
{
    typedef std::basic_string<TCHAR> string;
    typedef std::basic_stringstream<TCHAR> stringstream;
    typedef std::function<const wchar_t*(int i)> fnGetColumnName;

    inline const wchar_t* operator[](int i) const
    {
        const wchar_t* value = L"";

        if(flags & (int)ETW_LOGGER_FLAG::ETW_LOGGER_FLAG_DBGVIEW)
        {
            switch(i)
            {
            case 0: value = id;                     break;
            case 1: value = strTime;                break;
            case 2: value = strPid;                 break;
            case 9: value = strText.c_str();        break;
            }
        }
        else
        {
            switch(i)
            {
            case 0: value = id;                     break;
            case 1: value = strTime;                break;
            case 2: value = strPid;                 break;
            case 3: value = strTid;                 break;
            case 4: value = strProject->c_str();    break;
            case 5: value = file + offset_of_file;  break;
            case 6: value = func;                   break;
            case 7: value = strLine;                break;
            case 8: value = strLevel->c_str();      break;
            case 9: value = strText.c_str();        break;
            }
        }

        return value;
    }

    static void a2u(const char* a, wchar_t* u, int c) {
        ::MultiByteToWideChar(CP_ACP, 0, a, -1, u, c);
    }

    static constexpr int cols() { return 10; }
    std::wostringstream& to_html_tr(std::wostringstream& os) const;
    string to_string(fnGetColumnName get_column_name) const;
    void to_luaobj(lua_State* L) const;
    string to_tip(fnGetColumnName get_column_name) const;

    static LogDataUI* from_dbgview(DWORD pid, const char* str, void* place = nullptr);
    static LogDataUI* from_logdata(LogData* log, void* place = nullptr);

    string          strText;        // 日志，这个比较特殊，和原结构体并不同

    TCHAR id[22];
    TCHAR strTime[10+1+12+1];       // 2016-12-23 10:52:28:123
    TCHAR strLine[11];              // 4294967295
    TCHAR strPid[11];
    TCHAR strTid[11];

    string* strProject;

    int offset_of_file;

    string* strLevel;
};

typedef LogDataUI* LogDataUIPtr;

//---------------------------------------------------------------------------------

const char* const lualogdata_metaname = "lualogdata_metaname";

int logdata_openlua(lua_State* L);

}

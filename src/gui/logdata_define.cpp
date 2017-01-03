#include "stdafx.h"

#include <codecvt>

#include "logdata_define.h"

namespace taolog {

std::wostringstream& LogDataUI::to_html_tr(std::wostringstream& os) const
{
    static constexpr bool should_escape[cols()] = {false,false,false,false,true,true,false,false,false,true};

    static auto escape = [](const wchar_t* s) {
        std::wstring r(s);

        r = std::regex_replace(r, std::wregex(L"&"), L"&amp;");
        r = std::regex_replace(r, std::wregex(L"<"), L"&lt;");
        r = std::regex_replace(r, std::wregex(L">"), L"&gt;");

        return r;
    };

    int begin = 0, end = cols();

    os << L"<tr>";

    for(int i = begin; i < end; ++i) {
        os << L"<td>" << (should_escape[i] ? escape(operator[](i)) : operator[](i)) << L"</td>";
    }

    os << L"</tr>\n";

    return os;
}

taolog::LogDataUI::string LogDataUI::to_string(fnGetColumnName get_column_name) const
{
    TCHAR tmp[128];
    stringstream ss;

    auto gc = get_column_name;

    ss << strText;
    ss << L"\r\n\r\n" << string(50, L'-') << L"\r\n\r\n";
    ss << gc(0) << L"：" << operator[](0) << L"\r\n";
    ss << gc(2) << L"：" << operator[](2) << L"\r\n";
    ss << gc(3) << L"：" << operator[](3) << L"\r\n";

    auto& t = time;
    _snwprintf(tmp, _countof(tmp), L"%s：%d-%02d-%02d %02d:%02d:%02d:%03d\r\n",
        gc(1),
        t.wYear, t.wMonth, t.wDay,
        t.wHour, t.wMinute, t.wSecond, t.wMilliseconds
    );

    ss << tmp;

    ss << gc(4) << L"：" << operator[](4) << L"\r\n";
    ss << gc(5) << L"：" << file << L"\r\n"; // 现在完整路径
    ss << gc(6) << L"：" << operator[](6) << L"\r\n";
    ss << gc(7) << L"：" << operator[](7) << L"\r\n";
    ss << gc(8) << L"：" << operator[](8) << L"\r\n";

    return std::move(ss.str());
}

LogDataUI* LogDataUI::from_dbgview(DWORD pid, const char* str, void* place /*= nullptr*/)
{
    auto logui = place ? new (place) LogDataUI : new LogDataUI;

    ::memset(logui, 0, sizeof(LogData));

    logui->pid = (int)pid;
    logui->level = TRACE_LEVEL_INFORMATION;
    logui->flags |= (int)ETW_LOGGER_FLAG::ETW_LOGGER_FLAG_DBGVIEW;

    _snwprintf(logui->strPid, _countof(logui->strPid), L"%d", logui->pid);

    {
        wchar_t buf[4096]; // enough
        buf[0] = L'\0';
        if(int cch = ::MultiByteToWideChar(CP_ACP, 0, str, -1, &buf[0], _countof(buf))) {
            auto p = &buf[cch - 2]; // & !'\0';

            while(p >= buf && (*p == L'\r' || *p == L'\n'))
                --p;

            logui->strText.assign(buf, p + 1);
        }
    }

    {
        auto& buf = logui->strTime;
        SYSTEMTIME t;
        ::GetLocalTime(&t);

        _sntprintf(&buf[0], _countof(buf),
            _T("%02d:%02d:%02d:%03d"),
            t.wHour, t.wMinute, t.wSecond, t.wMilliseconds
        );
    }

    return logui;
}

LogDataUI* LogDataUI::from_logdata(LogData* log, void* place /*= nullptr*/)
{
    const auto& log_data = *log;
    auto log_ui = place ? new (place) LogDataUI : new LogDataUI;

    // 日志正文前面的部分都是一样的
    // 除了日志输出方长度固定，接收方长度不固定
    ::memcpy(log_ui, &log_data, sizeof(LogData));

    bool bIsUnicode = log_ui->flags & (int)ETW_LOGGER_FLAG::ETW_LOGGER_FLAG_UNICODE;

    // 如果是非 Unicode 则需要转换
    // 包含 file, func, text
    if(!bIsUnicode) {
        char filebuf[_countof(log_ui->file)];
        ::strcpy(filebuf, (char*)log_ui->file);
        a2u(filebuf, log_ui->file, _countof(log_ui->file));

        char funcbuf[_countof(log_ui->func)];
        ::strcpy(funcbuf, (char*)log_ui->func);
        a2u(funcbuf, log_ui->func, _countof(log_ui->func));
    }

    // 拷贝日志正文（cch包括 '\0'，因而始终大于零
    if(bIsUnicode) {
        const wchar_t* pText = (const wchar_t*)((char*)&log_data + sizeof(LogData));
        assert(pText[log_ui->cch - 1] == 0);
        log_ui->strText.assign(pText, log_ui->cch - 1);
    }
    else {
        const char* pText = (const char*)((char*)&log_data + sizeof(LogData));
        assert(pText[log_ui->cch - 1] == 0);

        if(log_ui->cch > 4096) {
            auto p = std::make_unique<char[]>(log_ui->cch);
            memcpy(p.get(), pText, log_ui->cch);
            ::MultiByteToWideChar(CP_ACP, 0, p.get(), -1, (wchar_t*)pText, log_ui->cch);
            log_ui->strText.assign((wchar_t*)pText);
        }
        else {
            char buf[4096];
            memcpy(buf, pText, log_ui->cch);
            ::MultiByteToWideChar(CP_ACP, 0, buf, -1, (wchar_t*)pText, log_ui->cch);
            log_ui->strText.assign((wchar_t*)pText);
        }
    }

    _snwprintf(log_ui->strPid, _countof(log_ui->strPid), L"%d", log_ui->pid);
    _snwprintf(log_ui->strTid, _countof(log_ui->strTid), L"%d", log_ui->tid);
    _snwprintf(log_ui->strLine, _countof(log_ui->strLine), L"%d", log_ui->line);

    {
        auto& buf = log_ui->strTime;
        auto& t = log_ui->time;

        _sntprintf(&buf[0], _countof(buf),
            _T("%02d:%02d:%02d:%03d"),
            t.wHour, t.wMinute, t.wSecond, t.wMilliseconds
        );
    }

    return log_ui;
}

//---------------------------------------------------------------------------------

namespace lualogdata {

typedef std::wstring_convert<std::codecvt_utf8_utf16<unsigned short>, unsigned short> U8U16Cvt;

std::string to_utf8(const std::wstring& s)
{
    return (const char*)U8U16Cvt().to_bytes((const unsigned short*)s.c_str()).c_str();
}

struct LuaObject
{
    const LogDataUI* log;
};

static LuaObject* check_object(lua_State* L)
{
    return reinterpret_cast<LuaObject*>(luaL_checkudata(L, 1, lualogdata_metaname));
}

static int logdata_id(lua_State* L)
{
    auto luaobj = check_object(L);
    auto id = std::stoi(luaobj->log->id);

    lua_pushinteger(L, id);

    return 1;
}

static int logdata_time(lua_State* L)
{
    auto luaobj = check_object(L);
    auto time = to_utf8(luaobj->log->strTime);

    lua_pushlstring(L, time.c_str(), time.size());

    return 1;
}

static int logdata_pid(lua_State* L)
{
    auto luaobj = check_object(L);
    auto pid = luaobj->log->pid;

    lua_pushinteger(L, pid);

    return 1;
}

static int logdata_tid(lua_State* L)
{
    auto luaobj = check_object(L);
    auto tid = luaobj->log->tid;

    lua_pushinteger(L, tid);

    return 1;
}

static int logdata_mod(lua_State* L)
{
    auto luaobj = check_object(L);
    auto mod = to_utf8(*luaobj->log->strProject);

    lua_pushlstring(L, mod.c_str(), mod.size());

    return 1;
}

static int logdata_file(lua_State* L)
{
    auto luaobj = check_object(L);
    auto file = to_utf8(luaobj->log->file);

    lua_pushlstring(L, file.c_str(), file.size());

    return 1;
}

static int logdata_func(lua_State* L)
{
    auto luaobj = check_object(L);
    auto func = to_utf8(luaobj->log->func);

    lua_pushlstring(L, func.c_str(), func.size());

    return 1;
}

static int logdata_line(lua_State* L)
{
    auto luaobj = check_object(L);
    auto line = luaobj->log->line;

    lua_pushinteger(L, line);

    return 1;
}

static int logdata_level(lua_State* L)
{
    auto luaobj = check_object(L);
    auto level = luaobj->log->level;

    lua_pushinteger(L, level);

    return 1;
}

static int logdata_log(lua_State* L)
{
    auto luaobj = check_object(L);
    auto log = to_utf8(luaobj->log->strText);

    lua_pushlstring(L, log.c_str(), log.size());

    return 1;
}

static int logdata_cid(lua_State* L)
{
    check_object(L);

    int top = lua_gettop(L);

    static std::map<std::string, int> sid2cid {
        {"id",      0},
        {"time",    1},
        {"pid",     2},
        {"tid",     3},
        {"mod",     4},
        {"file",    5},
        {"func",    6},
        {"line",    7},
        {"level",   8},
        {"log",     9},
    };

    for(int i = 2; i <= top; ++i) {
        auto sid = std::string(luaL_checkstring(L, i));
        auto it = sid2cid.find(sid);

        if(it != sid2cid.cend())
            lua_pushinteger(L, it->second);
        else
            lua_pushinteger(L, -1);
    }

    return top - 1;
}

static const luaL_Reg lualogdata_lib[] = {
    {"cid",     logdata_cid},

    {"id",      logdata_id},
    {"time",    logdata_time},
    {"pid",     logdata_pid},
    {"tid",     logdata_tid},
    {"mod",     logdata_mod},
    {"file",    logdata_file},
    {"func",    logdata_func},
    {"line",    logdata_line},
    {"level",   logdata_level},
    {"log",     logdata_log},
    {nullptr,   nullptr}
};

} // namespace lualogdata

void LogDataUI::to_luaobj(lua_State* L) const
{
    auto luaobj = reinterpret_cast<lualogdata::LuaObject*>(lua_newuserdata(L, sizeof(lualogdata::LuaObject)));
    luaL_setmetatable(L, lualogdata_metaname);

    luaobj->log = this;
}

int logdata_openlua(lua_State* L)
{
    luaL_newmetatable(L, lualogdata_metaname);
    luaL_setfuncs(L, lualogdata::lualogdata_lib, 0);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    return 0;
}

} // namespace taolog

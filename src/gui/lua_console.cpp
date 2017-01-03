#include "stdafx.h"

#include "misc/utils.h"
#include "misc/config.h"

extern "C" {
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#include "lua_console.h"

namespace taolog {

LuaConsoleWindow* __this;

int LuaConsoleWindow::LuaPrint(lua_State* L)
{
  int n = lua_gettop(L);  /* number of arguments */
  int i;
  lua_getglobal(L, "tostring");
  for (i=1; i<=n; i++) {
    const char *s;
    size_t l;
    lua_pushvalue(L, -1);  /* function to be called */
    lua_pushvalue(L, i);   /* value to print */
    lua_call(L, 1, 1);
    s = lua_tolstring(L, -1, &l);  /* get result */
    if (s == NULL)
      return luaL_error(L, "'tostring' must return a string to 'print'");
    if (i>1) __this->append_result("\t", 1);
    __this->append_result(s, l);
    lua_pop(L, 1);  /* pop result */
  }
  lua_writeline();
  return 0;
}

void LuaConsoleWindow::execute()
{
    auto script = g_config.us(_edt_script->get_text());

    int ret = luaL_dostring(_L, script.c_str());

    if(ret != LUA_OK) {
        std::wstring err;
        // lua 的 luaO_chunkid 函数会截取脚本的部分用作错误提示
        // 然而其不支持 utf-8，可能导致一个完整的字符被截断
        // 不完整的字符调用 codecvt 会抛异常。
        try {
            err = g_config.ws(lua_tostring(_L, -1));
        }
        catch(...) {
            err = L"LUA解码错误（内部错误）。";
        }
        _root->find<taowin::control>(L"bot")->set_visible(false);
        _root->find<taowin::control>(L"stabar")->set_visible(true);
        _lbl_status->set_text(err.c_str());
        msgbox(err.c_str(), MB_ICONERROR, L"LUA脚本错误");
        _edt_script->focus();
        lua_pop(_L, 1);
    }
    else {
        _root->find<taowin::control>(L"bot")->set_visible(true);
        _root->find<taowin::control>(L"stabar")->set_visible(false);
    }
}

void LuaConsoleWindow::append_result(const char* s, int len)
{
    auto ws = g_config.ws(std::string(s, len));
    ws = std::regex_replace(ws, std::wregex(LR"(\r?\n)"), L"\r\n");
    _edt_result->append(ws.c_str());
}

LPCTSTR LuaConsoleWindow::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="LUA 控制台" size="750,450">
    <res>
        <font name="default" face="微软雅黑" size="12"/>
        <font name="14" face="微软雅黑" size="14"/>
        <font name="consolas" face="Consolas" size="12"/>
    </res>
    <root>
        <vertical padding="5,5,5,0">
            <horizontal>
                <vertical>
                    <label height="20" text="输入 LUA 脚本（F5执行，F6清空）：" />
                    <edit name="script" font="consolas" style="multiline,vscroll,hscroll,wantreturn" exstyle="clientedge"/>
                </vertical>
                <control width="5" />
                <vertical>
                    <label height="20" text="结果（F7清空）：" />
                    <edit name="result" font="consolas" style="multiline,vscroll,hscroll,wantreturn" exstyle="clientedge"/>
                </vertical>
            </horizontal>
            <control name="bot" height="5" />
            <horizontal name="stabar" height="20">
                <label style="centerimage,center" width="40" text="错误：" />
                <label font="consolas" style="centerimage" name="status" />
            </horizontal>
        </vertical>
    </root>
</window>
)tw";
    return json;
}

LRESULT LuaConsoleWindow::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
    case WM_CREATE:
    {
        _L = luaL_newstate();
        luaL_openlibs(_L);
        lua_pushcfunction(_L, LuaConsoleWindow::LuaPrint);
        lua_setglobal(_L, "print");

        _edt_script = _root->find<taowin::edit>(L"script");
        _edt_result = _root->find<taowin::edit>(L"result");
        _lbl_status = _root->find<taowin::label>(L"status");

        _root->find<taowin::control>(L"bot")->set_visible(true);
        _root->find<taowin::control>(L"stabar")->set_visible(false);

        _edt_script->focus();

        return 0;
    }
    case WM_DESTROY:
        lua_close(_L);
        break;
    case WM_ACTIVATE:
        if(wparam != WA_INACTIVE) {
            __this = this;
        }
        break;
    }
    return __super::handle_message(umsg, wparam, lparam);
}

bool LuaConsoleWindow::filter_message(MSG* msg) {
    if(msg->message == WM_KEYDOWN){
        switch(msg->wParam)
        {
        case VK_F5:
            execute();
            return true;
        case VK_F6:
            _edt_script->set_text(L"");
            return true;
        case VK_F7:
            _edt_result->set_text(L"");
            return true;
        case VK_TAB:
            if(::GetFocus() == _edt_script->hwnd() || ::GetFocus() == _edt_result->hwnd())
                return false;
            break;
        case 'A':
            if(::GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                HWND hFocus = ::GetFocus();
                if(hFocus == _edt_result->hwnd())
                    _edt_result->set_sel(0, -1);
                else if(hFocus == _edt_script->hwnd())
                    _edt_script->set_sel(0, -1);
            }
            break;
        case VK_ESCAPE:
        case VK_RETURN:
            if (filter_special_key(msg->wParam))
                return true;
            // pass through
        default:
            // I don't want IsDialogMessage to process VK_ESCAPE, because it produces a WM_COMMAND
            // menu message with id == 2. It is undocumented.
            if(::IsDialogMessage(_hwnd, msg))
                return true;
            break;
        }
    }
    return false;
}

}

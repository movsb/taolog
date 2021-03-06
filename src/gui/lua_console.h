#pragma once

struct lua_State;

namespace taolog {

class LuaConsoleWindow : public taowin::WindowCreator
{
public:
    LuaConsoleWindow()
    {
    }

    ~LuaConsoleWindow()
    {
    }

protected:
    taowin::TextBox*    _edt_script;
    taowin::TextBox*    _edt_result;
    taowin::Label*   _lbl_status;

    lua_State*      _L;

protected:
    void execute();
    void append_result(const char* s, int len);
    static int LuaPrint(lua_State* L);
    static int LuaOSExit(lua_State* L);

protected:
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual void on_final_message() override { __super::on_final_message(); delete this; }
    virtual bool filter_message(MSG* msg) override;
};

}

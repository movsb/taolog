#pragma once

#include <taowin/src/tw_taowin.h>

#include "config.h"

namespace taoetw {

struct Column {
    std::wstring name;
    bool show;
    int width;

    Column(const wchar_t* name_, bool show_, int width_)
        : name(name_)
        , show(show_)
        , width(width_)
    {

    }

    json11::Json to_json() const
    {
        return json11::Json::object {
            {"name", g_config.us(name)},
            {"show", show},
            {"width", width},
        };
    }
};

typedef std::function<void(int i)> OnToggleCallback;
typedef std::vector<Column> ColumnContainer;

class ColumnSelection : public taowin::window_creator
{
public:
    ColumnSelection(ColumnContainer& cols)
        : _columns(cols)
    { }

    void OnToggle(OnToggleCallback fn) { _on_toggle = fn; }

protected:
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual LRESULT on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr);
    virtual void on_final_message() override { __super::on_final_message(); delete this; }

protected:
    OnToggleCallback    _on_toggle;
    ColumnContainer&    _columns;
};

} // namespace taoetw

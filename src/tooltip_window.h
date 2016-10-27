#pragma once

namespace taoetw {

class TooltipWindow : public taowin::window_creator
{
public:
    TooltipWindow()
    {
    }

    void popup(const wchar_t* str, HFONT font);
    bool showing() const { return !!::IsWindowVisible(_hwnd); }

protected:
    virtual void get_metas(WindowMeta* metas) override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual void on_final_message() override { __super::on_final_message(); delete this; }

protected:
    HFONT           _font;
    POINT           _pt;
    const wchar_t*  _text;
};



}

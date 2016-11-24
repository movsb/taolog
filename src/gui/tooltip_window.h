#pragma once

namespace taoetw {

class TooltipWindow : public taowin::window_creator
{
    static const int padding = 5;
    static const int offset = 15;

public:
    TooltipWindow()
    {
    }

    void popup(const wchar_t* str);
    bool showing() const { return !!::IsWindowVisible(_hwnd); }
    void set_font(HFONT font) { _font = font; }
    void hide();

protected:
    virtual void get_metas(WindowMeta* metas) override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual void on_final_message() override { __super::on_final_message(); delete this; }

    void adjust_window_pos(const taowin::Rect& calc);

protected:
    HFONT           _font;
    POINT           _pt;
    taowin::Rect    _pos;
    const wchar_t*  _text;
};



}

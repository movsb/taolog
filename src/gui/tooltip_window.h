#pragma once

namespace taoetw {

class TooltipWindow : public taowin::window_creator
{
    static const int padding = 5;
    static const int offset = 15;

    struct Div
    {
        int                 width;
        int                 height;
        std::wstring        text;
    };

    struct Line
    {
        int                 width;
        int                 height;
        std::vector<Div>    divs;
    };

public:
    TooltipWindow()
    {
    }

    void format(const wchar_t* fmt);
    void popup(const wchar_t* str);
    void format(const std::wstring& s) { return format(s.c_str()); }
    void popup(const std::wstring& s) { return popup(s.c_str()); }
    bool showing() const { return !!::IsWindowVisible(_hwnd); }
    void set_font(HFONT font) { _font = font; }
    void hide();

protected:
    virtual void get_metas(WindowMeta* metas) override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual void on_final_message() override { __super::on_final_message(); delete this; }

    void adjust_window_pos(const taowin::Rect& calc);

    void parse(const wchar_t* fmt);
    taowin::Rect calc_pos();
    void draw_it(HDC hdc);

protected:
    HFONT               _font;
    POINT               _pt;
    taowin::Rect        _pos;
    const wchar_t*      _text;
    std::vector<Line>   _lines;

};



}

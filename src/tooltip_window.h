#pragma once

namespace taoetw {

class TooltipWindow : public taowin::window_creator
{
public:
    TooltipWindow(const wchar_t* text, HFONT font)
        : _text(text)
        , _font(font)
    {
    }

    void on_destroy(std::function<void()> fn) { _on_destroy = fn; }

protected:
    virtual void get_metas(WindowMeta* metas) override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual void on_final_message() override { __super::on_final_message(); delete this; }

protected:
    std::function<void()> _on_destroy;
    HFONT           _font;
    POINT           _pt;
    const wchar_t*  _text;
};



}

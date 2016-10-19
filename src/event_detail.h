#pragma once

#include <taowin/src/tw_taowin.h>

#include "_logdata_define.hpp"

namespace taoetw {

class EventDetail : public taowin::window_creator
{
private:
    LogDataUIPtr _log;
    COLORREF _fg, _bg;
    HBRUSH _hbrBk;
    HWND _hText;

public:
    EventDetail(LogDataUIPtr log, COLORREF fg = RGB(0, 0, 0), COLORREF bg = RGB(255, 255, 255))
        : _log(log)
        , _fg(fg)
        , _bg(bg)
    {
        _hbrBk = ::CreateSolidBrush(_bg);
    }

    ~EventDetail()
    {
        ::DeleteObject(_hbrBk);
    }

protected:
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual void on_final_message() override { __super::on_final_message(); delete this; }
};

}

#include "stdafx.h"

#include "tooltip_window.h"

namespace taoetw {

void TooltipWindow::format(const wchar_t* fmt)
{
    _text = nullptr;
    _lines.clear();
    parse(fmt);
    auto rc = calc_pos();
    adjust_window_pos(rc);
    ::ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
    ::SetTimer(_hwnd, 1, 250, 0);
}

void TooltipWindow::popup(const wchar_t* str)
{
    _text = str;

    taowin::Rect rc {0,0,::GetSystemMetrics(SM_CXSCREEN),0};
    HDC hdc = ::GetDC(_hwnd);
    HFONT hOldFont = (HFONT)::SelectObject(hdc, _font);
    ::DrawText(hdc, _text, -1, &rc, DT_CALCRECT|DT_NOPREFIX|DT_WORDBREAK|DT_EDITCONTROL);
    ::SelectObject(hdc, hOldFont);
    ::ReleaseDC(_hwnd, hdc);
    adjust_window_pos(rc);
    ::ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
    ::SetTimer(_hwnd, 1, 250, 0);
}

void TooltipWindow::hide()
{
    ::KillTimer(_hwnd, 1);

    taowin::Rect rcClient;
    ::GetClientRect(_hwnd, &rcClient);
    HDC hdc = ::GetDC(_hwnd);
    ::FillRect(hdc, &rcClient, GetStockBrush(WHITE_BRUSH));
    ::ReleaseDC(_hwnd, hdc);
    ::ShowWindow(_hwnd, SW_HIDE);
}

void TooltipWindow::get_metas(WindowMeta* metas)
{
    __super::get_metas(metas);
    metas->style = WS_POPUP;
    metas->exstyle = WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED;
}

LRESULT TooltipWindow::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch(umsg)
    {
    case WM_CREATE:
    {
        ::SetLayeredWindowAttributes(_hwnd, 0, 245, LWA_ALPHA);

        break;
    }
    case WM_TIMER:
    {
        if(wparam == 1) {
            POINT pt;
            taowin::Rect rc;
            ::GetCursorPos(&pt);
            ::GetWindowRect(_hwnd, &rc);

            if((abs(pt.x - _pt.x) > offset || abs(pt.y - _pt.y) > offset)
                && !::PtInRect(&rc, pt)
            )
            {
                hide();
            }
            return 0;
        }

        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = ::BeginPaint(_hwnd, &ps);

        HFONT hOldFont = (HFONT)::SelectObject(hdc, _font);

        taowin::Rect rc;
        ::GetClientRect(_hwnd, &rc);

        HBRUSH hBrush = ::CreateSolidBrush(RGB(255, 255, 255));
        ::FillRect(hdc, &rc, hBrush);
        ::DeleteBrush(hBrush);

        ::SelectObject(hdc, GetStockBrush(NULL_BRUSH));
        Rectangle(hdc, 0, 0, rc.right, rc.bottom);

        ::SetBkMode(hdc, TRANSPARENT);

        rc.deflate(padding, padding);

        if(_text) {
            ::DrawText(hdc, _text, -1, &rc, DT_NOPREFIX | DT_WORDBREAK | DT_EDITCONTROL);
        }
        else {
            int Y = rc.top;
            int X = rc.left;

            for(const auto& line : _lines) {
                for(const auto& d : line.divs) {
                    taowin::Rect rc {X, Y, X + d.width, Y + d.height};
                    ::DrawText(hdc, d.text.c_str(), -1, &rc, DT_NOPREFIX | DT_WORDBREAK | DT_EDITCONTROL);
                    X += d.width;
                }

                X = rc.left;
                Y += line.height;
            }
        }

        ::SelectObject(hdc, hOldFont);

        ::EndPaint(_hwnd, &ps);

        break;
    }
    }
    return __super::handle_message(umsg, wparam, lparam);
}

// 调整窗口的最终位置
void TooltipWindow::adjust_window_pos(const taowin::Rect& calc)
{
    // calc 是计算出的文字所占据的大小
    taowin::Rect rc(calc);

    // 加上文字外边距
    rc.inflate(padding, padding);

    // 屏幕信息
    taowin::Rect rcScreen;
    {
        HMONITOR hMonitor = ::MonitorFromPoint(_pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info = {sizeof(info)};

        if(hMonitor && ::GetMonitorInfo(hMonitor, &info)) {
            rcScreen = info.rcWork;
        }
        else {
            rcScreen = {0, 0, ::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN)};
        }
    }

    rcScreen.deflate(padding, padding);

    // 限制大小为屏幕大小（工作区）
    if(rc.width() > rcScreen.width())   rc.right = rc.left + rcScreen.width();
    if(rc.height() > rcScreen.height()) rc.bottom = rc.top + rcScreen.height();

    // 根据当前鼠标定位原点
    ::GetCursorPos(&_pt);
    rc.offset(_pt.x + offset, _pt.y + offset);

    // 不能超出屏幕边缘
    int dxscr = rc.right - rcScreen.right;
    int dyscr = rc.bottom - rcScreen.bottom;
    if(dxscr > 0 || dyscr > 0) {
        int dx = std::max(dxscr, 0);
        int dy = std::max(dyscr, 0);
        rc.offset(-dx, -dy);
    }

    ::SetWindowPos(_hwnd, HWND_TOPMOST, rc.left, rc.top, rc.width(), rc.height(), SWP_NOACTIVATE);
}

void TooltipWindow::parse(const wchar_t* f)
{
    for(; *f; )
    {
        Line line;

        for(;;) {
            auto bp = f;

            while(*f && *f != L'\b') {
                ++f;
            }

            auto ep = f;

            if(*f == L'\b') {
                ++f;
                if(*f == L'n') {
                    ++f;
                    Div d;
                    d.text.assign(bp, ep);
                    d.width = 0;
                    line.divs.push_back(std::move(d));
                    if(*f == L'\n') {
                        ++f;
                    }
                    break;
                }
                else if(*f == L'w') {
                    ++f;

                    bool has_brace = *f == L'{';
                    if(has_brace) ++f;

                    auto bbp = f;
                    int width = 0;
                    while('0' <= *f && *f <= '9') {
                        width = width * 10 + *f - '0';
                        ++f;
                    }
                    auto eep = f;
                    if(bbp == eep) {
                        assert("bad width" && 0);
                    }
                    Div d;
                    d.text.assign(bp, ep);
                    d.width = width;
                    line.divs.push_back(std::move(d));

                    if(has_brace && *f == '}') {
                        ++f;
                    }
                }
                else {
                    assert("bad slash" && 0);
                }
            }
            else {
                Div d;
                d.text.assign(bp, ep);
                d.width = 0;
                line.divs.push_back(std::move(d));
                break;
            }
        }

        _lines.push_back(std::move(line));
    }
}

taowin::Rect TooltipWindow::calc_pos()
{
    HDC hdc = ::GetDC(_hwnd);
    HFONT hOldFont = (HFONT)::SelectObject(hdc, _font);
    int max_width = ::GetSystemMetrics(SM_CXSCREEN);

    auto laCalc = [&](int width, Div& d)
    {
        taowin::Rect rc {0,0, width == 0 ? max_width : width, 0};

        ::DrawText(hdc, d.text.c_str(), -1, &rc, DT_CALCRECT|DT_NOPREFIX|DT_WORDBREAK|DT_EDITCONTROL);
        
        if(!width) d.width = rc.width();
        d.height = rc.height();
    };

    int width_all = 0;
    int height_all = 0;

    for(auto& line : _lines) {
        if(line.divs.size() == 1) {
            auto& d = line.divs[0];
            laCalc(0, d);
            line.width = d.width;
            line.height = d.height;
        }
        else {
            int max_height = 0;
            int width_all = 0;
            for(auto& d : line.divs) {
                laCalc(d.width, d);
                if(d.height > max_height)
                    max_height = d.height;
                width_all += d.width;
            }
            line.width = width_all;
            line.height = max_height;
        }
        if(line.width > width_all)
            width_all = line.width;
        height_all += line.height;
    }

    ::SelectObject(hdc, hOldFont);
    ::ReleaseDC(_hwnd, hdc);

    return {0, 0, width_all, height_all};
}

void TooltipWindow::draw_it(HDC hdc)
{
}

}

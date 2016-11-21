#pragma once

#include "log/dbgview.h"

namespace taoetw {

class MiniView : public taowin::window_creator
{
private:
    static RECT winpos;

    struct LogMessage
    {
        enum Value {
            __Start,
            Alloc,
            Dealloc,
            Log,
        };
    };

    static constexpr UINT kLogCmd = WM_USER + 1;

private:
    taowin::listview*   _listview;

protected:
    MemPoolT<LogDataUI, 1024>
                        _logpool;
    EventContainer      _events;
    EventContainerS     _filters;
    ColumnContainer     _columns;
    DebugView           _dbgview;

    EventContainer*     _curflt;

    TooltipWindow*      _tipwnd;

public:
    MiniView()
    {
        _tipwnd = new TooltipWindow;
        _tipwnd->create();
    }

    ~MiniView()
    {
        // 日志结构体是由内存池管理的
        // 所以要强制手动释放，不能等到智能指针析构的时候进行
        _clear_results();

        ::DestroyWindow(_tipwnd->hwnd());
        _tipwnd = nullptr;
    }

    void update();

protected:
    void _clear_results();

protected:
    LRESULT _on_logcmd(LogMessage::Value cmd, LPARAM lParam);

protected:
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual LRESULT control_message(taowin::syscontrol* ctl, UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual LRESULT on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr) override;
    LRESULT _on_custom_draw_listview(NMHDR * hdr);
    LRESULT _on_get_dispinfo(NMHDR* hdr);
    LRESULT _on_drag_column(NMHDR* hdr);
    virtual void on_final_message() override { __super::on_final_message(); delete this; }
};

}

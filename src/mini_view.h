#pragma once

#include "debug_view.h"

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

public:
    MiniView()
    {
    }

    ~MiniView()
    {
    }

    void update();

protected:
    LRESULT _on_logcmd(LogMessage::Value cmd, LPARAM lParam);

protected:
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual LRESULT on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr) override;
    LRESULT _on_custom_draw_listview(NMHDR * hdr);
    virtual void on_final_message() override { __super::on_final_message(); delete this; }
};

}

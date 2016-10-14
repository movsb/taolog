#pragma once

#include <taowin/src/tw_taowin.h>

#include "etwlogger.h"

#include "_module_entry.hpp"

#include "column_selection.h"
#include "module_manager.h"
#include "event_detail.h"

#include "controller.h"
#include "consumer.h"

namespace taoetw {

class MainWindow : public taowin::window_creator
{
public:
    struct ItemColor { COLORREF fg; COLORREF bg; };
    typedef std::map<int, ItemColor> MapColors;

    typedef std::vector<ETWLogger::LogDataUI*> EventContainer;

private:
    static const UINT kDoLog = WM_USER + 1;
    
private:
    taowin::listview*   _listview;

    MapColors           _colors;
    ColumnContainer     _columns;
    // MenuContainer       _menus;
    ModuleContainer     _modules;
    EventContainer      _events;

    Controller          _controller;
    Consumer            _consumer;

    Guid2Module         _guid2mod;

protected:
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual LRESULT on_menu(int id, bool is_accel = false);
    virtual LRESULT on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr) override;

protected:
    void _start();
    void _stop();

    void _init_listview();
    void _init_menu();
    void _view_detail(int i);

    LRESULT _on_create();
    LRESULT _on_log(ETWLogger::LogDataUI* log);
    LRESULT _on_custom_draw_listview(NMHDR* hdr);
    LRESULT _on_get_dispinfo(NMHDR* hdr);
    LRESULT _on_select_column();
    LRESULT _on_drag_column(NMHDR* hdr);

protected:
    void _module_from_guid(const GUID& guid, std::wstring* name, const std::wstring** root);
};

}

#pragma once

#include <regex>

#include <taowin/src/tw_taowin.h>

#include "event_container.h"

namespace taoetw {

class ResultFilter : public taowin::window_creator
{
public:
    typedef std::function<void(std::vector<std::wstring>*)> fnOnGetBases;
    typedef std::function<void(int i)> fnOnDeleteFilter;
    typedef std::function<void(EventContainer* p)> fnOnSetFilter;
    typedef std::function<void(EventContainer* p)> fnOnAddNewFilter;

public:
    ResultFilter(EventContainerS& filters, fnOnGetBases getbases, fnOnDeleteFilter ondelete, fnOnSetFilter onsetfilter, fnOnAddNewFilter onaddnew, EventContainer* curflt)
        : _filters(filters)
        , _on_get_bases(getbases)
        , _on_delete(ondelete)
        , _on_set_filter(onsetfilter)
        , _on_add_new(onaddnew)
        , _current_filter(curflt)
    { }

protected:
    taowin::listview*   _listview;
    taowin::button*     _btn_add;
    taowin::button*     _btn_delete;
    taowin::button*     _btn_all;

protected:
    EventContainerS&    _filters;
    fnOnGetBases        _on_get_bases;
    fnOnDeleteFilter    _on_delete;
    fnOnSetFilter       _on_set_filter;
    fnOnAddNewFilter    _on_add_new;
    EventContainer*     _current_filter;

protected:
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual LRESULT on_notify(HWND hwnd, taowin::control * pc, int code, NMHDR * hdr) override;
    virtual void on_final_message() override { __super::on_final_message(); delete this; }
};

class AddNewFilter : public taowin::window_creator
{
public:
    std::wstring name;
    std::wstring base;
    int          base_int;
    std::wstring rule;

    AddNewFilter(ResultFilter::fnOnGetBases getbases)
        : _on_get_bases(getbases)
    { }

protected:
    std::vector<std::wstring> _bases;
    ResultFilter::fnOnGetBases _on_get_bases;

    taowin::edit*       _name;
    taowin::combobox*   _base;
    taowin::edit*       _rule;
    taowin::button*     _save;
    taowin::button*     _cancel;

protected:
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual LRESULT on_notify(HWND hwnd, taowin::control * pc, int code, NMHDR * hdr) override;
    virtual bool filter_special_key(int vk) override;

    int _on_save();
};

}

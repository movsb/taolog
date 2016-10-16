#pragma once

#include <regex>
#include <unordered_map>

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
    typedef std::function<void(int, std::unordered_map<int, const wchar_t*>*)> fnGetValueList;

public:
    ResultFilter(EventContainerS& filters, fnOnGetBases getbases, fnOnDeleteFilter ondelete, fnOnSetFilter onsetfilter, fnOnAddNewFilter onaddnew, EventContainer* curflt, fnGetValueList getvalues)
        : _filters(filters)
        , _on_get_bases(getbases)
        , _on_delete(ondelete)
        , _on_set_filter(onsetfilter)
        , _on_add_new(onaddnew)
        , _current_filter(curflt)
        , _get_value_list(getvalues)
    {
        _this_instance = this;
    }

    ~ResultFilter()
    {
        _this_instance = nullptr;
    }

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
    fnGetValueList      _get_value_list;

public:
    static ResultFilter* _this_instance;

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
    int          rule2;
    std::wstring str_base_value;
    std::wstring rule;

    AddNewFilter(ResultFilter::fnOnGetBases getbases, ResultFilter::fnGetValueList getvalues)
        : _on_get_bases(getbases)
        , _get_values(getvalues)
    { }

protected:
    std::vector<std::wstring> _bases;
    ResultFilter::fnOnGetBases _on_get_bases;
    std::unordered_map<int, const wchar_t*> _values;
    ResultFilter::fnGetValueList _get_values;

    taowin::edit*       _name;
    taowin::combobox*   _base;
    taowin::combobox*   _rule2;
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

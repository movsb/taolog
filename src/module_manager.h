#pragma once

#include <taowin/src/tw_taowin.h>

#include "_module_entry.hpp"

namespace taoetw {

class ModuleManager : public taowin::window_creator
{
private:
    taowin::listview* _listview;
    std::vector<ModuleEntry*>& _modules;

public:
    ModuleManager(std::vector<ModuleEntry*>& modules)
        : _modules(modules)
    {}

protected:
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual LRESULT on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr) override;
    virtual void on_final_message() override { __super::on_final_message(); delete this; }

protected:
    void _toggle_enable(int i);
    void _modify_item(int i);
    void _delete_item();
    void _add_item();
    bool _has_guid(const GUID& guid);
};


}

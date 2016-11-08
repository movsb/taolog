#pragma once

namespace taoetw {

class ModuleManager : public taowin::window_creator
{
public:
    typedef std::function<bool(ModuleEntry* module, bool enable, std::wstring* err)> fnOnToggleEnable;
    typedef std::function<bool()> fnIsEtwOpen;

protected:
    std::vector<ModuleEntry*>& _modules;
    ModuleLevelMap&            _levels;

    fnOnToggleEnable    _on_toggle;
    fnIsEtwOpen         _get_is_open;

protected:
    taowin::listview*   _listview;
    taowin::button*     _btn_enable;
    taowin::button*     _btn_add;
    taowin::button*     _btn_modify;
    taowin::button*     _btn_delete;
    taowin::button*     _btn_copy;
    taowin::button*     _btn_paste;

public:
    ModuleManager(std::vector<ModuleEntry*>& modules, ModuleLevelMap& levels)
        : _modules(modules)
        , _levels(levels)
    {}

    void on_toggle_enable(fnOnToggleEnable fn) { _on_toggle = fn; }
    void on_get_is_open(fnIsEtwOpen fn) { _get_is_open = fn; }

protected:
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual LRESULT on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr) override;
    virtual void on_final_message() override { __super::on_final_message(); delete this; }

protected:
    void _enable_items(const std::vector<int>& items, int state); // 1: enable, 0: disable, -1: toggle
    void _modify_item(int i);
    void _delete_items(const std::vector<int>& items);
    void _add_item();
    bool _has_guid(const GUID& guid);
    int _get_enable_state_for_items(const std::vector<int>& items);
    void _on_item_state_change();
    void _save_modules();
    void _copy_items();
    bool _paste_items(bool test);
    void _update_pastebtn_status();
};


}

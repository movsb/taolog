#pragma once

namespace taolog {

class ModuleManager : public taowin::window_creator
{
public:
    typedef std::function<bool(ModuleEntry* module, bool enable, std::wstring* err)> fnOnToggleEnable;
    typedef std::function<bool()> fnIsEtwOpen;

    class ModuleDataSource : public taowin::ListViewControl::IDataSource
    {
        typedef std::vector<ModuleEntry*> MODULES;

    public:
        void SetModules(MODULES* modules)
        {
            _modules = modules;
        }

    protected:
        virtual size_t size() const override
        {
            return _modules->size();
        }

        virtual LPCTSTR get(int item, int subitem) const override
        {
            auto& mod = (*_modules)[item];
            const TCHAR* value = _T("");

            switch (subitem)
            {
            case 0: value = mod->name.c_str();                   break;
            case 1: value = mod->enable ? L"已启用" : L"已禁用";  break;
            }

            return value;
        }

    protected:
        MODULES* _modules;
    };

protected:
    std::vector<ModuleEntry*>& _modules;
    ModuleLevelMap&            _levels;
    ModuleDataSource           _data_source;

    fnOnToggleEnable    _on_toggle;
    fnIsEtwOpen         _get_is_open;

protected:
    taowin::ListViewControl*   _listview;
    taowin::button*     _btn_enable;
    taowin::button*     _btn_add;
    taowin::button*     _btn_modify;
    taowin::button*     _btn_delete;
    taowin::button*     _btn_copy;
    taowin::button*     _btn_paste;

    TooltipWindow*      _tipwnd;

public:
    ModuleManager(std::vector<ModuleEntry*>& modules, ModuleLevelMap& levels)
        : _modules(modules)
        , _levels(levels)
    {
        _tipwnd = new TooltipWindow;
    }

    ~ModuleManager()
    {
        _tipwnd = nullptr;
    }

public:
    void on_toggle_enable(fnOnToggleEnable fn) { _on_toggle = fn; }
    void on_get_is_open(fnIsEtwOpen fn) { _get_is_open = fn; }

protected:
    virtual void get_metas(WindowMeta* metas) override;
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual LRESULT control_message(taowin::syscontrol* ctl, UINT umsg, WPARAM wparam, LPARAM lparam);
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

#pragma once

namespace taoetw {

class ModuleEntryEditor : public taowin::window_creator
{
public:
    typedef std::function<void(ModuleEntry* p)> fnOnOk;
    typedef std::function<bool(const GUID& guid, std::wstring* err)> fnOnCheckGuid;

private:
    ModuleEntry* _mod;
    ModuleLevelMap* _levels;
    fnOnOk _onok;
    fnOnCheckGuid _oncheckguid;

    taowin::edit* _name;
    taowin::edit* _guid;
    taowin::edit* _path;
    taowin::combobox* _level;
    taowin::button* _ok;
    taowin::button* _cancel;

public:
    ModuleEntryEditor(ModuleEntry* mod, ModuleLevelMap& levels, fnOnOk onok, fnOnCheckGuid onguid)
        : _mod(mod)
        , _onok(onok)
        , _oncheckguid(onguid)
        , _levels(&levels)
    {}

protected:
    virtual void get_metas(WindowMeta* metas) override;
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual LRESULT on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr);
    virtual void on_final_message() override { __super::on_final_message(); delete this; }
    virtual bool filter_special_key(int vk) override;

protected:
    int _on_ok();
    bool _validate_form();
};

}

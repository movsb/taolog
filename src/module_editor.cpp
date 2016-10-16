#include <unordered_map>

#include <windows.h>
#include <evntrace.h>

#include "module_editor.h"

namespace taoetw {

LPCTSTR ModuleEntryEditor::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="添加模块" size="350,200">
    <res>
        <font name="default" face="微软雅黑" size="12"/>
        <font name="1" face="微软雅黑" size="12"/>
        <font name="consolas" face="Consolas" size="12"/>
    </res>
    <root>
        <vertical>
            <vertical name="container" padding="10,10,10,10" height="144">
                <horizontal height="30" padding="0,3,0,3">
                    <label style="centerimage" text="名字" width="50"/>
                    <edit name="name" style="tabstop" exstyle="clientedge"/>
                </horizontal>
                <horizontal height="30" padding="0,3,0,3">
                    <label style="centerimage" text="GUID" width="50"/>
                    <edit name="guid" style="tabstop" exstyle="clientedge" />
                </horizontal>
                <horizontal height="30" padding="0,3,0,3">
                    <label style="centerimage" text="目录" width="50"/>
                    <edit name="root" style="tabstop" exstyle="clientedge" />
                </horizontal>
                <horizontal height="30" padding="0,3,0,3">
                    <label style="centerimage" text="等级" width="50"/>
                    <combobox name="level" style="tabstop" height="200"/>
                </horizontal>
            </vertical>
            <horizontal height="40" padding="10,4,10,4">
                <control />
                <button name="ok" text="保存" width="50" style="tabstop,default"/>
                <control width="10" />
                <button name="cancel" text="取消" width="50" style="tabstop"/>
            </horizontal>
        </vertical>
    </root>
</window>
)tw";
    return json;
}

LRESULT ModuleEntryEditor::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg) {
    case WM_CREATE:
    {
        ::SetWindowText(_hwnd, !_mod ? L"添加模块" : L"修改模块");

        _name = _root->find<taowin::edit>(L"name");
        _path = _root->find<taowin::edit>(L"root");
        _level = _root->find<taowin::combobox>(L"level");
        _guid = _root->find<taowin::edit>(L"guid");
        _ok = _root->find<taowin::button>(L"ok");
        _cancel = _root->find<taowin::button>(L"cancel");

        for (auto& pair : *_levels) {
            int i = _level->add_string(pair.second.cmt2.c_str());
            _level->set_item_data(i, (void*)pair.first);
        }

        if (!_mod) {
            _level->set_cur_sel(0);
        }
        else {
            _name->set_text(_mod->name.c_str());
            _path->set_text(_mod->root.c_str());

            wchar_t guid[128];
            if (::StringFromGUID2(_mod->guid, &guid[0], _countof(guid))) {
                _guid->set_text(guid);
            }
            else {
                _guid->set_text(L"{00000000-0000-0000-0000-000000000000}");
            }

            for (int i = 0, n = _level->get_count();; i++) {
                if (i < n) {
                    if ((int)_level->get_item_data(i) == _mod->level) {
                        _level->set_cur_sel(i);
                        break;
                    }
                }
                else {
                    _level->set_cur_sel(0);
                    break;
                }
            }
        }

        _name->focus();

        return 0;
    }
    }
    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT ModuleEntryEditor::on_notify(HWND hwnd, taowin::control * pc, int code, NMHDR * hdr)
{
    if (!pc) return 0;

    if (pc->name() == L"ok") {
        _on_ok();
    }
    else if (pc->name() == L"cancel") {
        close();
        return 0;
    }

    return 0;
}

bool ModuleEntryEditor::filter_special_key(int vk)
{
    if (vk == VK_RETURN) {
        _on_ok();
        return true;
    }

    return __super::filter_special_key(vk);
}

int ModuleEntryEditor::_on_ok()
{
    if (!_validate_form())
        return 0;

    auto entry = _mod ? _mod : new ModuleEntry;
    entry->name = _name->get_text();
    entry->root = _path->get_text();
    entry->level = (int)_level->get_item_data(_level->get_cur_sel());
    ::CLSIDFromString(_guid->get_text().c_str(), &entry->guid);
    entry->enable = false;

    _onok(entry);

    close();

    return 0;
}

bool ModuleEntryEditor::_validate_form()
{
    if (_name->get_text() == L"") {
        msgbox(L"模块名字不应为空。", MB_ICONERROR);
        _name->focus();
        return false;
    }

    auto guid = _guid->get_text();
    CLSID clsid;
    if (FAILED(::CLSIDFromString(guid.c_str(), &clsid)) || ::IsEqualGUID(clsid, GUID_NULL)) {
        msgbox(L"无效 GUID 值。", MB_ICONERROR);
        _guid->focus();
        return false;
    }

    std::wstring err;
    if (!_oncheckguid(clsid, &err)) {
        msgbox(err.c_str(), MB_ICONERROR);
        _guid->focus();
        return false;
    }

    return true;
}

}


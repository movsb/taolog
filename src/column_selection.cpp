#include "column_selection.h"

namespace taoetw {

LPCTSTR ColumnSelection::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="±íÍ·" size="512,480">
    <res>
        <font name="default" face="Î¢ÈíÑÅºÚ" size="12"/>
        <font name="1" face="Î¢ÈíÑÅºÚ" size="12"/>
        <font name="consolas" face="Consolas" size="12"/>
    </res>
    <root>
        <vertical name="container" padding="20,20,20,20">

        </vertical>
    </root>
</window>
)tw";
    return json;
}

LRESULT ColumnSelection::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
    case WM_CREATE:
    {
        auto container = _root->find<taowin::vertical>(L"container");

        for (int i = 0; i < (int)_columns.size(); i++) {
            auto check = new taowin::check;
            std::map<taowin::string, taowin::string> attrs;

            attrs[_T("text")] = _columns[i].name.c_str();
            attrs[_T("name")] = std::to_wstring(i).c_str();
            attrs[_T("checked")] = _columns[i].show ? _T("true") : _T("false");

            check->create(_hwnd, attrs, _mgr);
            ::SetWindowLongPtr(check->hwnd(), GWL_USERDATA, LONG(check));
            container->add(check);
        }

        taowin::Rect rc{ 0, 0, 200, (int)_columns.size() * 20 + 50 };
        ::AdjustWindowRectEx(&rc, ::GetWindowLongPtr(_hwnd, GWL_STYLE), !!::GetMenu(_hwnd), ::GetWindowLongPtr(_hwnd, GWL_EXSTYLE));
        ::SetWindowPos(_hwnd, nullptr, 0, 0, rc.width(), rc.height(), SWP_NOMOVE | SWP_NOZORDER);

        return 0;
    }
    }
    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT ColumnSelection::on_notify(HWND hwnd, taowin::control * pc, int code, NMHDR * hdr)
{
    if (!pc) return 0;

    int id = -1;
    ::swscanf(pc->name().c_str(), L"%d", &id);

    if (id >= 0 && id < int(_columns.size())) {
        if (code == BN_CLICKED) {
            int index = _ttoi(pc->name().c_str());
            auto& col = _columns[index];
            col.show = !col.show;
            _on_toggle(index);
            return 0;
        }
    }

    return 0;
}

} // namespace taoetw

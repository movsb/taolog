#include "stdafx.h"

#include "config.h"

#include "_module_entry.hpp"

#include "listview_color.h"

namespace taoetw {

namespace {
    COLORREF last_colors[16];
}

void ListviewColor::choose_color_for(int i, bool fg)
{
    auto& current = fg ? (*_colors)[i].fg : (*_colors)[i].bg;
    CHOOSECOLOR cc = {sizeof(cc)};
    cc.hwndOwner = *this;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    cc.rgbResult = current;
    cc.lpCustColors = last_colors;
    
    if(::ChooseColor(&cc)) {
        current = cc.rgbResult;
        auto sta = _root->find<taowin::label>(std::to_wstring(i).c_str())->hwnd();
        if(!fg) {
            DeleteBrush(_brush_maps[sta]);
            _brush_maps[sta] = ::CreateSolidBrush(current);
        }
        ::InvalidateRect(sta, nullptr, TRUE);

        _set(i, fg);
    }
}

void ListviewColor::get_metas(WindowMeta * metas)
{
    __super::get_metas(metas);
    metas->style &= ~(WS_MINIMIZEBOX);
}

LPCTSTR ListviewColor::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="ÏêÇé" size="380,300">
    <res>
        <font name="default" face="Î¢ÈíÑÅºÚ" size="12"/>
        <font name="1" face="Consolas" size="16"/>
    </res>
    <root>
        <vertical padding="15,15,15,15">
            <horizontal padding="5,5,5,5">
                <label name="5" font="1" style="center,centerimage" exstyle="clientedge"/>
                <control width="5" />
                <vertical width="40">
                    <button name="fg5" text="Ç°¾°" /> <button name="bg5" text="±³¾°" />
                </vertical>
            </horizontal>
            <horizontal padding="5,5,5,5">
                <label name="4" font="1" style="center,centerimage" exstyle="clientedge"/>
                <control width="5" />
                <vertical width="40">
                    <button name="fg4" text="Ç°¾°" /> <button name="bg4" text="±³¾°" />
                </vertical>
            </horizontal>
            <horizontal padding="5,5,5,5">
                <label name="3" font="1" style="center,centerimage" exstyle="clientedge"/>
                <control width="5" />
                <vertical width="40">
                    <button name="fg3" text="Ç°¾°" /> <button name="bg3" text="±³¾°" />
                </vertical>
            </horizontal>
            <horizontal padding="5,5,5,5">
                <label name="2" font="1" style="center,centerimage" exstyle="clientedge"/>
                <control width="5" />
                <vertical width="40">
                    <button name="fg2" text="Ç°¾°" /> <button name="bg2" text="±³¾°" />
                </vertical>
            </horizontal>
            <horizontal padding="5,5,5,5">
                <label name="1" font="1" style="center,centerimage" exstyle="clientedge"/>
                <control width="5" />
                <vertical width="40">
                    <button name="fg1" text="Ç°¾°" /> <button name="bg1" text="±³¾°" />
                </vertical>
            </horizontal>
        </vertical>
    </root>
</window>
)tw";
    return json;
}

LRESULT ListviewColor::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
    case WM_CREATE:
    {
        constexpr unsigned int start = 1, end = 6;

        for(unsigned int i = start; i < end; i++) {
            auto ctl = _root->find<taowin::label>(std::to_wstring(i).c_str());
            ctl->set_text((*_levels)[i].cmt2.c_str());
            _label_maps[ctl->hwnd()] = i;
            _brush_maps[ctl->hwnd()] = ::CreateSolidBrush((*_colors)[i].bg);
        }

        return 0;
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wparam;
        HWND hwnd = (HWND)lparam;

        auto it = _label_maps.find(hwnd);
        if(it != _label_maps.cend()) {
            ::SetTextColor(hdc, (*_colors)[it->second].fg);
            ::SetBkColor(hdc, (*_colors)[it->second].bg);
            return LRESULT(_brush_maps[hwnd]);
        }

        break;
    }

    }
    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT ListviewColor::on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr)
{
    if(pc) {
        if(code == BN_CLICKED) {
            choose_color_for(pc->name()[2] - '0', pc->name()[0] == 'f');
        }
    }

    return 0;
}

}

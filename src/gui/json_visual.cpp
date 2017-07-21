#include "stdafx.h"

#include "misc/utils.h"
#include "misc/config.h"

#include "json_visual.h"

namespace taolog {

void JsonVisual::get_metas(WindowMeta * metas)
{
    __super::get_metas(metas);
    metas->style &= ~(WS_MINIMIZEBOX);
}

LPCTSTR JsonVisual::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<Window title="JSON ¿ÉÊÓ»¯" size="500,400">
    <Resource>
        <Font name="default" face="Î¢ÈíÑÅºÚ" size="12"/>
        <Font name="14" face="Î¢ÈíÑÅºÚ" size="14"/>
        <Font name="consolas" face="Consolas" size="12"/>
    </Resource>
    <Root>
        <Vertical padding="5,5,5,5">
            <Vertical name="nojson">
                <Control/>
                <Vertical height="60">
                    <Label font="14" style="center,centerimage" text="¼ôÌù°åÖÐÃ»ÓÐÓÐÐ§µÄ JSON ×Ö·û´®¡£" />
                    <Label font="14" style="center,centerimage" name="err" />
                </Vertical>
                <Control/>
            </Vertical>
            <TextBox name="text" font="consolas" style="multiline,vscroll,hscroll" exstyle="clientedge"/>
        </Vertical>
    </Root>
</Window>
)tw";
    return json;
}

LRESULT JsonVisual::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
    case WM_CREATE:
    {
        auto edit = _root->find<taowin::TextBox>(L"text");
        auto nojs_wrapper = _root->find<taowin::Label>(L"nojson");
        auto nojs_err = _root->find<taowin::Label>(L"err");

        auto wstr = utils::get_clipboard_text();
        auto str = g_config.us(wstr);
        std::string err;
        auto json = json11::Json::parse(str.c_str(), err);
        if(!err.empty()) {
            edit->set_visible(false);
            nojs_err->set_text(g_config.ws('(' + err + ')').c_str());
        }
        else {
            nojs_wrapper->set_visible(false);

            auto s = g_config.ws(json.dump(true));
            s = std::regex_replace(s, std::wregex(L"\n"), L"\r\n");
            edit->set_text(s.c_str());
        }

        return 0;
    }
    }
    return __super::handle_message(umsg, wparam, lparam);
}

}

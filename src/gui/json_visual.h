#pragma once

namespace taolog {

class JsonVisual : public taowin::WindowCreator
{
public:
    JsonVisual()
    {
    }

    ~JsonVisual()
    {
    }

protected:
    virtual void get_metas(WindowMeta* metas) override;
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual void on_final_message() override { __super::on_final_message(); delete this; }
};

}

#include "stdafx.h"
#include "_module_entry.hpp"

size_t taolog::ModuleContainer2ComboBoxDataSource::Size()
{
    return _data != nullptr ? _data->size() : 0;
}

void taolog::ModuleContainer2ComboBoxDataSource::GetAt(size_t index, TCHAR const ** text, void ** tag)
{
    auto m = (*_data)[index];
    *text = m->name.c_str();
    *tag = m;
}

void taolog::ModuleContainer2ComboBoxDataSource::set_source(ModuleContainer * data)
{
    _data = data;
}

taolog::ModuleContainer2ComboBoxDataSource::ModuleContainer2ComboBoxDataSource()
    :_data(nullptr)
{
}

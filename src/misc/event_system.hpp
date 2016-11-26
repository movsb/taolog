#pragma once

#include <cassert>

#include <string>
#include <map>
#include <vector>
#include <stack>
#include <functional>

namespace taoetw {

class EventArg
{
public:
    struct Type {
        enum Value {
            Undefined,
            Boolean,
            Integer,
            String,
            Pointer,
        };
    };

public:
    EventArg()
        : _type(Type::Undefined)
    {
        
    }

    EventArg(bool b)
    {
        assign(b);
    }

    EventArg(int i)
    {
        assign(i);
    }

    EventArg(const wchar_t* s)
    {
        assign(s);
    }

    EventArg(const std::wstring& s)
    {
        assign(s);
    }

    EventArg(const void* p)
    {
        assign(p);
    }

    // TODO 为啥需要这个？
    EventArg(std::nullptr_t p)
    {
        assign((void*)p);
    }

public:
    bool operator== (bool b) const
    {
        return type() == Type::Boolean
            && _bool_value == b;
    }

    bool operator== (int i) const
    {
        return type() == Type::Integer
            && _int_value == i;
    }

    bool operator== (const wchar_t* s) const
    {
        return type() == Type::String
            && _str_value == s;
    }

    bool operator== (const std::wstring& s) const
    {
        return type() == Type::String
            && _str_value == s;
    }

    bool operator== (const void* p) const
    {
        return type() == Type::Pointer
            && _ptr_value == p;
    }

public:
    Type::Value type() const    { return _type; }

    bool    bool_value() const  { return _bool_value; }
    int     int_value() const   { return _int_value; }
    const std::wstring&
            str_value() const   { return _str_value;}
    void*   ptr_value() const   { return const_cast<void*>(_ptr_value); }

protected:
    void assign(bool b)
    {
        _type = Type::Boolean;
        _bool_value = b;
    }

    void assign(int i)
    {
        _type = Type::Integer;
        _int_value = i;
    }

    void assign(const wchar_t* s)
    {
        _type = Type::String;
        _str_value.assign(s);
    }

    void assign(const std::wstring& s)
    {
        assign(s.c_str());
    }

    void assign(const void* p)
    {
        _type = Type::Pointer;
        _ptr_value = p;
    }

protected:
    Type::Value     _type;
    bool            _bool_value;
    int             _int_value;
    std::wstring    _str_value;
    const void*     _ptr_value;
};

class EventArguments
{
public:
    typedef std::vector<EventArg> Container;

public:
    int size() const { return (int)_argv.size(); }

    const EventArg& operator[](int i) const
    {
        if(i >= 0 && i < size()) {
            return _argv[i];
        }
        else {
            assert("Invalid index of EventArguments" && 0);
            return _nothing;
        }
    }

    template<typename F, typename ...T>
    void operator()(const F& f, T ...args)
    {
        _argv.emplace_back(f);

        operator()(std::forward<T>(args)...);
    }

    void operator()() {}

protected:
    EventArg    _nothing;
    Container   _argv;
};

class EventSystem
{
public:
    typedef std::function<void()>                       Callable;
    typedef unsigned long long                          Cookie;
    typedef std::map<Cookie, Callable>                  Callbacks;
    typedef std::map<std::wstring, Cookie>              NextCookie;
    typedef std::map<std::wstring, Callbacks>           EventMap;
    typedef std::stack<EventArguments*>                 ArgStack;

public:
    Cookie attach(const wchar_t* name, Callable fn)
    {
        auto cookie = ++_next[name];
        _events[name][cookie] = fn;
        return cookie;
    }

    void detach(const wchar_t* name, Cookie cookie)
    {
        _events[name].erase(cookie);
    }

    template<typename ...T>
    void trigger(const wchar_t* name, T ...args)
    {
        EventArguments arguments;

        arguments(std::forward<T>(args)...);

        _argstk.emplace(&arguments);

        for(const auto& pair : _events[name]) {
            pair.second();
        }

        _argstk.pop();
    }

    const EventArg& operator[](int i) const
    {
        return (*_argstk.top())[i];
    }

protected:
    EventMap    _events;
    NextCookie  _next;
    ArgStack    _argstk;
    
};

extern EventSystem& g_evtsys;

}

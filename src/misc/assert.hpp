#pragma once

#include <string>
#include <sstream>
#include <windows.h>

//////////////////////////////////////////////////////////////////////////
// Smart Assert

// #ifdef _DEBUG
// #define __SMART_DEBUG
// #else
// #undef __SMART_DEBUG
// #endif

#define __SMART_DEBUG

#ifdef __SMART_DEBUG

class _SmartAssert
{
public:
	_SmartAssert()
		: __SMART_ASSERT_A(*this)
		, __SMART_ASSERT_B(*this)
	{}

	_SmartAssert& __SMART_ASSERT_A;
	_SmartAssert& __SMART_ASSERT_B;

	_SmartAssert& _Context(const wchar_t* expr, const wchar_t* file, int line)
	{
		std::wstringstream ss;
		ss << L"EXPR: " << expr << L"\nFILE: " << file << L"\nLINE: " << line;
		m_expr = ss.str();
		return *this;
	}

	// to use this , you must have an operator<<(ostream&, your_class) overloaded
	template <class T>
	_SmartAssert& _Out(const wchar_t* str, T t)
	{
		std::wstringstream ss;
		ss << L"\n    " << str << L" = " << t << L"\n";
		m_expr += ss.str();
		return *this;
	}

	void Warning()
	{
		std::wstring str(L"警告: 应用程序遇到警告性错误!\n\n");
		str += m_expr;

		::MessageBox(nullptr, str.c_str(), L"断言失败", MB_ICONWARNING);
	}

	void Fatal()
	{
		std::wstring str(L"错误: 应用程序遇到严重性错误!\n\n");
		str += m_expr;
		::MessageBox(nullptr, str.c_str(), L"断言失败", MB_ICONERROR);
	}

private:
	std::wstring m_expr;
};

#define __SMART_ASSERT_A(x) __SMART_ASSERT(x,B)
#define __SMART_ASSERT_B(x) __SMART_ASSERT(x,A)

#define __SMART_ASSERT(x,next) \
	__SMART_ASSERT_A._Out(_T(#x), x).__SMART_ASSERT_##next

#define SMART_ASSERT(expr) \
	if(expr) ;\
	else _SmartAssert()._Context(_T(#expr), _T(__FILE__), __LINE__).__SMART_ASSERT_A

#define SMART_ENSURE(a,b) SMART_ASSERT((a)b) // or (a) ## b

#else // !SMART_DEBUG

#define SMART_ASSERT __SMART_ASSERT/
#define __SMART_ASSERT /

#define SMART_ENSURE(a,b) a;SMART_ASSERT

#endif // SMART_DEBUG

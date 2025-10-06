#pragma once
#include <Windows.h>
#include <string>
#include <iostream>
#include <format>
#include <dbghelp.h>
#include <strsafe.h>
#include <wrl.h>
#include <dxgidebug.h>

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "dxguid.lib")

namespace DebugUtility {
    void Log(std::ostream& os, const std::string& message);
    std::wstring ConvertString(const std::string& str);
    std::string ConvertString(const std::wstring& str);
    LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);
}

struct D3DResourceLeakChecker {
    D3DResourceLeakChecker();
    ~D3DResourceLeakChecker();
};
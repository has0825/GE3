#include "DebugUtility.h"
#include <cassert>

namespace DebugUtility {

    void Log(std::ostream& os, const std::string& message) {
        os << message << std::endl;
        OutputDebugStringA(message.c_str());
        OutputDebugStringA("\n");
    }

    std::wstring ConvertString(const std::string& str) {
        if (str.empty()) {
            return std::wstring();
        }
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    std::string ConvertString(const std::wstring& str) {
        if (str.empty()) {
            return std::string();
        }
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
        SYSTEMTIME time;
        GetLocalTime(&time);
        wchar_t filePath[MAX_PATH] = { 0 };
        CreateDirectory(L"./Dumps", nullptr);
        StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d%02d.dmp",
            time.wYear, time.wMonth, time.wDay, time.wHour,
            time.wMinute, time.wSecond);

        HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

        MINIDUMP_EXCEPTION_INFORMATION minidumpInformation = {};
        minidumpInformation.ThreadId = GetCurrentThreadId();
        minidumpInformation.ExceptionPointers = exception;
        minidumpInformation.ClientPointers = TRUE;

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFileHandle,
            MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

        CloseHandle(dumpFileHandle);

        return EXCEPTION_EXECUTE_HANDLER;
    }
}

D3DResourceLeakChecker::D3DResourceLeakChecker() {}

D3DResourceLeakChecker::~D3DResourceLeakChecker() {
#ifdef _DEBUG
    Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
        debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
    }
#endif
}
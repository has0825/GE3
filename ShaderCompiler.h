// ShaderCompiler.h
#pragma once
#include <Windows.h>
#include <wrl.h>
#include <dxcapi.h>
#include <string>
#include <iostream>

#pragma comment(lib, "dxcompiler.lib")

class ShaderCompiler {
public:
    static ShaderCompiler* GetInstance();

    // 初期化
    void Initialize();
    // コンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> Compile(
        const std::wstring& filePath,
        const wchar_t* profile);

private:
    ShaderCompiler() = default;
    ~ShaderCompiler() = default;
    ShaderCompiler(const ShaderCompiler&) = delete;
    const ShaderCompiler& operator=(const ShaderCompiler&) = delete;

    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};
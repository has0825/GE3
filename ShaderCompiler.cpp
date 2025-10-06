#include "ShaderCompiler.h"
#include "DebugUtility.h"
#include <cassert>
#include <stdexcept> // std::runtime_error を使うために追加

ShaderCompiler* ShaderCompiler::GetInstance() {
    static ShaderCompiler instance;
    return &instance;
}

void ShaderCompiler::Initialize() {
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));
    hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
    assert(SUCCEEDED(hr));
}

Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::Compile(const std::wstring& filePath, const wchar_t* profile) {
    DebugUtility::Log(std::cout, DebugUtility::ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
    HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);

    // assertよりも堅牢なエラーチェックに変更
    if (FAILED(hr)) {
        std::string errorMsg = "Failed to load shader file: " + DebugUtility::ConvertString(filePath) + "\nCheck if the file exists and the Working Directory is set to $(SolutionDir).";
        throw std::runtime_error(errorMsg);
    }

    DxcBuffer shaderSourceBuffer;
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8;

    LPCWSTR arguments[] = {
        filePath.c_str(),
        L"-E", L"main",
        L"-T", profile,
        L"-Zi", L"-Qembed_debug",
        L"-Od",
        L"-Zpr"
    };

    Microsoft::WRL::ComPtr<IDxcResult> shaderResult = nullptr;
    hr = dxcCompiler_->Compile(
        &shaderSourceBuffer,
        arguments,
        _countof(arguments),
        includeHandler_.Get(),
        IID_PPV_ARGS(&shaderResult));
    assert(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError = nullptr;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        DebugUtility::Log(std::cerr, shaderError->GetStringPointer());
        assert(false);
    }

    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    DebugUtility::Log(std::cout, DebugUtility::ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));

    return shaderBlob;
}
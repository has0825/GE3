#define _USE_MATH_DEFINES
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <wrl.h>
#include <Windows.h>
#include <d3d12.h>
#include <dbghelp.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <strsafe.h>
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam);


struct Vector2 {
    float x, y;
};
struct Vector3 {
    float x, y, z;
};
struct Vector4 {
    float x, y, z, w;
};
struct Matrix3x3 {
    float m[3][3];
};
struct Matrix4x4 {
    float m[4][4];
};

struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};
// 頂点、マテリアル関連
struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};
struct Material {
    Vector4 color;
    int32_t enableLighting;
    float padding[3];
    Matrix4x4 uvTransform;
};
struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
};
struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
};
// パーティクル等
struct Fragment {
    Vector3 position;
    Vector3 velocity;
    Vector3 rotation;
    Vector3 rotationSpeed;
    float alpha;
    bool active;
};

// モデルデータ
struct MaterialData {
    std::string textureFilePath;
};

struct ModelData {
    std::vector<VertexData> vertices;
    MaterialData material;
};
//==音声構造体==//
// チャンクヘッダ
struct ChunkHeader {
    char id[4]; // チャンクID
    uint32_t size; // チャンクサイズ
};

// RIFFヘッダチャンク
struct RiffHeader {
    ChunkHeader chunk; // チャンクヘッダ(RIFF)
    char type[4]; // フォーマット（"WAVE"）
};

// FMTチャンク
struct FormatChunk {
    ChunkHeader chunk; // チャンクヘッダ(FMT)
    WAVEFORMATEX fmt; // WAVEフォーマット
};
// 音声データ
struct SoundData {
    // 波形フォーマット
    WAVEFORMATEX wfex;
    // バッファの先頭アドレス
    BYTE* pBuffer;
    // バッファのサイズ
    unsigned int bufferSize;
};

//------------------
// グローバル定数
//------------------
const int kSubdivision = 16; // 16分割
int kNumVertices = kSubdivision * kSubdivision * 6; // 頂点数
// --- 列挙体 ---
enum WaveType {
    WAVE_SINE,
    WAVE_CHAINSAW,
    WAVE_SQUARE,
};

enum AnimationType {
    ANIM_RESET,
    ANIM_NONE,
    ANIM_COLOR,
    ANIM_SCALE,
    ANIM_ROTATE,
    ANIM_TRANSLATE,
    ANIM_TORNADO,
    ANIM_PULSE,
    ANIM_AURORA,
    ANIM_BOUNCE,
    ANIM_TWIST,
    ANIM_ALL

};
// グローバル変数
WaveType waveType = WAVE_SINE;
AnimationType animationType = ANIM_NONE;
float waveTime = 0.0f;
//////////////---------------------------------------
// 関数の作成///
//////////////
#pragma region 行列関数
// 単位行列の作成
Matrix4x4 MakeIdentity4x4()
{
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i)
        result.m[i][i] = 1.0f;
    return result;
}
// 拡大縮小行列S
Matrix4x4 Matrix4x4MakeScaleMatrix(const Vector3& s)
{
    Matrix4x4 result = {};
    result.m[0][0] = s.x;
    result.m[1][1] = s.y;
    result.m[2][2] = s.z;
    result.m[3][3] = 1.0f;
    return result;
}

// X軸回転行列R
Matrix4x4 MakeRotateXMatrix(float radian)
{
    Matrix4x4 result = {};

    result.m[0][0] = 1.0f;
    result.m[1][1] = std::cos(radian);
    result.m[1][2] = std::sin(radian);
    result.m[2][1] = -std::sin(radian);
    result.m[2][2] = std::cos(radian);
    result.m[3][3] = 1.0f;

    return result;
}
// Y軸回転行列R
Matrix4x4 MakeRotateYMatrix(float radian)
{
    Matrix4x4 result = {};

    result.m[0][0] = std::cos(radian);
    result.m[0][2] = std::sin(radian);
    result.m[1][1] = 1.0f;
    result.m[2][0] = -std::sin(radian);
    result.m[2][2] = std::cos(radian);
    result.m[3][3] = 1.0f;

    return result;
}
// Z軸回転行列R
Matrix4x4 MakeRotateZMatrix(float radian)
{
    Matrix4x4 result = {};

    result.m[0][0] = std::cos(radian);
    result.m[0][1] = -std::sin(radian);
    result.m[1][0] = std::sin(radian);
    result.m[1][1] = std::cos(radian);
    result.m[2][2] = 1.0f;
    result.m[3][3] = 1.0f;

    return result;
}

// 平行移動行列T
Matrix4x4 MakeTranslateMatrix(const Vector3& tlanslate)
{
    Matrix4x4 result = {};
    result.m[0][0] = 1.0f;
    result.m[1][1] = 1.0f;
    result.m[2][2] = 1.0f;
    result.m[3][3] = 1.0f;
    result.m[3][0] = tlanslate.x;
    result.m[3][1] = tlanslate.y;
    result.m[3][2] = tlanslate.z;

    return result;
}
// 行列の積
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2)
{
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                result.m[i][j] += m1.m[i][k] * m2.m[k][j];
    return result;
}
// ワールドマトリックス、メイクアフィン
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate,
    const Vector3& translate)
{
    Matrix4x4 scaleMatrix = Matrix4x4MakeScaleMatrix(scale);
    Matrix4x4 rotateX = MakeRotateXMatrix(rotate.x);
    Matrix4x4 rotateY = MakeRotateYMatrix(rotate.y);
    Matrix4x4 rotateZ = MakeRotateZMatrix(rotate.z);
    Matrix4x4 rotateMatrix = Multiply(Multiply(rotateX, rotateY), rotateZ);
    Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

    Matrix4x4 worldMatrix = Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);
    return worldMatrix;
}
// 4x4 行列の逆行列を計算する関数
Matrix4x4 Inverse(Matrix4x4 m)
{
    Matrix4x4 result;
    float det;
    int i;

    result.m[0][0] = m.m[1][1] * m.m[2][2] * m.m[3][3] - m.m[1][1] * m.m[2][3] * m.m[3][2] - m.m[2][1] * m.m[1][2] * m.m[3][3] + m.m[2][1] * m.m[1][3] * m.m[3][2] + m.m[3][1] * m.m[1][2] * m.m[2][3] - m.m[3][1] * m.m[1][3] * m.m[2][2];

    result.m[0][1] = -m.m[0][1] * m.m[2][2] * m.m[3][3] + m.m[0][1] * m.m[2][3] * m.m[3][2] + m.m[2][1] * m.m[0][2] * m.m[3][3] - m.m[2][1] * m.m[0][3] * m.m[3][2] - m.m[3][1] * m.m[0][2] * m.m[2][3] + m.m[3][1] * m.m[0][3] * m.m[2][2];

    result.m[0][2] = m.m[0][1] * m.m[1][2] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[3][2] - m.m[1][1] * m.m[0][2] * m.m[3][3] + m.m[1][1] * m.m[0][3] * m.m[3][2] + m.m[3][1] * m.m[0][2] * m.m[1][3] - m.m[3][1] * m.m[0][3] * m.m[1][2];

    result.m[0][3] = -m.m[0][1] * m.m[1][2] * m.m[2][3] + m.m[0][1] * m.m[1][3] * m.m[2][2] + m.m[1][1] * m.m[0][2] * m.m[2][3] - m.m[1][1] * m.m[0][3] * m.m[2][2] - m.m[2][1] * m.m[0][2] * m.m[1][3] + m.m[2][1] * m.m[0][3] * m.m[1][2];

    result.m[1][0] = -m.m[1][0] * m.m[2][2] * m.m[3][3] + m.m[1][0] * m.m[2][3] * m.m[3][2] + m.m[2][0] * m.m[1][2] * m.m[3][3] - m.m[2][0] * m.m[1][3] * m.m[3][2] - m.m[3][0] * m.m[1][2] * m.m[2][3] + m.m[3][0] * m.m[1][3] * m.m[2][2];

    result.m[1][1] = m.m[0][0] * m.m[2][2] * m.m[3][3] - m.m[0][0] * m.m[2][3] * m.m[3][2] - m.m[2][0] * m.m[0][2] * m.m[3][3] + m.m[2][0] * m.m[0][3] * m.m[3][2] + m.m[3][0] * m.m[0][2] * m.m[2][3] - m.m[3][0] * m.m[0][3] * m.m[2][2];

    result.m[1][2] = -m.m[0][0] * m.m[1][2] * m.m[3][3] + m.m[0][0] * m.m[1][3] * m.m[3][2] + m.m[1][0] * m.m[0][2] * m.m[3][3] - m.m[1][0] * m.m[0][3] * m.m[3][2] - m.m[3][0] * m.m[0][2] * m.m[1][3] + m.m[3][0] * m.m[0][3] * m.m[1][2];

    result.m[1][3] = m.m[0][0] * m.m[1][2] * m.m[2][3] - m.m[0][0] * m.m[1][3] * m.m[2][2] - m.m[1][0] * m.m[0][2] * m.m[2][3] + m.m[1][0] * m.m[0][3] * m.m[2][2] + m.m[2][0] * m.m[0][2] * m.m[1][3] - m.m[2][0] * m.m[0][3] * m.m[1][2];

    result.m[2][0] = m.m[1][0] * m.m[2][1] * m.m[3][3] - m.m[1][0] * m.m[2][3] * m.m[3][1] - m.m[2][0] * m.m[1][1] * m.m[3][3] + m.m[2][0] * m.m[1][3] * m.m[3][1] + m.m[3][0] * m.m[1][1] * m.m[2][3] - m.m[3][0] * m.m[1][3] * m.m[2][1];

    result.m[2][1] = -m.m[0][0] * m.m[2][1] * m.m[3][3] + m.m[0][0] * m.m[2][3] * m.m[3][1] + m.m[2][0] * m.m[0][1] * m.m[3][3] - m.m[2][0] * m.m[0][3] * m.m[3][1] - m.m[3][0] * m.m[0][1] * m.m[2][3] + m.m[3][0] * m.m[0][3] * m.m[2][1];

    result.m[2][2] = m.m[0][0] * m.m[1][1] * m.m[3][3] - m.m[0][0] * m.m[1][3] * m.m[3][1] - m.m[1][0] * m.m[0][1] * m.m[3][3] + m.m[1][0] * m.m[0][3] * m.m[3][1] + m.m[3][0] * m.m[0][1] * m.m[1][3] - m.m[3][0] * m.m[0][3] * m.m[1][1];

    result.m[2][3] = -m.m[0][0] * m.m[1][1] * m.m[2][3] + m.m[0][0] * m.m[1][3] * m.m[2][1] + m.m[1][0] * m.m[0][1] * m.m[2][3] - m.m[1][0] * m.m[0][3] * m.m[2][1] - m.m[2][0] * m.m[0][1] * m.m[1][3] + m.m[2][0] * m.m[0][3] * m.m[1][1];

    result.m[3][0] = -m.m[1][0] * m.m[2][1] * m.m[3][2] + m.m[1][0] * m.m[2][2] * m.m[3][1] + m.m[2][0] * m.m[1][1] * m.m[3][2] - m.m[2][0] * m.m[1][2] * m.m[3][1] - m.m[3][0] * m.m[1][1] * m.m[2][2] + m.m[3][0] * m.m[1][2] * m.m[2][1];

    result.m[3][1] = m.m[0][0] * m.m[2][1] * m.m[3][2] - m.m[0][0] * m.m[2][2] * m.m[3][1] - m.m[2][0] * m.m[0][1] * m.m[3][2] + m.m[2][0] * m.m[0][2] * m.m[3][1] + m.m[3][0] * m.m[0][1] * m.m[2][2] - m.m[3][0] * m.m[0][2] * m.m[2][1];

    result.m[3][2] = -m.m[0][0] * m.m[1][1] * m.m[3][2] + m.m[0][0] * m.m[1][2] * m.m[3][1] + m.m[1][0] * m.m[0][1] * m.m[3][2] - m.m[1][0] * m.m[0][2] * m.m[3][1] - m.m[3][0] * m.m[0][1] * m.m[1][2] + m.m[3][0] * m.m[0][2] * m.m[1][1];

    result.m[3][3] = m.m[0][0] * m.m[1][1] * m.m[2][2] - m.m[0][0] * m.m[1][2] * m.m[2][1] - m.m[1][0] * m.m[0][1] * m.m[2][2] + m.m[1][0] * m.m[0][2] * m.m[2][1] + m.m[2][0] * m.m[0][1] * m.m[1][2] - m.m[2][0] * m.m[0][2] * m.m[1][1];

    det = m.m[0][0] * result.m[0][0] + m.m[0][1] * result.m[1][0] + m.m[0][2] * result.m[2][0] + m.m[0][3] * result.m[3][0];

    if (det == 0)
        return Matrix4x4{}; // またはエラー処理

    det = 1.0f / det;

    for (i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            result.m[i][j] = result.m[i][j] * det;

    return result;
}
// 透視投影行列
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio,
    float nearClip, float farClip)
{
    Matrix4x4 result = {};

    float f = 1.0f / std::tan(fovY / 2.0f);

    result.m[0][0] = f / aspectRatio;
    result.m[1][1] = f;
    result.m[2][2] = farClip / (farClip - nearClip);
    result.m[2][3] = 1.0f;
    result.m[3][2] = -(nearClip * farClip) / (farClip - nearClip);
    return result;
}
// 正射影行列
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right,
    float bottom, float nearClip, float farClip)
{
    Matrix4x4 m = {};

    m.m[0][0] = 2.0f / (right - left);
    m.m[1][1] = 2.0f / (top - bottom);
    m.m[2][2] = 1.0f / (farClip - nearClip);
    m.m[3][0] = -(right + left) / (right - left);
    m.m[3][1] = -(top + bottom) / (top - bottom);
    m.m[3][2] = -nearClip / (farClip - nearClip);
    m.m[3][3] = 1.0f;

    return m;
}
// ビューポート変換行列
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height,
    float minDepth, float maxDepth)
{
    Matrix4x4 m = {};

    // 行0：X方向スケーリングと移動
    m.m[0][0] = width / 2.0f;
    m.m[1][1] = -height / 2.0f;
    m.m[2][2] = maxDepth - minDepth;
    m.m[3][0] = left + width / 2.0f;
    m.m[3][1] = top + height / 2.0f;
    m.m[3][2] = minDepth;
    m.m[3][3] = 1.0f;

    return m;
}
// 正規化関数
Vector3 Normalize(const Vector3& v)
{
    float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length == 0.0f)
        return { 0.0f, 0.0f, 0.0f };
    return { v.x / length, v.y / length, v.z / length };
}
#pragma endregion

//===エラーハンドリング用の身にダンプ出力関数===///
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception)
{
    // 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下ぶ出力
    SYSTEMTIME time;
    GetLocalTime(&time);
    wchar_t filePath[MAX_PATH] = { 0 };
    CreateDirectory(L"./Dumps", nullptr);
    StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
        time.wYear, time.wMonth, time.wDay, time.wHour,
        time.wMinute);
    HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    // processId(このexeのId)とクラッシュ(例外)の発生したthreadIdを取得
    DWORD processId = GetCurrentProcessId();
    DWORD threadId = GetCurrentThreadId();
    // 設定情報を入力
    MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
    minidumpInformation.ThreadId = threadId;
    minidumpInformation.ExceptionPointers = exception;
    minidumpInformation.ClientPointers = TRUE;
    // Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
    MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle,
        MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
    // 他に関連づけられているSEH例外ハンドラがあれば実行。

    return EXCEPTION_EXECUTE_HANDLER;
}

//=== ログ出力関数（コンソールとデバッグ出力）==//
void Log(std::ostream& os, const std::string& message)
{
    os << message << std::endl;
    OutputDebugStringA(message.c_str());
}

//=== UTF-8文字列 -> ワイド文字列への変換===/
std::wstring ConvertString(const std::string& str)
{
    if (str.empty()) {
        return std::wstring();
    }

    auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]),
        static_cast<int>(str.size()), NULL, 0);
    if (sizeNeeded == 0) {
        return std::wstring();
    }
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]),
        static_cast<int>(str.size()), &result[0], sizeNeeded);
    return result;
}

//=== ワイド文字列 -> UTF-8文字列への変換 ===
std::string ConvertString(const std::wstring& str)
{
    if (str.empty()) {
        return std::string();
    }

    auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
        NULL, 0, NULL, NULL);
    if (sizeNeeded == 0) {
        return std::string();
    }
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
        result.data(), sizeNeeded, NULL, NULL);
    return result;
}

//=== D3D12バッファリソース作成（UPLOADヒープ） ===
Microsoft::WRL::ComPtr<ID3D12Resource>
CreateBufferResource(ID3D12Device* device, size_t sizeInBytes)
{

    // 頂点リソース用のヒープの設定02_03
    D3D12_HEAP_PROPERTIES uploadHeapProperties{};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // Uploadheapを使う
    // 頂点リソースの設定02_03
    D3D12_RESOURCE_DESC vertexResourceDesc{};
    // バッファリソース。テクスチャの場合はまた別の設定をする02_03
    vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vertexResourceDesc.Width = sizeInBytes; // リソースのサイズ　02_03
    // バッファの場合はこれらは１にする決まり02_03
    vertexResourceDesc.Height = 1;
    vertexResourceDesc.DepthOrArraySize = 1;
    vertexResourceDesc.MipLevels = 1;
    vertexResourceDesc.SampleDesc.Count = 1;
    // バッファの場合はこれにする決まり02_03
    vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // 実際に頂点リソースを作る02_03
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&vertexResource));
    assert(SUCCEEDED(hr));

    return vertexResource.Get();
}

//=== D3D12ディスクリプタヒープ作成 ===
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
CreateDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12Device> device,
    D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors,
    bool shaderVisivle)
{
    // ディスクリプタヒープの生成02_02
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DescriptorHeap = nullptr;

    D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc{};
    DescriptorHeapDesc.Type = heapType;
    DescriptorHeapDesc.NumDescriptors = numDescriptors;
    DescriptorHeapDesc.Flags = shaderVisivle
        ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = device->CreateDescriptorHeap(&DescriptorHeapDesc,
        IID_PPV_ARGS(&DescriptorHeap));
    // ディスクリプタヒープが作れなかったので起動できない
    assert(SUCCEEDED(hr)); // 1
    return DescriptorHeap.Get();
}

//=== D3D12テクスチャリソース作成（DEFAULTヒープ） ===
Microsoft::WRL::ComPtr<ID3D12Resource>
CreateTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device,
    const DirectX::TexMetadata& metadata)
{
    // 1.metadataをもとにResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width); // Textureの幅
    resourceDesc.Height = UINT(metadata.height); // Textureの高さ
    resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipdmapの数
    resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // 奥行き　or 配列Textureの配列数
    resourceDesc.Format = metadata.format; // TextureのFormat
    resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(
        metadata.dimension); // Textureの次元数　普段使っているのは二次元
    // 2.利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // 細かい設定を行う//03_00EX
    // heapProperties.CPUPageProperty =
    //     D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; //
    //     WriteBaackポリシーでCPUアクセス可能
    // heapProperties.MemoryPoolPreference =
    //     D3D12_MEMORY_POOL_L0; // プロセッサの近くに配置

    // 3.Resourceを生成する
    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties, // Heapの固定
        D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし
        &resourceDesc, // Resourceの設定
        D3D12_RESOURCE_STATE_COPY_DEST, // 初回のResourceState.Textureは基本読むだけ//03_00EX
        nullptr, // Clear最適地。使わないのでnullptr
        IID_PPV_ARGS(&resource)); // 作成するResourceポインタへのポインタ
    assert(SUCCEEDED(hr));
    return resource.Get();
}

// データを転送するUploadTextureData関数を作る03_00EX
[[nodiscard]] // 03_00EX
Microsoft::WRL::ComPtr<ID3D12Resource>
UploadTextureData(
    Microsoft::WRL::ComPtr<ID3D12Resource> texture,
    const DirectX::ScratchImage& mipImages,
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList)
{
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    DirectX::PrepareUpload(device.Get(), mipImages.GetImages(),
        mipImages.GetImageCount(), mipImages.GetMetadata(),
        subresources);

    uint64_t intermediateSize = GetRequiredIntermediateSize(
        texture.Get(), 0, static_cast<UINT>(subresources.size()));
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediate = CreateBufferResource(device.Get(), intermediateSize);

    UpdateSubresources(commandList.Get(), texture.Get(), intermediate.Get(), 0, 0,
        static_cast<UINT>(subresources.size()),
        subresources.data());

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    return intermediate;
}

// ミップマップです03_00
DirectX::ScratchImage LoadTexture(const std::string& filePath)
{
    // テクスチャファイルを読んでプログラムで扱えるようにする
    DirectX::ScratchImage image{};
    std::wstring filePathW = ConvertString(filePath);
    HRESULT hr = DirectX::LoadFromWICFile(
        filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    // std::wcout << L"LoadFromWICFile HR: " << std::hex << hr << std::endl;
    assert(SUCCEEDED(hr));

    // ミップマップの作成
    DirectX::ScratchImage mipImages{};
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(),
        image.GetMetadata(), DirectX::TEX_FILTER_SRGB,
        0, mipImages);
    assert(SUCCEEDED(hr));

    // ミップマップ付きのデータを返す
    return mipImages;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
CreateDepthStencilTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device,
    int32_t width, int32_t height)
{
    // 生成するResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    // 利用するheapの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    // 深度値のクリア設定
    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // Resourceの設定
    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr; // com
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource.Get();
}

// 球の頂点生成関数_05_00_OTHER新しい書き方
void GenerateSphereVertices(VertexData* vertices, int kSubdivision,
    float radius)
{
    // 経度(360)
    const float kLonEvery = static_cast<float>(M_PI * 2.0f) / kSubdivision;
    // 緯度(180)
    const float kLatEvery = static_cast<float>(M_PI) / kSubdivision;

    for (int latIndex = 0; latIndex < kSubdivision; ++latIndex) {
        float lat = -static_cast<float>(M_PI) / 2.0f + kLatEvery * latIndex;
        float nextLat = lat + kLatEvery;

        for (int lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
            float lon = kLonEvery * lonIndex;
            float nextLon = lon + kLonEvery;

            // verA
            VertexData vertA;
            vertA.position = { cosf(lat) * cosf(lon), sinf(lat), cosf(lat) * sinf(lon),
                1.0f };
            vertA.texcoord = { static_cast<float>(lonIndex) / kSubdivision,
                1.0f - static_cast<float>(latIndex) / kSubdivision };
            vertA.normal = { vertA.position.x, vertA.position.y, vertA.position.z };

            // verB
            VertexData vertB;
            vertB.position = { cosf(nextLat) * cosf(lon), sinf(nextLat),
                cosf(nextLat) * sinf(lon), 1.0f };
            vertB.texcoord = { static_cast<float>(lonIndex) / kSubdivision,
                1.0f - static_cast<float>(latIndex + 1) / kSubdivision };
            vertB.normal = { vertB.position.x, vertB.position.y, vertB.position.z };

            // vertC
            VertexData vertC;
            vertC.position = { cosf(lat) * cosf(nextLon), sinf(lat),
                cosf(lat) * sinf(nextLon), 1.0f };
            vertC.texcoord = { static_cast<float>(lonIndex + 1) / kSubdivision,
                1.0f - static_cast<float>(latIndex) / kSubdivision };
            vertC.normal = { vertC.position.x, vertC.position.y, vertC.position.z };

            // vertD
            VertexData vertD;
            vertD.position = { cosf(nextLat) * cosf(nextLon), sinf(nextLat),
                cosf(nextLat) * sinf(nextLon), 1.0f };
            vertD.texcoord = { static_cast<float>(lonIndex + 1) / kSubdivision,
                1.0f - static_cast<float>(latIndex + 1) / kSubdivision };
            vertD.normal = { vertD.position.x, vertD.position.y, vertD.position.z };

            // 初期位置//
            uint32_t startIndex = (latIndex * kSubdivision + lonIndex) * 6;

            vertices[startIndex + 0] = vertA;
            vertices[startIndex + 1] = vertB;
            vertices[startIndex + 2] = vertC;
            vertices[startIndex + 3] = vertC;
            vertices[startIndex + 4] = vertD;
            vertices[startIndex + 5] = vertB;
        }
    }
}
// D3Dリソースリークチェック用のクラス
struct D3DResourceLeakChecker {
    ~D3DResourceLeakChecker()
    {
        Microsoft::WRL::ComPtr<IDXGIDebug1> debug;

        // DXGIのデバッグインターフェースを取得
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
            // DXGI全体のリソースチェック（アプリが作ったリソースがまだ残ってるか確認）
            debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
            debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
            debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
        }
    }
};

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{

    //
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    // メッセージに応じて固有の処理を行う
    switch (msg) {
        // ウィンドウが破棄された
    case WM_DESTROY:
        // OSに対して、アプリの終了を伝える
        PostQuitMessage(0);
        return 0;
    }
    // 標準のメッセージ処理を行う
    return DefWindowProc(hwnd, msg, wparam, lparam);
}
// CompileShader関数02_00
Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
    // CompilerするSHaderファイルへのパス02_00
    const std::wstring& filepath,
    // Compilerに使用するprofile02_00
    const wchar_t* profile,
    // 初期化で生成したものを3つ02_00
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils,
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler,
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler,
    std::ostream& os)
{
    // ここの中身を書いていく02_00
    // 1.hlslファイルを読み込む02_00
    // これからシェーダーをコンパイルする旨をログに出す02_00
    Log(os, ConvertString(std::format(L"Begin CompileShader,path:{},profike:{}\n", filepath, profile)));
    // hlslファイルを読む02_00
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
    HRESULT hr = dxcUtils->LoadFile(filepath.c_str(), nullptr, &shaderSource);
    // 読めなかったら止める02_00
    assert(SUCCEEDED(hr));
    // 読み込んだファイルの内容を設定する02_00
    DxcBuffer shaderSourceBuffer;
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8の文字コードであることを通知02_00
    // 2.Compileする
    LPCWSTR arguments[] = {
        filepath.c_str(), // コンパイル対象のhlslファイル名02_00
        L"-E",
        L"main", // エントリーポイントの指定。基本的にmain以外にはしない02_00
        L"-T",
        profile, // shaderProfileの設定02_00
        L"-Zi",
        L"-Qembed_debug", // デバック用の設定を埋め込む02_00
        L"-Od", /// 最適化を外しておく02_00
        L"-Zpr" // メモリレイアウトは行優先02_00

    };
    // 実際にShaderをコンパイルする02_00
    Microsoft::WRL::ComPtr<IDxcResult> shaderResult = nullptr;
    hr = dxcCompiler->Compile(

        &shaderSourceBuffer, // 読み込んだファイル02_00
        arguments, // コンパイルオプション02_00
        _countof(arguments), // コンパイルオプションの数02_00
        includeHandler.Get(), // includeが含まれた諸々02_00
        IID_PPV_ARGS(&shaderResult) // コンパイル結果02_00
    );
    // コンパイルエラーではなくdxcが起動できないなど致命的な状況02_00
    assert(SUCCEEDED(hr));
    // 3.警告、エラーが出ていないか確認する02_00
    // 警告.エラーが出ていたらログに出して止める02_00
    IDxcBlobUtf8* shaderError = nullptr;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        Log(os, shaderError->GetStringPointer());
        // 警告、エラーダメ絶対02_00
        assert(false);
    }
    // 4.Compile結果を受け取って返す02_00
    // コンパイル結果から実行用のバイナリ部分を取得02_00
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob),
        nullptr);
    assert(SUCCEEDED(hr));
    // 成功したログを出す02_00
    Log(os,
        ConvertString(std::format(L"Compile Succeeded, path:{}, profike:{}\n ",
            filepath, profile)));
    // もう使わないリソースを解放02_00
    // shaderSource->Release();
    // shaderResult->Release();
    // 実行用のバイナリを返却02_00
    return shaderBlob.Get(); // get
}

// CG2_05_01_page_5
D3D12_CPU_DESCRIPTOR_HANDLE
GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap,
    uint32_t descriptorSize, uint32_t index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += (descriptorSize * index);
    return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE
GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap,
    uint32_t descriptorSize, uint32_t index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += (descriptorSize * index);
    return handleGPU;
}

/// CG_02_06
MaterialData LoadMaterialTemplateFile(const std::string& directoryPath,
    const std::string& filename)
{
    // 1.中で必要となる変数の宣言
    MaterialData materialData; // 構築するMaterialData
    // 2.ファイルを開く
    std::string line; // ファイルから読んだ１行を格納するもの
    std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
    assert(file.is_open()); // とりあえず開けなかったら止める
    // 3.実際にファイルを読み、MaterialDataを構築していく
    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;
        // identifierに応じた処理
        if (identifier == "map_Kd") {
            std::string textureFilename;
            s >> textureFilename;
            // 連結してファイルパスにする
            materialData.textureFilePath = directoryPath + "/" + textureFilename;
        }
    }
    // 4.materialDataを返す
    return materialData;
}
// std::stringは文字列を扱う
// 06_02
ModelData LoadOjFile(const std::string& directoryPath,
    const std::string& filename)
{

    // 1.中で必要となる変数の宣言
    ModelData modelData; // 構築するModelData
    std::vector<Vector4> positions; // 位置
    std::vector<Vector3> normals; // 法線
    std::vector<Vector2> texcoords; // テクスチャ座標
    std::string line; // ファイルから読んだ一行を格納するもの

    // 2.ファイルを開く
    std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
    assert(file.is_open()); // とりあえず開けなかったら止める

    // 3.実際にファイルを読み,ModelDataを構築していく
    while (std::getline(file, line)) {
        std::string identifiler;
        std::istringstream s(line);
        s >> identifiler; // 先頭の識別子を読む

        // identifierに応じた処理
        if (identifiler == "v") {
            Vector4 position;
            s >> position.x >> position.y >> position.z;
            // 左手座標にする
            position.x *= -1.0f;

            position.w = 1.0f;
            positions.push_back(position);
        } else if (identifiler == "vt") {
            Vector2 texcoord;
            s >> texcoord.x >> texcoord.y;
            // 上下逆にする

            // texcoord.y *= -1.0f;
            texcoord.y = 1.0f - texcoord.y;
            // CG2_06_02_kusokusosjsusuawihoafwhgiuwhkgfau
            texcoords.push_back(texcoord);
        } else if (identifiler == "vn") {
            Vector3 normal;
            s >> normal.x >> normal.y >> normal.z;
            // 左手座標にする
            normal.x *= -1.0f;

            normals.push_back(normal);
        } else if (identifiler == "f") {
            VertexData triangle[3]; // 三つの頂点を保存
            // 面は三角形限定。その他は未対応
            for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
                std::string vertexDefinition;
                s >> vertexDefinition;
                // 頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してえIndexを取得する
                std::istringstream v(vertexDefinition);
                uint32_t elementIndices[3];
                for (int32_t element = 0; element < 3; ++element) {
                    std::string index;

                    std::getline(v, index, '/'); // 区切りでインデックスを読んでいく
                    elementIndices[element] = std::stoi(index);
                }
                // 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
                Vector4 position = positions[elementIndices[0] - 1];
                Vector2 texcoord = texcoords[elementIndices[1] - 1];
                Vector3 normal = normals[elementIndices[2] - 1];
                // X軸を反転して左手座標系に

                triangle[faceVertex] = { position, texcoord, normal };
            }
            // 逆順にして格納（2 → 1 → 0）
            modelData.vertices.push_back(triangle[2]);
            modelData.vertices.push_back(triangle[1]);
            modelData.vertices.push_back(triangle[0]);
            //?
        } else if (identifiler == "mtllib") {
            // materialTemplateLibraryファイルの名前を取得する
            std::string materialFilename;
            s >> materialFilename;
            // 基本的にobjファイルと同一階層mtlは存在させるので、ディレクトリ名とファイル名を渡す。
            modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
        }
    }
    // 4.ModelDataを返す
    return modelData;
}

//SoundData SoundLoadWave(const char* filename)
//{
//    HRESULT result;
//
//    std::ifstream file(filename, std::ios::binary);
//    assert(file.is_open());
//
//    // RIFFヘッダーの読み込み
//    RiffHeader riff;
//    file.read((char*)&riff, sizeof(riff));
//    if (strncmp(riff.chunk.id, "RIFF", 4) != 0 || strncmp(riff.type, "WAVE", 4) != 0) {
//        assert(0);
//    }
//
//    FormatChunk format = {};
//    ChunkHeader chunk = {};
//
//    // チャンクを順に読み取って fmt と data を探す
//    char* pBuffer = nullptr;
//    unsigned int dataSize = 0;
//
//    while (file.read((char*)&chunk, sizeof(chunk))) {
//        if (strncmp(chunk.id, "fmt ", 4) == 0) {
//            assert(chunk.size <= sizeof(WAVEFORMATEX));
//            file.read((char*)&format.fmt, chunk.size);
//        } else if (strncmp(chunk.id, "data", 4) == 0) {
//            pBuffer = new char[chunk.size];
//            file.read(pBuffer, chunk.size);
//            dataSize = chunk.size;
//        } else {
//            // 他のチャンク（JUNKなど）はスキップ
//            file.seekg(chunk.size, std::ios::cur);
//        }
//
//        // fmt も data も読み込めたら終わり
//        if (format.fmt.nChannels != 0 && pBuffer != nullptr) {
//            break;
//        }
//    }
//
//    file.close();
//
//    assert(format.fmt.nChannels != 0); // fmt チャンクが見つからなかった
//    assert(pBuffer != nullptr); // data チャンクが見つからなかった
//
//    SoundData soundData = {};
//    soundData.wfex = format.fmt;
//    soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
//    soundData.bufferSize = dataSize;
//    return soundData;
//}

////==音声データ解放==//
//void SoundUnload(SoundData* soundData)
//{
//
//    // バッファのメモリを解放
//    delete[] soundData->pBuffer;
//
//    soundData->pBuffer = 0;
//    soundData->bufferSize = 0;
//    soundData->wfex = {};
//}
//void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData)
//{
//    HRESULT result;
//
//    // 波形フォーマットをもとにsourceVoiceの生成
//    IXAudio2SourceVoice* pSourceVoice = nullptr;
//    result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
//    assert(SUCCEEDED(result));
//
//    // 再生する波形データの設定
//    XAUDIO2_BUFFER buf{};
//    buf.pAudioData = soundData.pBuffer;
//    buf.AudioBytes = soundData.bufferSize;
//    buf.Flags = XAUDIO2_END_OF_STREAM;
//
//    // 波形データの再生
//    result = pSourceVoice->SubmitSourceBuffer(&buf);
//    result = pSourceVoice->Start();
//}
//
//// 音声読み込み
//SoundData soundData1 = SoundLoadWave("Resources/BGM.wav");

// 音声再生

////////////////
// main関数/////-------------------------------------------------------------------------------------------------
//  Windwsアプリでの円とリポウント(main関数)

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    D3DResourceLeakChecker leakChecker;

    CoInitializeEx(0, COINIT_MULTITHREADED);

    // 誰も補足しなかった場合(Unhandled),補足する関数を登録
    // main関数はじまってすぐに登録するとよい
    SetUnhandledExceptionFilter(ExportDump);
    // ログのディレクトリを用意
    std::filesystem::create_directory("logs");
    // main関数の先頭//
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice;

    // 現在時刻を取得(UTC時刻)
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    // ログファイルの名前にコンマ何秒はいらないので削って秒にする
    std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
        nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    // 日本時間(PCの設定時間)に変換
    std::chrono::zoned_time loacalTime{ std::chrono::current_zone(), nowSeconds };
    // formatを使って年月日_時分秒の文字列に変換
    std::string dateString = std::format("{:%Y%m%d_%H%M%S}", loacalTime);
    // 時刻を使ってファイル名を決定
    std::string logFilePath = std::string("logs/") + dateString + ".log";
    // ファイルを作って書き込み準備
    std::ofstream logStream(logFilePath);
    // 出力

    WNDCLASS wc{};
    // ウィンドウプロシージャ
    wc.lpfnWndProc = WindowProc;
    // ウィンドウクラス名(何でもよい)
    wc.lpszClassName = L"CG2WindowClass";
    // インスタンスバンドル
    wc.hInstance = GetModuleHandle(nullptr);
    // カーソル
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // ウィンドウクラスを登録する
    RegisterClass(&wc);
    // クライアント領域のサイズ
    const int32_t kClientWidth = 1280;
    const int32_t kClientHeight = 720;

    // ウィンドウサイズを表す構造体体にクライアント領域を入れる
    RECT wrc = { 0, 0, kClientWidth, kClientHeight };

    // クライアント領域をもとに実際のサイズにwrcを変更してもらう
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    // ウィンドウの生成
    HWND hwnd = CreateWindow(wc.lpszClassName, // 利用するクラス名
        L"CG2", // タイトルバーの文字(何でもよい)
        WS_OVERLAPPEDWINDOW, // よく見るウィンドウスタイル
        CW_USEDEFAULT, // 表示X座標(Windowsに任せる)
        CW_USEDEFAULT, // 表示Y座標(WindowsOSに任せる)
        wrc.right - wrc.left, // ウィンドウ横幅
        wrc.bottom - wrc.top, // ウィンドウ縦幅
        nullptr, // 親ウィンドウハンドル
        nullptr, // メニューハンドル
        wc.hInstance, // インスタンスハンドル
        nullptr); // オプション

#ifdef _DEBUG

    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr; // COM
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        // デバックレイヤーを有効化する
        debugController->EnableDebugLayer();
        // さらに6PU側でもチェックリストを行うようにする
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif // _DEBUG

    //==XAudioエンジンのインスタンスを生成==//
    HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);

    //==マスターボイスを生成==//
    result = xAudio2->CreateMasteringVoice(&masterVoice);

    //SoundPlayWave(xAudio2.Get(), soundData1); // 音声再生

    // ウィンドウを表示する
    ShowWindow(hwnd, SW_SHOW);

    // DXGIファクトリーの生成
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr; // com
    // HRESULTはWindows系のエラー子どであり
    // 関数が成功したかをSUCCEEDEDマクロで判定できる
    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
    // 初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、どうにもできない場合が多いのでassertにしておく
    assert(SUCCEEDED(hr));
    // 使用するアダプタ用の変数,最初にnullptrを入れておく
    IDXGIAdapter4* useAdapter = nullptr; // com
    // よい順にアダプタを頼む
    for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(
        i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        IID_PPV_ARGS(&useAdapter))
        != DXGI_ERROR_NOT_FOUND;
        ++i) {

        // アダプターの情報を取得する
        DXGI_ADAPTER_DESC3 adapterDesc{}; // com
        hr = useAdapter->GetDesc3(&adapterDesc); // comGet
        assert(SUCCEEDED(hr)); // 取得できないのは一大事
        // ソフトウェアアダプタでなければ採用!
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) { // get
            // 採用したアダプタの情報をログに出力wstringの方なので注意
            Log(logStream,
                ConvertString(std::format(L"Use Adapater:{}\n",
                    adapterDesc.Description))); // get
            break;
        }
        useAdapter = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
    }

    // 適切なアダプタが見つからなかったので起動できない
    assert(useAdapter != nullptr);
    Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
    // 昨日レベルとログ出力用の文字列
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
    };

    const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };
    // 高い順に生成できるか試していく
    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        // 採用したアダプターでデバイスを生成
        hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
        // 指定した昨日レベルでデバイスは生成できたか確認
        if (SUCCEEDED(hr)) {
            // 生成できたのでログ出力を行ってループを抜ける
            Log(logStream,
                std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
            break;
        }
    }
    // デバイスの生成が上手くいかなかったので起動できない
    assert(device != nullptr);
    Log(logStream, "Complete create D3D12Device!!!\n"); // 初期化完了のログを出す

#ifdef _DEBUG

    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        // やばいエラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        // エラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        // 警告時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        // 抑制するメッセージのＩＤ
        D3D12_MESSAGE_ID denyIds[] = {
            // windows11でのDXGIデバックレイヤーとDX12デバックレイヤーの相互作用バグによるエラーメッセージ
            // https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
        };
        // 抑制するレベル
        D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        // 指定したメッセージの表示wp抑制する
        infoQueue->PushStorageFilter(&filter);
        // 解放
        /*  infoQueue->Release();*/
    }

#endif // DEBUG

    // コマンドキューを生成する
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>
        commandQueue = nullptr; // com
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    hr = device->CreateCommandQueue(&commandQueueDesc,
        IID_PPV_ARGS(&commandQueue));
    // コマンドキューの生成が上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));
    // コマンドアロケーターを生成する
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr; // com
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandAllocator));
    // コマンドキューアロケーターの生成があ上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));

    // コマンドリストを生成する
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr; // com
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator.Get(), nullptr, // get
        IID_PPV_ARGS(&commandList));
    // コマンドリストの生成が上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));

    // スワップチェーンを生成する
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain = nullptr; // com
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = kClientWidth; // 画面の幅。ウィンドウのクライアント領域を同じものんにしておく
    swapChainDesc.Height = kClientHeight; // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
    swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとしてりようする
    swapChainDesc.BufferCount = 2; // ダブルバッファ
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニターに移したら,中身を吐き
    // コマンドキュー,ウィンドウバンドル、設定を渡して生成する
    hr = dxgiFactory->CreateSwapChainForHwnd(
        commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr,
        reinterpret_cast<IDXGISwapChain1**>(
            swapChain.GetAddressOf())); // com.Get,OF
    assert(SUCCEEDED(hr));

    // RTV用のヒープでディスクリプタの数は２。RTVはSHADER内で触るものではないので、shaderVisivleはfalse02_02
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = // com
        CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2,
            false);

    // DSV用のヒープでディスクリプタの数は１。DSVはshader内で触るものではないので,ShaderVisibleはfalse
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap = // com
        CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1,
            false);

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = // com
        CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            128, true);

    // SwapChainからResourceを引っ張ってくる
    Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2] = {
        nullptr
    }; // com
    hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
    // 上手く取得できなければ起動できない
    assert(SUCCEEDED(hr));
    hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
    assert(SUCCEEDED(hr));

    // RTVの設定
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    // ディスクリプタの先頭を取得する
    D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    // RTVを2つ作るのでディスクリプタを２つ用意
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
    // まず１つ目をつくる。１つ目は最初のところに作る。作る場所をこちらで指定して上げる必要がある
    rtvHandles[0] = rtvStartHandle;
    device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc,
        rtvHandles[0]);
    // 2つ目のディスクリプタハンドルを得る（自力で）
    rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    // 2つ目を作る
    device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc,
        rtvHandles[1]);

    // 初期値でFenceを作る01_02
    Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr; // com
    uint64_t fenceValue = 0;
    hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence));
    assert(SUCCEEDED(hr));

    // FenceのSignalを待つためのイベントを作成する01_02
    HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(fenceEvent != nullptr);

    // dxcCompilerを初期化CG2_02_00
    IDxcUtils* dxcUtils = nullptr;
    IDxcCompiler3* dxcCompiler = nullptr;
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
    assert(SUCCEEDED(hr));

    // 現時点でincludeはしないがincludeに対応するための設定を行っておく
    IDxcIncludeHandler* includHandler = nullptr;
    hr = dxcUtils->CreateDefaultIncludeHandler(&includHandler);
    assert(SUCCEEDED(hr));
    // ==== ルートシグネチャを作る準備 ====
    // RootSignature作成02_00
    // 頂点データの形式を使っていいよ！というフラグを立てる
    // ルート何？03_00
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0〜1の範囲外をリピート
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipMapを使う
    staticSamplers[0].ShaderRegister = 0; // レジスタ番号0を使う
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う

    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);
    // RootParameter作成。複数設定できるので配列。今回は結果１つだけなので長さ１の配列
    // PixelShaderのMaterialとVertexShaderのTransform
    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
    rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号０とバインド
    // ここから[2]
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // Vertexshaderで使う
    rootParameters[1].Descriptor.ShaderRegister = 0; // 得wジスタ番号０を使う
    // ここまで[2]
    // 新しいディスクリプタレンジ03_00
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
    descriptorRange[0].NumDescriptors = 1; // 数は1つ
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算
    descriptionRootSignature.pParameters = rootParameters; // ルートパラメータ配列へのポインタ
    descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

    // 新しいディスクリプタレンジ03_00
    // ここから[3]03_00
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // Tableで利用する数
    // ここまで[3]//05_03追加しろー
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PxelShaderで使う
    rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号１を使う
    // ==== シリアライズしてバイナリにする（GPUが読める形に変換） ====
    // バイナリになるデータを入れるための箱02_00
    ID3DBlob* signatureBlob = nullptr; // ルートシグネチャ本体
    ID3DBlob* errorBlob = nullptr; // エラー内容が入るかも
    // GPUが読めるようにデータ変換！（バイナリ化）
    hr = D3D12SerializeRootSignature(&descriptionRootSignature,
        D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob,
        &errorBlob);

    // もし失敗したら、エラーメッセージを出して止める
    if (FAILED(hr)) {
        Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer())); // エラーをログに出す
        assert(false); // 絶対成功してないと困るので、止める
    }

    // バイナリをもとに生成02_00
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr; // com
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));

    // Textureを読んで転送する03_00
    DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = // com
        CreateTextureResource(device.Get(), metadata); // get
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(textureResource.Get(), mipImages, device.Get(),
        commandList.Get()); //?
    // モデル読み込み
    ModelData modelData = LoadOjFile("resources", "Plane.obj");

    std::cout << "テクスチャファイルパス: " << modelData.material.textureFilePath
        << std::endl;

    if (!std::filesystem::exists(modelData.material.textureFilePath)) {
        std::cerr << "ファイルが存在しません！" << std::endl;
    }

    // 2枚目のTextureを読んで転送するCG2_05_01_page_8
    DirectX::ScratchImage mipImages2 = LoadTexture(modelData.material.textureFilePath);

    const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 = // com
        CreateTextureResource(device.Get(), metadata2); // get
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2 = UploadTextureData(textureResource2.Get(), mipImages2, device.Get(),
        commandList.Get());

    // 03_00EX
    // ID3D12Resource *intermediateResource =
    //    UploadTextureData(textureResource, mipImages, device, commandList);

#pragma region ディスクリプタサイズを取得する（SRV/RTV/DSV）
    // DescriptorSizeを取得しておくCG2_05_01_page_6
    const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
#pragma endregion

    // metaDataを基にSRVの設定03_00
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

    // metaData2を基にSRVの設定CG2_05_01_page_9
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
    srvDesc2.Format = metadata2.format;
    srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
    srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

    // SRVを作成するDescriptorHeapの場所を決める//変更CG2_05_01_0page6
    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 1);
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 1);

    // SRVを作成するDescriptorHeapの場所を決めるCG2_05_01_page_9
    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);

    // SRVの生成03_00
    device->CreateShaderResourceView(textureResource.Get(), &srvDesc,
        textureSrvHandleCPU);
    // 05_01
    device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2,
        textureSrvHandleCPU2);
    // InputLayout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    // 05_03
    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // BlendStateの設定
    D3D12_BLEND_DESC blendDesc{};
    // 全ての色要素を書き込む
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // RasiterzerStateの設定
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    // 裏面(時計回り)を表示しない
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    // 三角形の中を塗りつぶす
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    // Shaderをコンパイルする
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = CompileShader(L"Object3d.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler,
        includHandler, logStream);
    assert(vertexShaderBlob != nullptr);

    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = CompileShader(L"Object3d.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler,
        includHandler, logStream);
    assert(pixelShaderBlob != nullptr);

    // PSOを生成する
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature.Get(); // RootSignatrue
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
    graphicsPipelineStateDesc.VS = {
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize()
    }; // VertexShader
    graphicsPipelineStateDesc.PS = {
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize()
    }; // PixelShader
    graphicsPipelineStateDesc.BlendState = blendDesc; // BlensState
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState

    // DepthStencillStateの設定
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    // depthの機能を有効かする
    depthStencilDesc.DepthEnable = true;
    // 書き込みします
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    // 比較関数はLessEqual。つまり、近ければ描画される
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    // depthStenncillの設定
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // 書き込むRTVの情報
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    // 利用するトポロジ(形状)のタイプ。三角形
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // どのように画面に色を打ち込むかの設定(気にしなくて良い)
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    //////////////
    // 実際に生成//
    //////////////
    ////--------------------------
    //// 通常モデル用リソース
    ////--------------------------
    // ID3D12Resource *vertexResource =
    //     CreateBufferResource(device, sizeof(VertexData) * kNumVertices);
    // VertexData *vertexData = nullptr;
    // vertexResource->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));

    //--------------------------------------------------
    // modelDataを使う
    //--------------------------------------------------

    // 頂点リソースを作る
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = CreateBufferResource(
        device.Get(), sizeof(VertexData) * modelData.vertices.size());
    // 頂点バッファービューを作成する
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress(); // リソース先頭のアドレスを使う
    vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size()); // 使用するリソースの頂点のサイズ
    vertexBufferView.StrideInBytes = sizeof(VertexData); // 1頂点あたりのサイズ
    // 頂点リソースにデータを書き込む
    VertexData* vertexData = nullptr;
    vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    std::memcpy(vertexData, modelData.vertices.data(),
        sizeof(VertexData) * modelData.vertices.size()); // 頂点データをリソースにコピー

    ////--------------------------
    //// 三角形の頂点データ
    ////--------------------------
    //// 左下
    // vertexData[0].position = {-0.5f, -0.5f, 0.0f, 1.0f};
    // vertexData[0].texcoord = {0.0f, 1.0f};
    ////  上
    // vertexData[1].position = {0.0f, 0.5f, 0.0f, 1.0f};
    // vertexData[1].texcoord = {0.5f, 0.0f};
    ////  右下
    // vertexData[2].position = {0.5f, -0.5f, 0.0f, 1.0f};
    // vertexData[2].texcoord = {1.0f, 1.0f};

    //// 左下２03_01_Other
    // vertexData[3].position = {-0.5f, -0.5f, 0.5f, 1.0f};
    // vertexData[3].texcoord = {0.0f, 1.0f};

    //// 上２
    // vertexData[4].position = {0.0f, 0.0f, 0.0f, 1.0f};
    // vertexData[4].texcoord = {0.5f, 0.0f};
    //// 右下２
    // vertexData[5].position = {0.5f, -0.5f, -0.5f, 1.0f};
    // vertexData[5].texcoord = {1.0f, 1.0f};
    //--------------------------
    //  頂点バッファービュー
    //--------------------------
    ////   頂点バッファビューを作成する
    // D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    //// リソースの先頭のアドレスから使う
    // vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
    //// 使用するリソースのサイズは頂点３つ分のサイズ
    // vertexBufferView.SizeInBytes = sizeof(VertexData) * kNumVertices;
    //// 1頂点あたりのサイズ
    // vertexBufferView.StrideInBytes = sizeof(VertexData);
    //--------------------------
    //  マテリアル
    //--------------------------
    //   マテリアル用のリソースを作る今回はcolor一つ分のサイズを用意する05_03
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = CreateBufferResource(device.Get(), sizeof(Material));
    // マテリアルにデータを書き込む
    Material* materialData = nullptr;
    // 書き込むためのアドレスを取得
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    // 今回は赤を書き込んでみる
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData->uvTransform = MakeIdentity4x4(); // 06_01_UuvTransform行列を単位行列で初期化
    materialData->enableLighting = true;
    //--------------------------
    // WVP行列
    //--------------------------
    // WVPリソースを作る02_02
    Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = CreateBufferResource(device.Get(), sizeof(TransformationMatrix));
    // データを書き込む02_02
    TransformationMatrix* wvpData = nullptr;
    // 書き込むためのアドレスを取得02_02
    wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
    // 単位行列を書き込んでおく02_02
    Matrix4x4 identity = MakeIdentity4x4();
    // 05_03
    memcpy(&wvpData->WVP, &identity, sizeof(Matrix4x4));
    memcpy(&wvpData->World, &identity, sizeof(Matrix4x4));
    //--------------------------
    // Sprite用リソース
    //--------------------------
    // sprite用の頂点リソースを作る04_00
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = CreateBufferResource(device.Get(), sizeof(VertexData) * 4);
    // sprite用の頂点リソースにデータを書き込む04_00
    VertexData* vertexDataSprite = nullptr;
    //  書き込むためのアドレスを取得04_00
    vertexResourceSprite->Map(
        0, nullptr,
        reinterpret_cast<void**>(&vertexDataSprite)); // 04_00
    // 6頂点を4頂点にする
    vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f }; // 左下
    vertexDataSprite[0].texcoord = { 0.0f, 1.0f };

    vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
    vertexDataSprite[1].texcoord = { 0.0f, 0.0f };

    vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f }; // 右下
    vertexDataSprite[2].texcoord = { 1.0f, 1.0f };

    vertexDataSprite[3].position = { 640.0f, 0.0f, 0.0f, 1.0f }; // 右上
    vertexDataSprite[3].texcoord = { 1.0f, 0.0f };

    // sprite用の頂点バッファビューを作成する04_00
    D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
    // sprite用のリソースの先頭のアドレスから使う04_00
    vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
    // sprite用の使用するリーソースのサイズは頂点6つ分のサイズ04_00//06_00ここ４にしました
    vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;
    // sprite用の１頂点当たりのサイズ04_00
    vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

    // 06_00_page6
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = CreateBufferResource(device.Get(), sizeof(uint32_t) * 6);
    uint32_t* indexDataSprite = nullptr;
    // インデックスリソースにデータを書き込む uint32_t *indexDataSprite =
    // nullptr;
    indexResourceSprite->Map(0, nullptr,
        reinterpret_cast<void**>(&indexDataSprite));
    indexDataSprite[0] = 0;
    indexDataSprite[1] = 1;
    indexDataSprite[2] = 2;
    indexDataSprite[3] = 1;
    indexDataSprite[4] = 3;
    indexDataSprite[5] = 2;

    // Viewを作成する06_00_page6
    D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
    // リソースの先頭のアドレスから使う
    indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
    // 使用するリソースのサイズはインデックス６つ分のサイズ
    indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
    // インデックスはuint32_tとする
    indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

    // Sprite用のマテリアルリソースを作る05_03
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = CreateBufferResource(device.Get(), sizeof(Material));
    // Sprite用のマテリアルにデータを書き込む
    Material* materialDataSprite = nullptr;
    // 書き込むためのアドレスを取得
    materialResourceSprite->Map(0, nullptr,
        reinterpret_cast<void**>(&materialDataSprite));
    // 今回は白を設定
    materialDataSprite->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialDataSprite->uvTransform = MakeIdentity4x4(); // 06_01//同じ
    materialDataSprite->enableLighting = false; // kokomonstrball?

    // sprite用のTransfomationMatrix用のリソースを作る。Matrix4x4
    // 1つ分のサイズを用意する04_00
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = CreateBufferResource(device.Get(), sizeof(TransformationMatrix));
    // sprite用のデータを書き込む04_00
    TransformationMatrix* transformationMatrixDataSprite = nullptr;
    // sprite用の書き込むためのアドレスを取得04_00
    transformationMatrixResourceSprite->Map(
        0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
    // 単位行列を書き込んでおく04_00//これいったん消しました05_03
    // *transformationMatrixDataSprite = MakeIdentity4x4();

    //--------------------------
    // 共通リソース
    //--------------------------
    // 03_01_Other
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencillResource = CreateDepthStencilTextureResource(device.Get(), kClientWidth,
        kClientHeight);
    // DSVの設定
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    // DSVHeapの先端にDSVを作る
    device->CreateDepthStencilView(
        depthStencillResource.Get(), &dsvDesc,
        dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // 平行光源用の定数バッファ（CBV）を作成（バッファサイズは構造体に合わせる）05_03
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = CreateBufferResource(device.Get(), sizeof(DirectionalLight));
    // 平行光源用のデータを書き込み
    DirectionalLight* directionalLightData = nullptr;
    directionalLightResource->Map(
        0, nullptr, reinterpret_cast<void**>(&directionalLightData));
    directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色光
    directionalLightData->direction = Normalize({ 0.0f, -1.0f, 0.0f }); // 真上から下方向
    directionalLightData->intensity = 1.0f; // 標準の明るさ

    //--------------------------
    // その他リソース
    //--------------------------
    //   ビューポート
    D3D12_VIEWPORT viewport{};
    // クライアント領域のサイズと一緒にして画面全体に表示/
    viewport.Width = kClientWidth;
    viewport.Height = kClientHeight;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    // シザー矩形
    D3D12_RECT scissorRect{};
    // 基本的にビューポートと同じ矩形が構成されるようにする
    scissorRect.left = 0;
    scissorRect.right = kClientWidth;
    scissorRect.top = 0;
    scissorRect.bottom = kClientHeight;

    // 変数//
    // spriteトランスフォーム
    Transform transformSprite{
        { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }
    };
    // トランスフォーム
    Transform transform{
        { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }
    };
    // カメラトランスフォーム
    Transform cameraTransform{
        { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -5.0f }
    };
    // UVTransform用の変数を用意
    Transform uvTransformSprite{
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },
    };

    // Textureの切り替え
    bool useMonstarBall = true;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPinelineState = nullptr;
    hr = device->CreateGraphicsPipelineState(
        &graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPinelineState));
    assert(SUCCEEDED(hr));

    // スフィア作成_05_00_OTHER
    // GenerateSphereVertices(vertexData, kSubdivision, 0.5f);

    // ImGuiの初期化。詳細はさして重要ではないので解説は省略する。02_03
    // こういうもんである02_03
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsClassic();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(device.Get(), swapChainDesc.BufferCount, rtvDesc.Format,
        srvDescriptorHeap.Get(),
        srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

    MSG msg{};

    // ウィンドウの×ボタンが押されるまでループ
    while (msg.message != WM_QUIT) {

        // Windowにメッセージが来てたら最優先で処理させる
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {

            // ここがframeの先頭02_03
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            //
            // 開発用UIの処理。実際に開発用のUIを出す場合はここをげ０無固有の処理を置き換える02_03
            ImGui::
                ShowDemoWindow(); // ImGuiの始まりの場所-----------------------------

            ImGui::Begin("Materialcolor");
            ImGui::SliderFloat3("Scale", &transform.scale.x, 0.1f, 5.0f);
            ImGui::SliderAngle("RotateX", &transform.rotate.x, -180.0f, 180.0f);
            ImGui::SliderAngle("RotateY", &transform.rotate.y, -180.0f, 180.0f);
            ImGui::SliderAngle("RotateZ", &transform.rotate.z, -180.0f, 180.0f);
            ImGui::SliderFloat3("Translate", &transform.translate.x, -5.0f, 5.0f);

            /*   ImGui::ColorEdit4("Color", &(*materialData).x);*/
            ImGui::Text("useMonstarBall");
            ImGui::Checkbox("useMonstarBall", &useMonstarBall);
            ImGui::Text("LIgthng");
            ImGui::SliderFloat("x", &directionalLightData->direction.x, -10.0f,
                10.0f);
            ImGui::SliderFloat("y", &directionalLightData->direction.y, -10.0f,
                10.0f);
            ImGui::SliderFloat("z", &directionalLightData->direction.z, -10.0f,
                10.0f);
            ImGui::Text("UVTransform");
            ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f,
                -10.0f, 10.0f);
            ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f,
                10.0f);
            ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);
            ImGui::SliderFloat3("TranslateSprite", &transformSprite.translate.x, -50.0f, 500.0f);

            ImGui::End();

            // ImGuiの内部コマンドを生成する02_03
            ImGui::
                Render(); // ImGui終わりの場所。描画の前02_03--------------------------
            // 描画用のDescrriptorHeapの設定02_03
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = {
                srvDescriptorHeap
            };
            commandList->SetDescriptorHeaps(1, descriptorHeaps->GetAddressOf());

            //  ゲームの処理02_02
            //  02_02
            waveTime += 0.05f;

            //  メイクアフィンマトリックス02_02
            Matrix4x4 worldMatrix = MakeAffineMatrix(
                transform.scale, transform.rotate, transform.translate);
            // カメラのメイクアフィンマトリックス02_02
            Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate,
                cameraTransform.translate);
            // 逆行列カメラ02_02
            Matrix4x4 viewMatrix = Inverse(cameraMatrix);
            // 透視投影行列02_02
            Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
                0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);
            // ワールドビュープロジェクション行列02_02
            Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
            // CBVのバッファに書き込む02_02
            // CBVに正しい行列を書き込む
            memcpy(&wvpData->WVP, &worldViewProjectionMatrix, sizeof(Matrix4x4));

            // Sprite用のworldviewProjectionMatrixを作る04_00
            Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate,
                transformSprite.translate);
            Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
            Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(
                0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
            Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite,
                Multiply(viewMatrixSprite, projectionMatrixSprite));
            // 単位行列を書き込んでおく04_00
            transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
            transformationMatrixDataSprite->World = worldMatrixSprite;

            //-------------------------
            // UVTransform用の行列生成
            //-------------------------
            Matrix4x4 uvTransformMatrix = Matrix4x4MakeScaleMatrix(uvTransformSprite.scale);
            uvTransformMatrix = Multiply(
                uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
            uvTransformMatrix = Multiply(
                uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
            materialDataSprite->uvTransform = uvTransformMatrix;

            // 画面のクリア処理
            //   これから書き込むバックバッファのインデックスを取得
            UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
            // TransitionBarrieの設定01_02
            D3D12_RESOURCE_BARRIER barrier{};
            // 今回のバリアはTransion
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            // Noneにしておく
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            // バリアをはる対象のリソース。現在のバックバッファに対して行う
            barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
            // 遷移前(現在)のResourceState
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            // 遷移後のResourceState
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            // TransitionBarrierを張る
            commandList->ResourceBarrier(1, &barrier);

            //// 描画先のRTVうぃ設定する
            /*     commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex],
               false, nullptr);*/
               // 描画先のRTVとDSVを設定する
            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false,
                &dsvHandle);
            // 指定した色で画面全体をクリアする
            float clearColor[] = {
                0.1f, 0.25f, 0.5f,
                1.0f
            }; /// 青っぽい色RGBAの順
            /// //これ最初の文字1.0fにするとピンク画面になる
            commandList->ClearRenderTargetView(rtvHandles[backBufferIndex],
                clearColor, 0, nullptr);
            // 03_01
            commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH,
                1.0f, 0, 0, nullptr);
            // 描画
            commandList->RSSetViewports(1, &viewport); // viewportを設定
            commandList->RSSetScissorRects(1, &scissorRect); // Scirssorを設定
            // RootSignatureを設定。PS0に設定しているけど別途設定が必要
            commandList->SetGraphicsRootSignature(rootSignature.Get());
            commandList->SetPipelineState(graphicsPinelineState.Get()); // PS0を設定
            commandList->IASetVertexBuffers(0, 1, &vertexBufferView); // VBVを設定
            // 形状を設定。PS0に設定しているものとはまた別。同じものを設定すると考えていけばよい
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            commandList->SetGraphicsRootDescriptorTable(
                2, useMonstarBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);

            // マテリアルCbufferの場所を設定05_03変更
            commandList->SetGraphicsRootConstantBufferView(
                0, materialResource->GetGPUVirtualAddress()); // ここでmaterialResource使え

            // wvp用のCBufferの場所を設定02_02
            commandList->SetGraphicsRootConstantBufferView(
                1, wvpResource->GetGPUVirtualAddress());
            // 平行光源用のCbufferの場所を設定05_03
            commandList->SetGraphicsRootConstantBufferView(
                3, directionalLightResource->GetGPUVirtualAddress());

            // 描画！(DRAWCALL/ドローコール)。３頂点で１つのインスタンス。インスタンスについては今後_05_00_OHTER
            // commandList->DrawInstanced(kNumVertices, 1, 0, 0);
            // obj
            commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0,
                0); // オブジェクトのやつ
            // マテリアルCbufferの場所を設定05_03変更これ書くとUvChackerがちゃんとする
            commandList->SetGraphicsRootConstantBufferView(
                0, materialResourceSprite->GetGPUVirtualAddress()); // ここでmaterialResource使え

            // 描画
            commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
            // spriteの描画04_00
            commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
            // IBVを設定
            commandList->IASetIndexBuffer(&indexBufferViewSprite);

            commandList->SetGraphicsRootConstantBufferView(
                1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
            // UvChecker
            commandList->DrawIndexedInstanced(6, 1, 0, 0, 0); // 左上のやつ

            //  描画の最後です//----------------------------------------------------
            //   実際のcommandListのImGuiの描画コマンドを積む
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

            //  画面に描く処理は全て終わり,画面に映すので、状態を遷移01_02
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            // TransitionBarrierを張る
            commandList->ResourceBarrier(1, &barrier);

            // コマンドリストの内容を確定させる。すべ手のコマンドを積んでからCloseすること
            hr = commandList->Close();
            assert(SUCCEEDED(hr));

            // GPUにコマンドリストの実行を行わせる;
            Microsoft::WRL::ComPtr<ID3D12CommandList> commandLists[] = { commandList };
            commandQueue->ExecuteCommandLists(1, commandLists->GetAddressOf());
            // GPUとosに画面の交換を行うよう通知する
            swapChain->Present(1, 0);
            // Fenceの値を更新01_02
            fenceValue++;
            // GPUがじじなでたどり着いたときに,Fenceの値を指定した値に代入する01_02
            commandQueue->Signal(fence.Get(), fenceValue);
            // Fenceの値が指定したSignal値にたどりついているか確認する01_02
            // GetCompleteValueの初期値はFence作成時に渡した初期値01_02
            if (fence->GetCompletedValue() < fenceValue) {

                // 指定したSignalにたどり着いていないので,たどり着くまで待つようにイベントを設定する01_02
                fence->SetEventOnCompletion(fenceValue, fenceEvent);
                // イベント待つ01_02
                WaitForSingleObject(fenceEvent, INFINITE);
            }
            // 次のｆｒａｍｅ用のコマンドりイストを準備
            hr = commandAllocator->Reset();
            assert(SUCCEEDED(hr));
            hr = commandList->Reset(commandAllocator.Get(), nullptr);
            assert(SUCCEEDED(hr));
        }
    }

    // ImGuiの終了処理。詳細はさして重要ではないので解説は省略する。
    // こういうもんである。初期化と逆順に行う
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    //  // 解放処理CG2_01_03
    CloseHandle(fenceEvent);

    // リリースする場所
    // XAudio解放
    xAudio2.Reset();
    // 音声データ開放
    /*SoundUnload(&soundData1);*/
    CoInitialize(nullptr);
    // #endif
    CloseWindow(hwnd);

    return 0;

} // 最後のカギかっこ

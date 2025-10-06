// TextureManager.h
#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"

class TextureManager {
public:
    static DirectX::ScratchImage Load(const std::string& filePath);

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(
        ID3D12Device* device,
        const DirectX::TexMetadata& metadata);

    static void UploadTextureData(
        ID3D12Resource* texture,
        const DirectX::ScratchImage& mipImages,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList);
};
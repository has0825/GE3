#include "TextureManager.h"
#include "DebugUtility.h"
#include "DirectXCore.h"
#include <cassert>
#include <stdexcept>

DirectX::ScratchImage TextureManager::Load(const std::string& filePath) {
    DirectX::ScratchImage image{};
    std::wstring filePathW = DebugUtility::ConvertString(filePath);

    // WICテクスチャのロード
    // ★★★ 修正点: 最後の引数を image から &image に変更 ★★★
    HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);

    if (FAILED(hr)) {
        std::string errorMsg = "テクスチャファイルの読み込みに失敗しました: " + filePath + "\n\nファイルが存在するか、またプロジェクトの「作業ディレクトリ」が $(SolutionDir) に設定されているか確認してください。";
        throw std::runtime_error(errorMsg);
    }

    // ミップマップの作成
    DirectX::ScratchImage mipImages{};
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
    assert(SUCCEEDED(hr));

    return mipImages;
}

Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata) {
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width);
    resourceDesc.Height = UINT(metadata.height);
    resourceDesc.MipLevels = UINT16(metadata.mipLevels);
    resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
    resourceDesc.Format = metadata.format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

void TextureManager::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);

    uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, static_cast<UINT>(subresources.size()));
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = DirectXCore::GetInstance()->CreateBufferResource(intermediateSize);

    UpdateSubresources(commandList, texture, intermediateResource.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    commandList->ResourceBarrier(1, &barrier);
}
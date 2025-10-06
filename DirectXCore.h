// DirectXCore.h
#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <vector>
#include <string> // 追加
#include <format> // 追加
#include "externals/DirectXTex/d3dx12.h"

class DirectXCore {
public:
    static DirectXCore* GetInstance();

    void Initialize();
    void Finalize();

    // 描画前処理
    void PreDraw();
    // 描画後処理
    void PostDraw();

    // Getter
    ID3D12Device* GetDevice() const { return device_.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

    // バッファリソース作成
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
    // ディスクリプタヒープ作成
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
    // 深度ステンシルテクスチャ作成
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(int32_t width, int32_t height);
    // CPUディスクリプタハンドルの取得
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, UINT descriptorSize, uint32_t index);
    // GPUディスクリプタハンドルの取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, UINT descriptorSize, uint32_t index);

private:
    DirectXCore() = default;
    ~DirectXCore() = default;
    DirectXCore(const DirectXCore&) = delete;
    const DirectXCore& operator=(const DirectXCore&) = delete;

    void InitializeDXGIDevice();
    void InitializeCommand();
    void CreateSwapChain();
    void CreateRenderTargets();
    void CreateDepthBuffer();
    void CreateFence();

    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> swapChainResources_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;

    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    UINT64 fenceValue_ = 0;
    HANDLE fenceEvent_ = nullptr;
};
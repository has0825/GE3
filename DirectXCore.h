#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <cstdint>

class WinApp;

// DirectX 12のコア機能を管理するクラス
class DirectXCore {
public:
    // シングルトンインスタンスを取得
    static DirectXCore* GetInstance();

    // 初期化
    void Initialize();
    // 終了処理
    void Finalize();

    // 描画前処理
    void PreDraw();
    // 描画後処理
    void PostDraw();

    // 各種ゲッター
    ID3D12Device* GetDevice() const { return device_.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

    // バッファリソース作成用のヘルパー関数
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

private:
    // シングルトンにするためのコンストラクタ、デストラクタのプライベート化
    DirectXCore() = default;
    ~DirectXCore() = default;
    DirectXCore(const DirectXCore&) = delete;
    const DirectXCore& operator=(const DirectXCore&) = delete;

    WinApp* winApp_ = nullptr;

    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
    Microsoft::WRL::ComPtr<ID3D12Resource> backBuffers_[2];
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2];

    // GPUとの同期用フェンス
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    uint64_t fenceValue_ = 0;
    HANDLE fenceEvent_ = nullptr;

    // 深度バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
};
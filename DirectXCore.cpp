// DirectXCore.cpp
#include "DirectXCore.h"
#include "WinApp.h"
#include "DebugUtility.h"
#include <cassert>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

// (実装は変更なし)
// ... 以前の回答と同じ内容 ...
DirectXCore* DirectXCore::GetInstance() {
    static DirectXCore instance;
    return &instance;
}

void DirectXCore::Initialize() {
    InitializeDXGIDevice();
    InitializeCommand();
    CreateSwapChain();
    CreateRenderTargets();
    CreateDepthBuffer();
    CreateFence();
}

void DirectXCore::Finalize() {
    // 全てのコマンドの実行を待つ
    if (fenceEvent_) {
        WaitForSingleObject(fenceEvent_, INFINITE);
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }
}

void DirectXCore::PreDraw() {
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        swapChainResources_[backBufferIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &barrier);

    // RTVとDSVを取得
    UINT rtvDescriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), rtvDescriptorSize, backBufferIndex);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // RTVとDSVをクリア
    float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // ビューポートとシザー矩形を設定
    D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)WinApp::kWindowWidth, (float)WinApp::kWindowHeight);
    commandList_->RSSetViewports(1, &viewport);
    D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, WinApp::kWindowWidth, WinApp::kWindowHeight);
    commandList_->RSSetScissorRects(1, &scissorRect);
}

void DirectXCore::PostDraw() {
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        swapChainResources_[backBufferIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    commandList_->ResourceBarrier(1, &barrier);

    HRESULT hr = commandList_->Close();
    assert(SUCCEEDED(hr));

    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(_countof(commandLists), commandLists);

    swapChain_->Present(1, 0);

    // フェンスを使ってGPUの処理完了を待つ
    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));
}

void DirectXCore::InitializeDXGIDevice() {
#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif

    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
    assert(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter;
    for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC3 adapterDesc{};
        hr = useAdapter->GetDesc3(&adapterDesc);
        assert(SUCCEEDED(hr));
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            DebugUtility::Log(std::cout, DebugUtility::ConvertString(std::format(L"Use Adapter:{}\n", adapterDesc.Description)));
            break;
        }
        useAdapter = nullptr;
    }
    assert(useAdapter != nullptr);

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0 };
    const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };
    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
        if (SUCCEEDED(hr)) {
            DebugUtility::Log(std::cout, std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
            break;
        }
    }
    assert(device_ != nullptr);
    DebugUtility::Log(std::cout, "Complete create D3D12Device!!!\n");

#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);

        D3D12_MESSAGE_ID denyIds[] = { D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE };
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        infoQueue->PushStorageFilter(&filter);
    }
#endif
}

void DirectXCore::InitializeCommand() {
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    HRESULT hr = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
    assert(SUCCEEDED(hr));

    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
    assert(SUCCEEDED(hr));

    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
    assert(SUCCEEDED(hr));
}

void DirectXCore::CreateSwapChain() {
    HWND hwnd = WinApp::GetInstance()->GetHwnd();
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = WinApp::kWindowWidth;
    swapChainDesc.Height = WinApp::kWindowHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
    HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1);
    assert(SUCCEEDED(hr));

    hr = swapChain1->QueryInterface(IID_PPV_ARGS(&swapChain_));
    assert(SUCCEEDED(hr));
}

void DirectXCore::CreateRenderTargets() {
    rtvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

    swapChainResources_.resize(2);
    UINT descriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (UINT i = 0; i < 2; ++i) {
        HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
        assert(SUCCEEDED(hr));

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), descriptorSize, i);

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        device_->CreateRenderTargetView(swapChainResources_[i].Get(), &rtvDesc, rtvHandle);
    }
}

void DirectXCore::CreateDepthBuffer() {
    dsvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
    depthStencilResource_ = CreateDepthStencilTextureResource(WinApp::kWindowWidth, WinApp::kWindowHeight);

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());
}

void DirectXCore::CreateFence() {
    HRESULT hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));

    fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(fenceEvent_ != nullptr);
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCore::CreateBufferResource(size_t sizeInBytes) {
    D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = device_->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCore::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = heapType;
    desc.NumDescriptors = numDescriptors;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap));
    assert(SUCCEEDED(hr));
    return descriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCore::CreateDepthStencilTextureResource(int32_t width, int32_t height) {
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthClearValue.DepthStencil.Depth = 1.0f;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearValue,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCore::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, UINT descriptorSize, uint32_t index) {
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += (static_cast<UINT64>(descriptorSize) * index);
    return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCore::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, UINT descriptorSize, uint32_t index) {
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += (static_cast<UINT64>(descriptorSize) * index);
    return handleGPU;
}
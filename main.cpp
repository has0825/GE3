#include "WinApp.h"
#include "DirectXCore.h"
#include "DebugUtility.h"
#include "ShaderCompiler.h"
#include "TextureManager.h"
#include "Model.h"
#include "Audio.h"
#include "GraphicsData.h"
#include "MathUtility.h"

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#include <vector>

//==============================================================================================
// MyGame Class Definition
//==============================================================================================
class MyGame {
public:
    void Run();

private:
    void Initialize();
    void Finalize();
    void Update();
    void Draw();
    void InitializeImGui();
    void InitializeGraphicsPipeline();
    void CreateResources();
    void LoadAssets();
    void UpdateAnimation();
    void UpdateImGui();

    D3DResourceLeakChecker leakChecker_;

    WinApp* winApp_ = nullptr;
    DirectXCore* dxCore_ = nullptr;
    ShaderCompiler* shaderCompiler_ = nullptr;
    AudioManager* audioManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;

    Microsoft::WRL::ComPtr<ID3D12Resource> sphereVertexResource_;
    D3D12_VERTEX_BUFFER_VIEW sphereVertexBufferView_{};
    std::vector<VertexData> sphereVertices_;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
    TransformationMatrix* wvpData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
    DirectionalLight* directionalLightData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_;

    Transform cameraTransform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -5.0f} };
    Transform modelTransform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    Transform uvTransform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

    Matrix4x4 viewMatrix_;
    Matrix4x4 projectionMatrix_;

    WaveType waveType_ = WaveType::SINE;
    AnimationType animationType_ = AnimationType::NONE;
    float waveTime_ = 0.0f;
};

//==============================================================================================
// MyGame Class Implementation
//==============================================================================================
void MyGame::Run() {
    Initialize();

    while (true) {
        if (winApp_->ProcessMessages()) {
            break;
        }

        Update();
        Draw();
    }

    Finalize();
}

void MyGame::Initialize() {
    winApp_ = WinApp::GetInstance();
    winApp_->Initialize(L"CG2 Re-structured");

    dxCore_ = DirectXCore::GetInstance();
    dxCore_->Initialize();

    shaderCompiler_ = ShaderCompiler::GetInstance();
    shaderCompiler_->Initialize();

    audioManager_ = AudioManager::GetInstance();
    audioManager_->Initialize();

    InitializeImGui();
    InitializeGraphicsPipeline();
    CreateResources();
    LoadAssets();

    viewMatrix_ = MathUtility::Inverse(MathUtility::MakeAffineMatrix(cameraTransform_.scale, cameraTransform_.rotate, cameraTransform_.translate));
    projectionMatrix_ = MathUtility::MakePerspectiveFovMatrix(0.45f, (float)WinApp::kWindowWidth / WinApp::kWindowHeight, 0.1f, 100.0f);
}

void MyGame::Finalize() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    audioManager_->Finalize();
    dxCore_->Finalize();
    winApp_->Finalize();
}

void MyGame::Update() {
    UpdateImGui();
    UpdateAnimation();

    Matrix4x4 worldMatrix = MathUtility::MakeAffineMatrix(modelTransform_.scale, modelTransform_.rotate, modelTransform_.translate);
    Matrix4x4 worldViewProjectionMatrix = MathUtility::Multiply(worldMatrix, MathUtility::Multiply(viewMatrix_, projectionMatrix_));

    wvpData_->WVP = worldViewProjectionMatrix;
    wvpData_->World = worldMatrix;

    Matrix4x4 uvTransformMatrix = MathUtility::MakeAffineMatrix(uvTransform_.scale, uvTransform_.rotate, uvTransform_.translate);
    materialData_->uvTransform = uvTransformMatrix;
}

void MyGame::Draw() {
    dxCore_->PreDraw();

    ID3D12GraphicsCommandList* commandList = dxCore_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(graphicsPipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &sphereVertexBufferView_);

    // ★★★ 命令の順番を修正 ★★★
    // 先にディスクリプタヒープとテーブルを設定する
    ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_.Get() };
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetGraphicsRootDescriptorTable(2, srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart());

    // その後に定数バッファを設定する
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());
    // ★★★ ここまで変更 ★★★

    commandList->DrawInstanced(kNumSphereVertices, 1, 0, 0);

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

    dxCore_->PostDraw();
}

void MyGame::InitializeImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(winApp_->GetHwnd());

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    HRESULT hr = dxCore_->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvDescriptorHeap_));
    assert(SUCCEEDED(hr));

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU = srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU = srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart();

    ImGui_ImplDX12_Init(dxCore_->GetDevice(), 2, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, srvDescriptorHeap_.Get(), srvHandleCPU, srvHandleGPU);
}

void MyGame::InitializeGraphicsPipeline() {
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0; // t0
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    // material(b0) for Pixel Shader
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    // wvp(b0) for Vertex Shader
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 0;
    // texture(t0) for Pixel Shader
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);
    // light(b1) for Pixel Shader
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0; // s0
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        DebugUtility::Log(std::cerr, static_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }
    hr = dxCore_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;

    // シェーダーファイル名はご自身の環境に合わせてください (ObjVS.hlsl or Object3d.VS.hlsl)
    auto vsBlob = shaderCompiler_->Compile(L"resources/shaders/Object3d.VS.hlsl", L"vs_6_0");
    auto psBlob = shaderCompiler_->Compile(L"resources/shaders/Object3d.PS.hlsl", L"ps_6_0");
    graphicsPipelineStateDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    graphicsPipelineStateDesc.BlendState = CD3D12_BLEND_DESC(D3D12_DEFAULT);
    graphicsPipelineStateDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    graphicsPipelineStateDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    hr = dxCore_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
    assert(SUCCEEDED(hr));
}

void MyGame::CreateResources() {
    GeometryGenerator::GenerateSphereVertices(sphereVertices_, kSubdivision, 1.0f);
    sphereVertexResource_ = dxCore_->CreateBufferResource(sizeof(VertexData) * kNumSphereVertices);
    sphereVertexBufferView_.BufferLocation = sphereVertexResource_->GetGPUVirtualAddress();
    sphereVertexBufferView_.SizeInBytes = sizeof(VertexData) * kNumSphereVertices;
    sphereVertexBufferView_.StrideInBytes = sizeof(VertexData);
    VertexData* vertexData = nullptr;
    sphereVertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    std::memcpy(vertexData, sphereVertices_.data(), sizeof(VertexData) * kNumSphereVertices);

    materialResource_ = dxCore_->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = true;
    materialData_->uvTransform = MathUtility::MakeIdentity4x4();

    wvpResource_ = dxCore_->CreateBufferResource(sizeof(TransformationMatrix));
    wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
    wvpData_->WVP = MathUtility::MakeIdentity4x4();
    wvpData_->World = MathUtility::MakeIdentity4x4();

    directionalLightResource_ = dxCore_->CreateBufferResource(sizeof(DirectionalLight));
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
    directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
    directionalLightData_->intensity = 1.0f;
}

void MyGame::LoadAssets() {
    DirectX::ScratchImage mipImages = TextureManager::Load("resources/uvChecker.png");
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    textureResource_ = TextureManager::CreateTextureResource(dxCore_->GetDevice(), metadata);
    TextureManager::UploadTextureData(textureResource_.Get(), mipImages, dxCore_->GetDevice(), dxCore_->GetCommandList());

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);
    dxCore_->GetDevice()->CreateShaderResourceView(textureResource_.Get(), &srvDesc, srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());
}

void MyGame::UpdateAnimation() {
    waveTime_ += 0.01f;

    switch (animationType_) {
    case AnimationType::RESET:
        modelTransform_ = { {1, 1, 1}, {0, 0, 0}, {0, 0, 0} };
        materialData_->color = { 1, 1, 1, 1 };
        animationType_ = AnimationType::NONE;
        break;

    case AnimationType::COLOR:
        materialData_->color.x = std::abs(std::sin(waveTime_ * 2.0f));
        materialData_->color.y = std::abs(std::cos(waveTime_));
        materialData_->color.z = std::abs(std::sin(waveTime_ * 0.5f));
        break;

    case AnimationType::SCALE:
        modelTransform_.scale.x = 1.0f + 0.5f * std::sin(waveTime_);
        modelTransform_.scale.y = 1.0f + 0.5f * std::sin(waveTime_);
        modelTransform_.scale.z = 1.0f + 0.5f * std::sin(waveTime_);
        break;

    case AnimationType::ROTATE:
        modelTransform_.rotate.y += 0.02f;
        break;

    case AnimationType::TRANSLATE:
        modelTransform_.translate.y = std::sin(waveTime_) * 2.0f;
        break;

    case AnimationType::ALL:
        materialData_->color.x = std::abs(std::sin(waveTime_ * 2.0f));
        materialData_->color.y = std::abs(std::cos(waveTime_));
        materialData_->color.z = std::abs(std::sin(waveTime_ * 0.5f));
        modelTransform_.scale.x = 1.0f + 0.5f * std::sin(waveTime_);
        modelTransform_.scale.y = 1.0f + 0.5f * std::sin(waveTime_);
        modelTransform_.scale.z = 1.0f + 0.5f * std::sin(waveTime_);
        modelTransform_.rotate.y += 0.02f;
        modelTransform_.translate.y = std::sin(waveTime_) * 2.0f;
        break;

    case AnimationType::NONE:
    default:
        break;
    }
}

void MyGame::UpdateImGui() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Animation Controls");
    ImGui::Text("Select Animation Type:");
    if (ImGui::RadioButton("None", animationType_ == AnimationType::NONE)) { animationType_ = AnimationType::NONE; }
    if (ImGui::RadioButton("Color", animationType_ == AnimationType::COLOR)) { animationType_ = AnimationType::COLOR; }
    if (ImGui::RadioButton("Scale", animationType_ == AnimationType::SCALE)) { animationType_ = AnimationType::SCALE; }
    if (ImGui::RadioButton("Rotate", animationType_ == AnimationType::ROTATE)) { animationType_ = AnimationType::ROTATE; }
    if (ImGui::RadioButton("Translate", animationType_ == AnimationType::TRANSLATE)) { animationType_ = AnimationType::TRANSLATE; }
    if (ImGui::RadioButton("All", animationType_ == AnimationType::ALL)) { animationType_ = AnimationType::ALL; }
    if (ImGui::Button("Reset")) { animationType_ = AnimationType::RESET; }
    ImGui::Separator();
    ImGui::ColorEdit4("Object Color", &materialData_->color.x);
    ImGui::DragFloat3("Scale", &modelTransform_.scale.x, 0.01f);
    ImGui::DragFloat3("Rotate", &modelTransform_.rotate.x, 0.01f);
    ImGui::DragFloat3("Translate", &modelTransform_.translate.x, 0.01f);
    ImGui::Separator();
    ImGui::DragFloat3("Light Direction", &directionalLightData_->direction.x, 0.01f);
    directionalLightData_->direction = MathUtility::Normalize(directionalLightData_->direction);
    ImGui::SliderFloat("Light Intensity", &directionalLightData_->intensity, 0.0f, 2.0f);
    ImGui::End();
}

//==============================================================================================
// Entry Point (WinMain)
//==============================================================================================
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    SetUnhandledExceptionFilter(DebugUtility::ExportDump);

    try {
        MyGame game;
        game.Run();
    }
    catch (const std::exception& e) {
        DebugUtility::Log(std::cerr, e.what());
        return 1;
    }

    return 0;
}
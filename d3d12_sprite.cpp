#include "common.hpp"
#include "d3d12_sprite.hpp"
#include "d3d12_common.hpp"
#include "paths.hpp"

#include <d3d12.h>
#include <directx/d3dx12.h>

using namespace okami;
using namespace DirectX;

constexpr D3D12_INPUT_ELEMENT_DESC kInputLayout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "ROTATION", 0, DXGI_FORMAT_R32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "ORIGIN", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

Expected<ComPtr<ID3D12RootSignature>> SpriteRenderer::CreateRootSignature(ID3D12Device& device)
{
    // Define root parameters
    CD3DX12_ROOT_PARAMETER1 rootParameters[3];

    // b0: Globals constant buffer
    rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);

    // t1: Sprite texture
    CD3DX12_DESCRIPTOR_RANGE1 textureRange;
    textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    rootParameters[1].InitAsDescriptorTable(1, &textureRange, D3D12_SHADER_VISIBILITY_PIXEL);

    // s0: Sprite sampler
    CD3DX12_DESCRIPTOR_RANGE1 samplerRange;
    samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0);
    rootParameters[2].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

    // Create the root signature
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(
        _countof(rootParameters), rootParameters,
        0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);
    if (FAILED(hr)) {
        if (error) {
            std::string errorMsg(static_cast<char*>(error->GetBufferPointer()), error->GetBufferSize());
            return std::unexpected(Error("Failed to serialize root signature: " + errorMsg));
        }
        return std::unexpected(Error("Failed to serialize root signature: " + std::to_string(hr)));
    }

    ComPtr<ID3D12RootSignature> rootSignature;
    hr = device.CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) {
        return std::unexpected(Error("Failed to create root signature: " + std::to_string(hr)));
    }

    return rootSignature;
}

Expected<std::shared_ptr<SpriteRenderer>> SpriteRenderer::Create(
    ID3D12Device& device,
    std::shared_ptr<TextureManager> manager,
    std::shared_ptr<DescriptorPool> samplerPool,
    DirectX::RenderTargetState rts,
    int bufferCount)
{
    auto renderer = std::shared_ptr<SpriteRenderer>(new SpriteRenderer());

    // Create root signature
    auto rootSigResult = CreateRootSignature(device);
    if (!rootSigResult) {
        return std::unexpected(rootSigResult.error());
    }
    renderer->m_rootSignature = rootSigResult.value();

    // Load vertex shader
    auto vertexShader = LoadShaderFromFile(GetShaderPath("sprite_vs.cso"));
    if (!vertexShader.has_value()) {
        return std::unexpected(vertexShader.error());
    }

    // Load geometry shader
    auto geometryShader = LoadShaderFromFile(GetShaderPath("sprite_gs.cso"));
    if (!geometryShader.has_value()) {
        return std::unexpected(geometryShader.error());
    }

    // Load pixel shader
    auto pixelShader = LoadShaderFromFile(GetShaderPath("sprite_ps.cso"));
    if (!pixelShader.has_value()) {
        return std::unexpected(pixelShader.error());
    }

    // Create pipeline state
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { kInputLayout, _countof(kInputLayout) };
    psoDesc.pRootSignature = renderer->m_rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Get());
    psoDesc.GS = CD3DX12_SHADER_BYTECODE(geometryShader->Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    
    // Enable alpha blending for sprites
    psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    psoDesc.NumRenderTargets = rts.numRenderTargets;
    
    for (size_t i = 0; i < rts.numRenderTargets; ++i) {
        psoDesc.RTVFormats[i] = rts.rtvFormats[i];
    }
    
    psoDesc.DSVFormat = rts.dsvFormat;
    psoDesc.SampleDesc.Count = rts.sampleDesc.Count;
    psoDesc.SampleDesc.Quality = rts.sampleDesc.Quality;

    auto hr = device.CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&renderer->m_pipelineState));
    if (FAILED(hr)) {
        return std::unexpected(Error("Failed to create pipeline state: " + std::to_string(hr)));
    }

    // Initialize per-frame data
    renderer->m_perFrameData.resize(bufferCount);
    for (int i = 0; i < bufferCount; ++i) {
        auto globalConstantsResult = UploadBuffer<hlsl::Globals>::Create(device, UploadBufferType::Constant);
        if (!globalConstantsResult) {
            return std::unexpected(Error("Failed to create global constants buffer: " + globalConstantsResult.error().Str()));
        }
        renderer->m_perFrameData[i].m_globalConstants = std::move(globalConstantsResult.value());

        auto instanceBufferResult = UploadBuffer<hlsl::SpriteInstance>::Create(device, UploadBufferType::Vertex, 1);
        if (!instanceBufferResult) {
            return std::unexpected(Error("Failed to create instance buffer: " + instanceBufferResult.error().Str()));
        }
        renderer->m_perFrameData[i].m_instanceBuffer = std::move(instanceBufferResult.value());
    }

    // Store texture manager
    renderer->m_textureManager = manager;
    renderer->m_samplerPool = samplerPool;

    // Create default sampler
    renderer->m_samplerHandle = renderer->m_samplerPool->Alloc();
    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    device.CreateSampler(&samplerDesc, renderer->m_samplerPool->GetCpuHandle(renderer->m_samplerHandle));

    return renderer;
}

Error SpriteRenderer::Render(
    ID3D12Device& device,
    ID3D12GraphicsCommandList& commandList,
    hlsl::Globals const& globals,
    IStorageAccessor<Transform> const& transforms)
{
    auto& frameData = m_perFrameData[m_currentBuffer];

    // Update global constants
    {
        auto globalMap = frameData.m_globalConstants.Map();
        if (!globalMap) {
            return Error("Failed to map global constants: " + globalMap.error().Str());
        }
        **globalMap = globals;
    }

    struct Instance {
        TextureImpl const* m_texture;
        Transform m_transform;
        SpriteComponent m_sprite;
    };

    // Collect and batch sprite instances by texture
    std::vector<Instance> batchedSprites;

    auto storage = m_staticSpriteStorage.GetStorage<SpriteComponent>();

    auto const& texturesById = m_textureManager->GetTextures();

    for (auto [e, sprite] : storage) {
        // Get transform for this entity
        auto transformPtr = transforms.TryGet(e);
        const Transform& transform = transformPtr ? *transformPtr : Transform::Identity();

        // Set instance data
        auto textureIt = texturesById.find(sprite.m_texture.GetId());
        if (textureIt == texturesById.end() || 
            !textureIt->second.m_public->m_loaded.load()) {
            continue; // Texture not loaded yet
        }

        batchedSprites.push_back(Instance{&textureIt->second, transform, sprite});
    }

    if (batchedSprites.empty()) {
        return {}; // Nothing to render
    }

    // Set pipeline state and root signature
    commandList.SetPipelineState(m_pipelineState.Get());
    commandList.SetGraphicsRootSignature(m_rootSignature.Get());

    // Set primitive topology
    commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

    // Bind global constants
    commandList.SetGraphicsRootConstantBufferView(0, frameData.m_globalConstants.GetGPUAddress());

    // Bind descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { 
        m_textureManager->GetSrvHeap(), 
        m_samplerPool->GetHeap()
    };
    commandList.SetDescriptorHeaps(2, heaps);

    // Bind default sampler
    commandList.SetGraphicsRootDescriptorTable(2, m_samplerPool->GetGpuHandle(m_samplerHandle));

    std::sort(batchedSprites.begin(), batchedSprites.end(),
        [](const auto& a, const auto& b) {
            if (a.m_sprite.m_layer == b.m_sprite.m_layer) {
                return a.m_texture < b.m_texture; // Sort by texture pointer if layers are equal
            }
            return a.m_sprite.m_layer < b.m_sprite.m_layer; // Sort by layer
        });

    frameData.m_instanceBuffer.Reserve(device, batchedSprites.size());

    auto instanceMap = frameData.m_instanceBuffer.Map();
    if (!instanceMap) {
        return Error("Failed to map instance buffer: " + instanceMap.error().Str());
    }

    // Render each batch
    size_t instanceId = 0;
    UINT bufferOffset = 0;
    for (auto batchBegin = batchedSprites.begin(); batchBegin != batchedSprites.end();) {
        auto batchEnd = std::find_if(batchBegin, batchedSprites.end(),
            [texture = batchBegin->m_texture](const Instance& instance) {
                return instance.m_texture != texture;
            });

        for (auto it = batchBegin; it != batchEnd; ++it) {
            const Instance& instance = *it;

            // Create instance data
            hlsl::SpriteInstance instanceData;
            instanceData.position = { instance.m_transform.m_position.x, instance.m_transform.m_position.y, instance.m_transform.m_position.z };
            float rotation = static_cast<float>(2.0 * glm::atan(instance.m_transform.m_rotation.z, instance.m_transform.m_rotation.w));
            glm::vec2 scale{instance.m_transform.m_scaleShear[0][0], instance.m_transform.m_scaleShear[1][1]};
            glm::vec2 imageSize;
        
            if (instance.m_sprite.m_sourceRect) {
                imageSize = instance.m_sprite.m_sourceRect->GetSize();
                auto textureSize = instance.m_sprite.m_texture->GetSize();
                instanceData.uv0 = instance.m_sprite.m_sourceRect->GetMin() / textureSize;
                instanceData.uv1 = instance.m_sprite.m_sourceRect->GetMax() / textureSize;
            } else {
                imageSize = instance.m_sprite.m_texture->GetSize();
                instanceData.uv0 = glm::vec2(0.0f, 0.0f);
                instanceData.uv1 = glm::vec2(1.0f, 1.0f);
            }

            instanceData.origin = scale * instance.m_sprite.m_origin.value_or(imageSize / 2.0f);
            instanceData.size = scale * imageSize;
            instanceData.color = instance.m_sprite.m_color;
            instanceData.rotation = rotation;

            // Update instance buffer 
            instanceMap->operator[](instanceId++) = instanceData;
        }

        UINT batchSize = static_cast<UINT>(batchEnd - batchBegin);

        // Set vertex buffers
        D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[1];
        vertexBufferViews[0].BufferLocation = frameData.m_instanceBuffer.GetGPUAddress() + sizeof(hlsl::SpriteInstance) * bufferOffset;
        vertexBufferViews[0].StrideInBytes = sizeof(hlsl::SpriteInstance);
        vertexBufferViews[0].SizeInBytes = static_cast<UINT>(batchSize * sizeof(hlsl::SpriteInstance));

        commandList.IASetVertexBuffers(0, 1, vertexBufferViews);

        // Bind texture
        auto textureHandle = batchBegin->m_texture->m_private.m_handle;
        commandList.SetGraphicsRootDescriptorTable(1, m_textureManager->GetSrvPool().GetGpuHandle(textureHandle));
        commandList.DrawInstanced(batchSize, 1, 0, 0);

        batchBegin = batchEnd;
        bufferOffset += batchSize;
    }

    // Advance to next frame buffer
    m_currentBuffer = (m_currentBuffer + 1) % static_cast<int>(m_perFrameData.size());

    return {};
}
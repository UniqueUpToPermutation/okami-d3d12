#include "d3d12_static_mesh.hpp"
#include "d3d12_mesh_formats.hpp"

#include <glog/logging.h>
#include <directx/d3dx12.h>
#include <d3dcompiler.h>
#include <unordered_set>

#include <array>

#include "paths.hpp"
#include "d3d12_common.hpp"

using namespace okami;

Expected<ComPtr<ID3D12RootSignature>> StaticMeshRenderer::CreateRootSignature(ID3D12Device& device) {
    ComPtr<ID3D12RootSignature> rootSignature;

    // Create root parameters: constant buffer for globals, shader resource view for structured buffer
    CD3DX12_ROOT_PARAMETER1 rootParameters[2];
    
    rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsShaderResourceView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX);

    // Create root signature description
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);
    if (FAILED(hr)) {
        std::string errorMsg = "Failed to serialize root signature";
        if (error) {
            errorMsg += ": " + std::string(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize());
        }
        return std::unexpected(errorMsg);
    }

    hr = device.CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) {
        return std::unexpected(Error("Failed to create root signature"));
    }

    rootSignature->SetName(L"StaticMeshRenderer Root Signature");

    return rootSignature;
}

Expected<std::shared_ptr<StaticMeshRenderer>> StaticMeshRenderer::Create(
    ID3D12Device& device,
    std::shared_ptr<GeometryManager> manager,
    DirectX::RenderTargetState rts,
    int bufferCount) {
    auto renderer = std::shared_ptr<StaticMeshRenderer>(new StaticMeshRenderer());
    renderer->m_manager = manager;

    // Load vertex shader
    auto vertexShader = LoadShaderFromFile(GetShaderPath("static_mesh_vs.cso"));
    if (!vertexShader.has_value()) {
        return std::unexpected(vertexShader.error());
    }

    // Load pixel shader
    auto pixelShader = LoadShaderFromFile(GetShaderPath("static_mesh_ps.cso"));
    if (!pixelShader.has_value()) {
        return std::unexpected(pixelShader.error());
    }

    // Create root signature
    auto rootSignature = CreateRootSignature(device);
    if (!rootSignature.has_value()) {
        return std::unexpected(rootSignature.error());
    }
    renderer->m_rootSignature = std::move(rootSignature.value());

    // Define the graphics pipeline state object description
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    auto inputLayout = GetD3D12InputLayout(kStaticMeshAttributes);
    psoDesc.InputLayout = { 
        inputLayout.data(), 
        static_cast<UINT>(inputLayout.size()) 
    };
    psoDesc.pRootSignature = renderer->m_rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = rts.numRenderTargets;
    
    for (UINT i = 0; i < rts.numRenderTargets; ++i) {
        psoDesc.RTVFormats[i] = rts.rtvFormats[i];
    }
    
    psoDesc.DSVFormat = rts.dsvFormat;
    psoDesc.SampleDesc = rts.sampleDesc;

    // Create the graphics pipeline state object
    HRESULT hr = device.CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&renderer->m_pipelineState));
    if (FAILED(hr)) {
        return std::unexpected(Error("Failed to create graphics pipeline state"));
    }

    // Create per-frame data
    for (int i = 0; i < bufferCount; ++i) {
        auto shaderConstants = UploadBuffer<hlsl::Globals>::Create(
            device, UploadBufferType::Constant, L"Static Geometry Constants Buffer");
        if (!shaderConstants.has_value()) {
            return std::unexpected(shaderConstants.error());
        }

        auto instanceBuffer = UploadBuffer<hlsl::Instance>::Create(
            device, UploadBufferType::Structured, L"Static Geometry Instance Buffer", 0);
        if (!instanceBuffer.has_value()) {
            return std::unexpected(instanceBuffer.error());
        }

        renderer->m_perFrameData.emplace_back(PerFrameData{
            .m_globalConstants = std::move(shaderConstants.value()),
            .m_instanceBuffer = std::move(instanceBuffer.value())
        });
    }

    return renderer;
}

Error StaticMeshRenderer::Render(
    ID3D12Device& device,
    ID3D12GraphicsCommandList& commandList,
    hlsl::Globals const& globals,
    IStorageAccessor<Transform> const& transforms) {
    // Render static meshes
    auto staticMeshes = m_staticMeshStorage.GetStorage<StaticMeshComponent>();

    if (staticMeshes.empty()) {
        return {};
    }

    // Set constant buffer views for globals
    auto& globalConstantsBuffer = m_perFrameData[m_currentBuffer].m_globalConstants;
    {
        auto map = globalConstantsBuffer.Map();
        if (!map.has_value()) {
            return Error("Failed to map global constants");
        }
        **map = globals;
    }

    struct MeshInstanceData {
        StaticMeshComponent m_component;
        hlsl::Instance m_instance;
    };

    std::vector<MeshInstanceData> instanceData;
    auto const& meshesById = m_manager->GetMeshes();
    // Fill instance data for all static mesh entities
    for (auto const& [entity, staticMeshComponent] : staticMeshes) {
        // Get transform for this entity
        auto transform = transforms.GetOr(entity, Transform::Identity());
        auto worldMatrix = transform.AsMatrix();

        // Set instance data
        auto meshIt = meshesById.find(staticMeshComponent.m_mesh.GetId());
        if (meshIt == meshesById.end() || 
            !meshIt->second->m_loaded.load()) {
            continue; // Geometry not loaded yet
        }

        instanceData.emplace_back(
            MeshInstanceData{
            .m_component = staticMeshComponent,
            .m_instance = hlsl::Instance{
                .m_worldMatrix = worldMatrix,
                .m_worldInverseTransposeMatrix = glm::inverse(glm::transpose(worldMatrix))
            }
        });
    }

    // Resize structured buffer if necessary to fit all mesh instances
    auto& instanceBuffer = m_perFrameData[m_currentBuffer].m_instanceBuffer;
    auto reserveResult = instanceBuffer.Reserve(device, instanceData.size());
    if (reserveResult.IsError()) {
        return Error("Failed to reserve structured buffer for static mesh instances");
    }

    if (instanceData.empty()) {
        return {}; // No instances to draw
    }

    // Sort by mesh pointer
    std::sort(instanceData.begin(), instanceData.end(), [](auto const& a, auto const& b) {
        return a.m_component < b.m_component;
    });

    instanceBuffer.Reserve(device, instanceData.size());
    if (instanceData.empty()) {
        return {}; // No instances to draw
    }
   
    // Map and fill instance data
    {
        auto map = instanceBuffer.Map();
        if (!map.has_value()) {
            return Error("Failed to map structured buffer");
        }

        // Write instance data to buffer
        int index = 0;
        for (auto const& instance : instanceData) {
            map->At(index++) = instance.m_instance;
        }
    }
    // Set the pipeline state
    commandList.SetPipelineState(m_pipelineState.Get());
    commandList.SetGraphicsRootSignature(m_rootSignature.Get());
    commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList.SetGraphicsRootConstantBufferView(0, globalConstantsBuffer.GetGPUAddress());

    UINT firstInstance = 0;
    for (auto beginIt = instanceData.begin(); beginIt != instanceData.end();) {
        auto endIt = std::find_if(beginIt, instanceData.end(), [&beginIt](const auto& pair) {
            return beginIt->m_component != pair.m_component;
        });

        auto geometryData = beginIt->m_component.m_mesh;
        auto meshIndex = beginIt->m_component.m_meshIndex;

        auto const& mesh = geometryData->m_meshes[meshIndex];
        auto const& privateData = std::any_cast<GeometryPrivate const&>(geometryData->m_privateData);

        // Set vertex buffer
        std::array<D3D12_VERTEX_BUFFER_VIEW, kStaticMeshAttributes.size()> vertexBufferViews;
        for (int i = 0; i < vertexBufferViews.size(); ++i) {
            auto& meshAttrib = mesh.m_attributes[i];
            auto& buffer = privateData.m_vertexBuffer;
            vertexBufferViews[i] = {};
            vertexBufferViews[i].BufferLocation = buffer.GetGPUAddress() + meshAttrib.m_offset;
            vertexBufferViews[i].SizeInBytes = static_cast<UINT>(mesh.m_vertexCount * meshAttrib.GetStride());
            vertexBufferViews[i].StrideInBytes = static_cast<UINT>(meshAttrib.GetStride());
        }

        commandList.IASetVertexBuffers(0, 
            static_cast<UINT>(vertexBufferViews.size()),
            vertexBufferViews.data());

        if (mesh.HasIndexBuffer()) {
            auto& buffer = *privateData.m_indexBuffer;
            D3D12_INDEX_BUFFER_VIEW indexBuffer;
            indexBuffer.BufferLocation = buffer.GetGPUAddress() + mesh.m_indices->m_offset;
            indexBuffer.SizeInBytes = static_cast<UINT>(mesh.m_indices->m_count * mesh.m_indices->GetStride());
            indexBuffer.Format = GetD3D12Format(AccessorType::Scalar, mesh.m_indices->m_type);

            // Set index buffer
            commandList.IASetIndexBuffer(&indexBuffer);
        }

        // Count instances of this mesh
        UINT instanceCount = static_cast<UINT>(endIt - beginIt);

        // Set location in the instance buffer
        commandList.SetGraphicsRootShaderResourceView(1, instanceBuffer.GetGPUAddress() + sizeof(hlsl::Instance) * firstInstance);

        // Draw with or without index buffer
        if (mesh.HasIndexBuffer()) {
            commandList.DrawIndexedInstanced(static_cast<UINT>(mesh.m_indices->m_count),
                instanceCount, 0, 0, firstInstance);
        } else {
            commandList.DrawInstanced(static_cast<UINT>(mesh.m_vertexCount), 
                instanceCount, 0, firstInstance);
        }

        firstInstance += instanceCount;
        beginIt = endIt;
    }

    // Advance to the next frame's data
    m_currentBuffer = (m_currentBuffer + 1) % m_perFrameData.size();

    return {};
}
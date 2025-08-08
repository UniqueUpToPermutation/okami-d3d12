#include "d3d12_static_mesh.hpp"

#include <glog/logging.h>
#include <directx/d3dx12.h>
#include <d3dcompiler.h>
#include <unordered_set>

#include <array>

#include "paths.hpp"

using namespace okami;

// Define input layout for static mesh vertices
std::array<D3D12_INPUT_ELEMENT_DESC, 4> kInputElementDescs = {
    D3D12_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, 
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    D3D12_INPUT_ELEMENT_DESC{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    D3D12_INPUT_ELEMENT_DESC{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    D3D12_INPUT_ELEMENT_DESC{"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32,
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
};

StaticMeshRenderer::StaticMeshRenderer() {
    m_vertexAttributes = GetVertexAttributes(kInputElementDescs);
}

struct StaticMeshLoadTask final : public GpuUploaderTask {
    std::optional<std::filesystem::path> m_path;
    std::optional<Geometry> m_geometry;
    resource_id_t m_resourceId;
    StaticMeshRenderer* m_renderer = nullptr;
    MeshPrivate m_privateData;

    // Temporary upload buffers
    // These will be released after the GPU upload is complete
    ComPtr<ID3D12Resource> m_vertexUploadBuffer;
    ComPtr<ID3D12Resource> m_indexUploadBuffer;

    StaticMeshLoadTask(
        std::filesystem::path path,
        resource_id_t resourceId,
        StaticMeshRenderer* renderer) : 
        m_path(std::move(path)), m_resourceId(resourceId), m_renderer(renderer) {}

    StaticMeshLoadTask(
        Geometry geometry,
        resource_id_t resourceId,
        StaticMeshRenderer* renderer) : 
        m_geometry(std::move(geometry)), m_resourceId(resourceId), m_renderer(renderer) {}

    Error Execute(ID3D12Device& device, ID3D12GraphicsCommandList& commandList) override {
        auto geometry = std::move(m_geometry);

        if (!geometry) {
            if (!m_path) {
                return Error("No path or geometry provided for StaticMeshLoadTask");
            }

            auto result = Geometry::LoadGLTF(*m_path, m_renderer->GetVertexFormat());
            if (!result.has_value()) {
                return result.error();
            }

            geometry = std::move(result.value());
        }

        if (!geometry->HasFormat(m_renderer->GetVertexFormat())) {
            geometry = geometry->AsFormat(m_renderer->GetVertexFormat());
            LOG(WARNING) << "Static mesh geometry format changed, converting to static mesh attributes";
        }

        // Calculate vertex buffer size - geometry is in correct format with single interleaved buffer
        size_t vertexCount = 0;
        size_t vertexStride = m_renderer->GetVertexFormat()[0].m_stride;
        
        auto rawVertexData = geometry->GetRawVertexData(0);
        if (rawVertexData.empty()) {
            return Error("Geometry has no vertex data");
        }
        
        vertexCount = rawVertexData.size() / vertexStride;
        if (vertexCount == 0) {
            return Error("Invalid vertex data size");
        }

        // Create vertex buffer
        auto vertexBufferResult = GpuBuffer::Create(device, rawVertexData.size());
        if (!vertexBufferResult.has_value()) {
            return vertexBufferResult.error();
        }
        m_privateData.m_vertexBuffer = std::move(vertexBufferResult.value());

        // Create upload buffer for vertex data
        ComPtr<ID3D12Resource> vertexUploadBuffer;

        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(rawVertexData.size());
        HRESULT hr = device.CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&vertexUploadBuffer));

        if (FAILED(hr)) {
            return Error("Failed to create vertex upload buffer");
        }

        // Map and copy vertex data
        void* vertexMappedData = nullptr;
        hr = vertexUploadBuffer->Map(0, nullptr, &vertexMappedData);
        if (FAILED(hr)) {
            return Error("Failed to map vertex upload buffer");
        }

        std::memcpy(vertexMappedData, rawVertexData.data(), rawVertexData.size());
        vertexUploadBuffer->Unmap(0, nullptr);

        // Copy from upload buffer to default buffer
        commandList.CopyResource(m_privateData.m_vertexBuffer.GetResource(), vertexUploadBuffer.Get());
        m_vertexUploadBuffer = vertexUploadBuffer; // Don't release until after GPU upload

        // Store vertex count
        m_privateData.m_vertexCount = static_cast<UINT>(vertexCount);

        // Handle index buffer if present
        if (geometry->HasIndexBuffer()) {
            auto indexData = geometry->GetIndexBuffer();
            size_t indexBufferSize = indexData.size() * sizeof(index_t);

            auto indexBufferResult = GpuBuffer::Create(device, indexBufferSize);
            if (!indexBufferResult.has_value()) {
                return indexBufferResult.error();
            }
            m_privateData.m_indexBuffer = std::move(indexBufferResult.value());

            // Create upload buffer for index data
            ComPtr<ID3D12Resource> indexUploadBuffer;
            heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
            hr = device.CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&indexUploadBuffer));

            if (FAILED(hr)) {
                return Error("Failed to create index upload buffer");
            }

            // Map and copy index data
            void* indexMappedData = nullptr;
            hr = indexUploadBuffer->Map(0, nullptr, &indexMappedData);
            if (FAILED(hr)) {
                return Error("Failed to map index upload buffer");
            }

            std::memcpy(indexMappedData, indexData.data(), indexBufferSize);
            indexUploadBuffer->Unmap(0, nullptr);

            // Copy from upload buffer to default buffer
            commandList.CopyResource(m_privateData.m_indexBuffer.GetResource(), indexUploadBuffer.Get());
            m_indexUploadBuffer = indexUploadBuffer; // Don't release until after GPU upload

            // Store index count
            m_privateData.m_indexCount = static_cast<UINT>(indexData.size());
        }

        return {};
    }
    
    Error Finalize() override {
        return m_renderer->Finalize(m_resourceId, std::move(m_privateData));
    }
};

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

    return rootSignature;
}

Error StaticMeshRenderer::Startup(
    ID3D12Device& device,
    std::shared_ptr<GpuUploader> uploader,
    DirectX::RenderTargetState rts,
    int bufferCount) {
    m_uploader = uploader;

    // Load vertex shader
    auto vertexShader = LoadShaderFromFile(GetShaderPath("static_mesh_vs.cso"));
    if (!vertexShader.has_value()) {
        return vertexShader.error();
    }

    // Load pixel shader
    auto pixelShader = LoadShaderFromFile(GetShaderPath("static_mesh_ps.cso"));
    if (!pixelShader.has_value()) {
        return pixelShader.error();
    }

    // Create root signature
    auto rootSignature = CreateRootSignature(device);
    if (!rootSignature.has_value()) {
        return rootSignature.error();
    }
    m_rootSignature = std::move(rootSignature.value());

    // Define the graphics pipeline state object description
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { 
        kInputElementDescs.data(), 
        static_cast<UINT>(kInputElementDescs.size()) 
    };
    psoDesc.pRootSignature = m_rootSignature.Get();
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
    HRESULT hr = device.CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
    if (FAILED(hr)) {
        return Error("Failed to create graphics pipeline state");
    }

    // Create per-frame data
    for (int i = 0; i < bufferCount; ++i) {
        auto shaderConstants = ConstantBuffer<hlsl::Globals>::Create(device);
        if (!shaderConstants.has_value()) {
            return shaderConstants.error();
        }
        
        auto instanceBuffer = StructuredBuffer<hlsl::Instance>::Create(device, 0);
        if (!instanceBuffer.has_value()) {
            return instanceBuffer.error();
        }

        m_perFrameData.emplace_back(PerFrameData{
            .m_globalConstants = std::move(shaderConstants.value()),
            .m_instanceBuffer = std::move(instanceBuffer.value())
        });
    }

    return {};
}

Error StaticMeshRenderer::Render(
    ID3D12Device& device,
    ID3D12GraphicsCommandList& commandList,
    hlsl::Globals const& globals,
    IStorageAccessor<Transform> const& transforms) {

    // Transition all meshes that need to be transitioned
    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    while (!m_meshesToTransition.empty()) {
        auto nextResourceId = m_meshesToTransition.front();
        m_meshesToTransition.pop();

        auto it = m_meshesById.find(nextResourceId);
        if (it == m_meshesById.end()) {
            LOG(WARNING) << "Mesh with ID " << nextResourceId << " not found for transition";
            continue;
        }
        auto& mesh = it->second;

        // Vertex buffer transition
        D3D12_RESOURCE_BARRIER vertexBarrier = {};
        vertexBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        vertexBarrier.Transition.pResource = mesh.m_private.m_vertexBuffer.GetResource();
        vertexBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        vertexBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        vertexBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        barriers.push_back(vertexBarrier);

        // Index buffer transition if it exists
        if (mesh.m_private.m_indexBuffer.GetResource()) {
            D3D12_RESOURCE_BARRIER indexBarrier = {};
            indexBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            indexBarrier.Transition.pResource = mesh.m_private.m_indexBuffer.GetResource();
            indexBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            indexBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            indexBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
            barriers.push_back(indexBarrier);
        }
    }

    if (!barriers.empty()) {
        commandList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }

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

    // Resize structured buffer if necessary to fit all mesh instances
    auto& instanceBuffer = m_perFrameData[m_currentBuffer].m_instanceBuffer;
    auto reserveResult = instanceBuffer.Reserve(device, staticMeshes.size());
    if (reserveResult.IsError()) {
        return Error("Failed to reserve structured buffer for static mesh instances");
    }

    std::vector<std::pair<MeshPrivate*, hlsl::Instance>> instanceData;

    // Fill instance data for all static mesh entities
    int index = 0;
    for (auto const& [entity, staticMeshComponent] : staticMeshes) {
        // Get transform for this entity
        auto transform = transforms.GetOr(entity, Transform::Identity());
        auto worldMatrix = transform.AsMatrix();

        // Set instance data
        auto meshIt = m_meshesById.find(staticMeshComponent.m_mesh.GetId());
        if (meshIt == m_meshesById.end() || 
            !meshIt->second.m_public->m_loaded.load()) {
            continue; // Mesh not loaded yet
        }

        instanceData.emplace_back(
            &meshIt->second.m_private,
            hlsl::Instance{
                .m_worldMatrix = worldMatrix,
                .m_worldInverseTransposeMatrix = glm::inverse(glm::transpose(worldMatrix))
            }
        );
    }

    // Sort by mesh pointer
    std::sort(instanceData.begin(), instanceData.end(),
        [](auto const& a, auto const& b) {
            return a.first < b.first; 
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
        for (auto const& [meshPtr, instance] : instanceData) {
            map->At(index) = hlsl::Instance{
                .m_worldMatrix = instance.m_worldMatrix,
                .m_worldInverseTransposeMatrix = instance.m_worldInverseTransposeMatrix
            };
        }
    }

    // Set the pipeline state
    commandList.SetPipelineState(m_pipelineState.Get());
    commandList.SetGraphicsRootSignature(m_rootSignature.Get());
    commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList.SetGraphicsRootConstantBufferView(0, globalConstantsBuffer.GetGPUAddress());
    commandList.SetGraphicsRootShaderResourceView(1, instanceBuffer.GetGPUAddress());

    UINT firstInstance = 0;
    for (auto beginIt = instanceData.begin(); beginIt != instanceData.end();) {
        auto endIt = std::find_if(beginIt, instanceData.end(), [&beginIt](const auto& pair) {
            return pair.first != beginIt->first;
        });

        auto meshData = beginIt->first;

        // Set vertex buffer
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
        vertexBufferView.BufferLocation = meshData->m_vertexBuffer.GetGPUAddress();
        vertexBufferView.SizeInBytes = static_cast<UINT>(m_vertexAttributes[0].m_stride * meshData->m_vertexCount);
        vertexBufferView.StrideInBytes = static_cast<UINT>(m_vertexAttributes[0].m_stride);
        commandList.IASetVertexBuffers(0, 1, &vertexBufferView);

        // Count instances of this mesh
        UINT instanceCount = static_cast<UINT>(endIt - beginIt);

        // Draw with or without index buffer
        if (meshData->m_indexBuffer.GetResource() && meshData->m_indexCount > 0) {
            D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
            indexBufferView.BufferLocation = meshData->m_indexBuffer.GetGPUAddress();
            indexBufferView.SizeInBytes = static_cast<UINT>(meshData->m_indexCount * sizeof(index_t));
            indexBufferView.Format = DXGI_FORMAT_R32_UINT;
            commandList.IASetIndexBuffer(&indexBufferView);

            commandList.DrawIndexedInstanced(meshData->m_indexCount, instanceCount, 0, 0, firstInstance);
        } else {
            commandList.DrawInstanced(meshData->m_vertexCount, instanceCount, 0, firstInstance);
        }

        firstInstance += instanceCount;
        beginIt = endIt;
    }

    // Advance to the next frame's data
    m_currentBuffer = (m_currentBuffer + 1) % m_perFrameData.size();

    return {};
}

void StaticMeshRenderer::Shutdown() {
    m_meshPathsToIds.clear();
    m_meshesById.clear();
    m_meshesToTransition = {};
    m_nextResourceId.store(0);

    // Clear static mesh storage
    m_staticMeshStorage.Clear();

    // Release resources
    m_rootSignature.Reset();
    m_pipelineState.Reset();
    m_perFrameData.clear();
    m_currentBuffer = 0;
    m_uploader.reset();
}

Error StaticMeshRenderer::Finalize(resource_id_t resourceId, MeshPrivate&& privateData) {
    auto it = m_meshesById.find(resourceId);
    if (it == m_meshesById.end()) {
        return Error("Mesh not found");
    }

    it->second.m_public->m_loaded.store(true);
    it->second.m_private = std::move(privateData);
    m_meshesToTransition.push(resourceId);
    return {};
}

std::pair<resource_id_t, ResHandle<Mesh>> StaticMeshRenderer::NewResource(std::optional<std::string_view> path) {
    auto newId = m_nextResourceId.fetch_add(1);
    auto mesh = MeshImpl{std::make_unique<Resource<Mesh>>(), {}};
    mesh.m_public->m_id = newId;
    mesh.m_public->m_path = std::string(path.value_or(""));
    auto handle = ResHandle<Mesh>(mesh.m_public.get());
    m_meshesById.emplace(newId, std::move(mesh));
    return { newId, handle };
}

ResHandle<Mesh> StaticMeshRenderer::Load(std::string_view path) {
    // Find existing mesh if already loaded
    if (auto it = m_meshPathsToIds.find(path); it != m_meshPathsToIds.end()) {
        auto resourceId = it->second;
        auto meshIt = m_meshesById.find(resourceId);
        if (meshIt != m_meshesById.end()) {
            return meshIt->second.m_public.get();
        }
    }

    auto [resourceId, handle] = NewResource(path);
    m_uploader->SubmitTask(
        std::make_unique<StaticMeshLoadTask>(
            std::filesystem::path(path),
            resourceId,
            this));
    m_meshPathsToIds.emplace(path, resourceId);
    return handle;
}

ResHandle<Mesh> StaticMeshRenderer::Create(typename Mesh::CreationData&& data) {
    auto [resourceId, handle] = NewResource();
    m_uploader->SubmitTask(
        std::make_unique<StaticMeshLoadTask>(
            std::move(data),
            resourceId,
            this));
    return handle;
}

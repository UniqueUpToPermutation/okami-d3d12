#include "d3d12_mesh.hpp"

#include <glog/logging.h>

#include <directx/d3dx12.h>

using namespace okami;

Error MeshLoadTask::Execute(ID3D12Device& device, ID3D12GraphicsCommandList& commandList) {
    auto initGeometry = std::move(m_initData);

    if (!initGeometry) {
        if (!m_path) {
            return Error("No path or geometry provided for StaticMeshLoadTask");
        }

        auto result = InitMesh::LoadGLTF(*m_path, &GetMeshFormat);
        if (!result.has_value()) {
            return result.error();
        }

        initGeometry = std::move(result.value());
    }

    auto& geometry = initGeometry->m_geometry;
    auto vertexFormat = GetMeshFormat(initGeometry->m_type);
    OKAMI_ASSERT(geometry.HasFormat(vertexFormat), "Geometry does not match requested vertex format");

    // Calculate vertex buffer size - geometry is in correct format with single interleaved buffer
    size_t vertexCount = 0;
    size_t vertexStride = vertexFormat[0].m_stride;

    auto rawVertexData = geometry.GetRawVertexData(0);
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
    if (geometry.HasIndexBuffer()) {
        auto indexData = geometry.GetIndexBuffer();
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

    m_publicData.m_format = vertexFormat;

    return {};
}

Error MeshLoadTask::Finalize() {
    return m_manager->Finalize(m_resourceId, m_publicData, m_privateData, GetError());
}

void MeshManager::TransitionMeshes(ID3D12GraphicsCommandList& commandList) {
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
}

std::pair<resource_id_t, ResHandle<Mesh>> MeshManager::NewResource(
    std::optional<std::string_view> path) {
    auto newId = m_nextResourceId.fetch_add(1);
    auto mesh = MeshImpl{std::make_unique<Resource<Mesh>>(), {}};
    mesh.m_public->m_id = newId;
    mesh.m_public->m_path = std::string(path.value_or(""));
    auto handle = ResHandle<Mesh>(mesh.m_public.get());
    m_meshesById.emplace(newId, std::move(mesh));
    return { newId, handle };
}

Error MeshManager::Finalize(resource_id_t resourceId,
    Mesh publicData, 
    MeshPrivate privateData,
    Error error) {
    auto it = m_meshesById.find(resourceId);
    if (it == m_meshesById.end()) {
        return Error("Mesh not found");
    }

    it->second.m_public->m_data = publicData;
    it->second.m_private = std::move(privateData);

    if (error.IsOk()) {
        m_meshesToTransition.push(resourceId);
    } else {
        // Use default texture?
    }

    it->second.m_public->m_loaded.store(true);
    return {};
}

ResHandle<Mesh> MeshManager::Load(std::string_view path) {
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
        std::make_unique<MeshLoadTask>(
            std::filesystem::path(path),
            resourceId,
            this));
    m_meshPathsToIds.emplace(path, resourceId);
    return handle;
}

ResHandle<Mesh> MeshManager::Create(typename Mesh::CreationData&& data) {
    auto [resourceId, handle] = NewResource();
    m_uploader->SubmitTask(
        std::make_unique<MeshLoadTask>(
            std::move(data),
            resourceId,
            this));
    return handle;
}
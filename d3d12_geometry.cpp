#include "d3d12_geometry.hpp"
#include "d3d12_mesh_formats.hpp"

#include <glog/logging.h>

#include <directx/d3dx12.h>

using namespace okami;

Error MeshLoadTask::Execute(ID3D12Device& device, ID3D12GraphicsCommandList& commandList) {
    auto initGeometry = std::move(m_initData);

    if (!initGeometry) {
        if (!m_path) {
            return Error("No path or geometry provided for StaticMeshLoadTask");
        }

        auto result = RawGeometry::LoadGLTF(*m_path);
        if (!result.has_value()) {
            return result.error();
        }

        initGeometry = std::move(result.value());
    }

    auto reqs = GetD3D12MeshRequirements();

    size_t indexBufferSize = 0;
    size_t vertexBufferSize = 0;

    // Generate the descs for the meshes that will actually be on the GPU
    std::vector<GeometryMeshDesc> convertedMeshes;
    for (auto const& mesh : initGeometry->GetMeshes()) {
        auto meshCopy = mesh;
        auto const& meshReqs = reqs[mesh.m_type];
        meshCopy.m_attributes.clear();
        for (auto attrib : meshReqs) {
            meshCopy.m_attributes.push_back(Attribute{
                .m_type = attrib,
                .m_buffer = 0,
                .m_offset = vertexBufferSize,
            });
            vertexBufferSize += GetStride(attrib) * 
                mesh.m_vertexCount;
        }
        if (mesh.HasIndexBuffer()) {
            meshCopy.m_indices->m_buffer = 1;
            meshCopy.m_indices->m_offset = indexBufferSize;
            indexBufferSize += GetStride(AccessorType::Scalar, meshCopy.m_indices->m_type) * 
                meshCopy.m_indices->m_count;
        }
        convertedMeshes.push_back(meshCopy);
    }

    GeometryPrivate privateData;

    auto vertexBuffer = StaticBuffer::Create(device, vertexBufferSize);
    OKAMI_ERROR_RETURN(vertexBuffer);
    privateData.m_vertexBuffer = std::move(vertexBuffer.value());

    if (indexBufferSize > 0) {
        auto indexBuffer = StaticBuffer::Create(device, indexBufferSize);
        OKAMI_ERROR_RETURN(indexBuffer);
        privateData.m_indexBuffer = std::move(indexBuffer.value());
    }

    // Create and populate vertex upload buffer
    auto vertexUploadBuffer = UploadBuffer<uint8_t>::Create(
        device,
        UploadBufferType::Vertex,
        L"Vertex Upload Buffer",
        vertexBufferSize
    );
    OKAMI_ERROR_RETURN(vertexUploadBuffer);

    {
        auto map = vertexUploadBuffer->Map();
        OKAMI_ERROR_RETURN(map);

        for (int i = 0; i < convertedMeshes.size(); i++) {
            const auto& mesh = convertedMeshes[i];
            const auto& originalMesh = initGeometry->GetMeshes()[i];

            for (auto attrib : originalMesh.m_attributes) {
                auto it = std::find_if(mesh.m_attributes.begin(), mesh.m_attributes.end(),
                    [attrib](const Attribute& a) { return a.m_type == attrib.m_type; });

                // Original mesh doesn't have this attribute
                if (it == mesh.m_attributes.end()) {
                    GenerateDefaultAttributeData(
                        std::span(map->Data() + attrib.m_offset,
                            GetStride(attrib.m_type) * mesh.m_vertexCount),
                        attrib.m_type);
                } else {
                    // Mesh has this attribute, copy
                    std::memcpy(
                        map->Data() + attrib.m_offset,
                        initGeometry->GetRawVertexData(it->m_buffer).data() + it->m_offset,
                        GetStride(attrib.m_type) * mesh.m_vertexCount
                    );
                }
            }
        }
    }
    // Write copy commands into command list
    commandList.CopyResource(privateData.m_vertexBuffer.GetResource(),
        vertexUploadBuffer->GetResource());
    m_uploadBuffers.push_back(vertexUploadBuffer->GetResource());

    // Create and populate an index upload buffer
    if (privateData.m_indexBuffer) {
        auto indexUploadBuffer = UploadBuffer<uint8_t>::Create(
            device,
            UploadBufferType::Index,
            L"Index Upload Buffer",
            indexBufferSize
        );
        OKAMI_ERROR_RETURN(indexUploadBuffer);

        {
            auto map = indexUploadBuffer->Map();
            OKAMI_ERROR_RETURN(map);

            for (int i = 0; i < convertedMeshes.size(); i++) {
                const auto& mesh = convertedMeshes[i];
                const auto& originalMesh = initGeometry->GetMeshes()[i];

                if (mesh.HasIndexBuffer()) {
                    std::memcpy(
                        map->Data() + mesh.m_indices->m_offset,
                        initGeometry->GetRawVertexData(originalMesh.m_indices->m_buffer).data() + originalMesh.m_indices->m_offset,
                        mesh.m_indices->GetStride() * mesh.m_indices->m_count
                    );
                }
            }
        }
        // Write copy commands into command list
        commandList.CopyResource(privateData.m_indexBuffer->GetResource(),
            indexUploadBuffer->GetResource());
        m_uploadBuffers.push_back(indexUploadBuffer->GetResource());
    }

    m_resource.m_meshes = std::move(convertedMeshes);
    m_resource.m_privateData = std::move(privateData);

    return {};
}

Error MeshLoadTask::Finalize() {
    return m_manager->Finalize(m_resourceId, std::move(m_resource), GetError());
}

void GeometryManager::TransitionMeshes(ID3D12GraphicsCommandList& commandList) {
    // Transition all meshes that need to be transitioned
    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    while (!m_meshesToTransition.empty()) {
        auto nextResourceId = m_meshesToTransition.front();
        m_meshesToTransition.pop();

        auto it = m_meshesById.find(nextResourceId);
        if (it == m_meshesById.end()) {
            LOG(WARNING) << "Geometry with ID " << nextResourceId << " not found for transition";
            continue;
        }
        auto& geometry = it->second;

        auto& privateData = std::any_cast<GeometryPrivate&>(geometry->m_data.m_privateData);

        // Vertex buffer transition
        D3D12_RESOURCE_BARRIER vertexBarrier = {};
        vertexBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        vertexBarrier.Transition.pResource = privateData.m_vertexBuffer.GetResource();
        vertexBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        vertexBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        vertexBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        barriers.push_back(vertexBarrier);

        // Index buffer transition
        if (privateData.m_indexBuffer) {
            D3D12_RESOURCE_BARRIER indexBarrier = {};
            indexBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            indexBarrier.Transition.pResource = privateData.m_indexBuffer->GetResource();
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

std::pair<resource_id_t, ResHandle<Geometry>> GeometryManager::NewResource(
    std::optional<std::string_view> path) {
    auto newId = m_nextResourceId.fetch_add(1);
    auto mesh = std::make_unique<Resource<Geometry>>();
    mesh->m_id = newId;
    mesh->m_path = std::string(path.value_or(""));
    auto handle = ResHandle<Geometry>(mesh.get());
    m_meshesById.emplace(newId, std::move(mesh));
    return { newId, handle };
}

Error GeometryManager::Finalize(resource_id_t resourceId,
    Geometry data,
    Error error) {
    auto it = m_meshesById.find(resourceId);
    if (it == m_meshesById.end()) {
        return Error("Geometry not found");
    }

    it->second->m_data = std::move(data);

    if (error.IsOk()) {
        m_meshesToTransition.push(resourceId);
    } else {
        // Use default texture?
    }

    it->second->m_loaded.store(true);
    return {};
}

ResHandle<Geometry> GeometryManager::Load(std::string_view path) {
     // Find existing mesh if already loaded
    if (auto it = m_meshPathsToIds.find(path); it != m_meshPathsToIds.end()) {
        auto resourceId = it->second;
        auto meshIt = m_meshesById.find(resourceId);
        if (meshIt != m_meshesById.end()) {
            return meshIt->second.get();
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

ResHandle<Geometry> GeometryManager::Create(typename Geometry::CreationData&& data) {
    auto [resourceId, handle] = NewResource();
    m_uploader->SubmitTask(
        std::make_unique<MeshLoadTask>(
            std::move(data),
            resourceId,
            this));
    return handle;
}
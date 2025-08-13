#pragma once

#include "renderer.hpp"
#include "d3d12_common.hpp"
#include "d3d12_upload.hpp"
#include "storage.hpp"
#include "geometry.hpp"

namespace okami {
    class MeshManager;

    struct MeshPrivate {
		GpuBuffer m_vertexBuffer;
		GpuBuffer m_indexBuffer;
		UINT m_vertexCount = 0;
		UINT m_indexCount = 0;
	};

	struct MeshImpl {
		std::unique_ptr<Resource<Mesh>> m_public;
		MeshPrivate m_private;
	};

    struct MeshLoadTask final : public GpuUploaderTask {
    private:
        std::optional<std::filesystem::path> m_path;
        std::optional<InitMesh> m_initData;
        resource_id_t m_resourceId;
        MeshManager* m_manager = nullptr;
        MeshPrivate m_privateData;
        Mesh m_publicData;

        // Temporary upload buffers
        // These will be released after the GPU upload is complete
        ComPtr<ID3D12Resource> m_vertexUploadBuffer;
        ComPtr<ID3D12Resource> m_indexUploadBuffer;

    public:
        inline MeshLoadTask(
            std::filesystem::path path,
            resource_id_t resourceId,
            MeshManager* manager) :
            m_path(std::move(path)), m_resourceId(resourceId), m_manager(manager) {}

        inline MeshLoadTask(
            InitMesh initData,
            resource_id_t resourceId,
            MeshManager* manager) :
            m_initData(std::move(initData)), m_resourceId(resourceId), m_manager(manager) {}

        Error Execute(ID3D12Device& device, ID3D12GraphicsCommandList& commandList) override;
        Error Finalize() override;
    };

    class MeshManager : public IResourceManager<Mesh> {
    private:
        std::unordered_map<std::filesystem::path, resource_id_t> m_meshPathsToIds;
		std::unordered_map<resource_id_t, MeshImpl> m_meshesById;

		std::queue<resource_id_t> m_meshesToTransition;
		std::atomic<resource_id_t> m_nextResourceId{0};

        std::shared_ptr<GpuUploader> m_uploader;
    
    public:
        inline MeshManager(std::shared_ptr<GpuUploader> uploader) : m_uploader(std::move(uploader)) {}

        OKAMI_NO_COPY(MeshManager);
        OKAMI_NO_MOVE(MeshManager);

        inline void Register(InterfaceCollection& queryable) {
			queryable.Register<IResourceManager<Mesh>>(this);
		}

        inline std::unordered_map<resource_id_t, MeshImpl> const& GetMeshes() const {
            return m_meshesById;
        }

        std::pair<resource_id_t, ResHandle<Mesh>> NewResource(
            std::optional<std::string_view> path = std::nullopt);

		Error Finalize(
            resource_id_t resourceId, 
            Mesh publicData, 
            MeshPrivate privateData,
            Error error);

		ResHandle<Mesh> Load(std::string_view path) override;
		ResHandle<Mesh> Create(typename Mesh::CreationData&& data) override;

        void TransitionMeshes(ID3D12GraphicsCommandList& commandList);
    };
}
#pragma once

#include "renderer.hpp"
#include "d3d12_common.hpp"
#include "d3d12_upload.hpp"
#include "storage.hpp"
#include "geometry.hpp"

namespace okami {
    class GeometryManager;

    struct GeometryPrivate {
        StaticBuffer m_vertexBuffer;
        std::optional<StaticBuffer> m_indexBuffer;
	};

    struct MeshLoadTask final : public GpuUploaderTask {
    private:
        std::optional<std::filesystem::path> m_path;
        std::optional<RawGeometry> m_initData;
        resource_id_t m_resourceId;
        GeometryManager* m_manager = nullptr;
        Geometry m_resource;

        // Temporary upload buffers
        // These will be released after the GPU upload is complete
        std::vector<ComPtr<ID3D12Resource>> m_uploadBuffers;

    public:
        inline MeshLoadTask(
            std::filesystem::path path,
            resource_id_t resourceId,
            GeometryManager* manager) :
            m_path(std::move(path)), m_resourceId(resourceId), m_manager(manager) {}

        inline MeshLoadTask(
            RawGeometry initData,
            resource_id_t resourceId,
            GeometryManager* manager) :
            m_initData(std::move(initData)), m_resourceId(resourceId), m_manager(manager) {}

        Error Execute(
            ID3D12Device& device, 
            ID3D12GraphicsCommandList& commandList) override;
        Error Finalize() override;
    };

    class GeometryManager : public IResourceManager<Geometry> {
    private:
        std::unordered_map<std::filesystem::path, resource_id_t> m_meshPathsToIds;
		std::unordered_map<resource_id_t, std::unique_ptr<Resource<Geometry>>> m_meshesById;

		std::queue<resource_id_t> m_meshesToTransition;
		std::atomic<resource_id_t> m_nextResourceId{0};

        std::shared_ptr<GpuUploader> m_uploader;
    
    public:
        inline GeometryManager(std::shared_ptr<GpuUploader> uploader) : m_uploader(std::move(uploader)) {}

        OKAMI_NO_COPY(GeometryManager);
        OKAMI_NO_MOVE(GeometryManager);

        inline void Register(InterfaceCollection& queryable) {
			queryable.Register<IResourceManager<Geometry>>(this);
		}

        inline std::unordered_map<resource_id_t, std::unique_ptr<Resource<Geometry>>> const& GetMeshes() const {
            return m_meshesById;
        }

        std::pair<resource_id_t, ResHandle<Geometry>> NewResource(
            std::optional<std::string_view> path = std::nullopt);

		Error Finalize(
            resource_id_t resourceId, 
            Geometry data,
            Error error);

		ResHandle<Geometry> Load(std::string_view path) override;
		ResHandle<Geometry> Create(typename Geometry::CreationData&& data) override;

        void TransitionMeshes(ID3D12GraphicsCommandList& commandList);
    };
}
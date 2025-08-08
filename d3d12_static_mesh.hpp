#pragma once

#include <unordered_map>
#include <filesystem>
#include <DirectXTK12/RenderTargetState.h>

#include "renderer.hpp"
#include "d3d12_common.hpp"
#include "d3d12_upload.hpp"
#include "storage.hpp"

namespace okami {
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

	class StaticMeshRenderer : public IResourceManager<Mesh> {
	private:
		std::unordered_map<std::filesystem::path, resource_id_t> m_meshPathsToIds;
		std::unordered_map<resource_id_t, MeshImpl> m_meshesById;

		std::queue<resource_id_t> m_meshesToTransition;
		std::atomic<resource_id_t> m_nextResourceId{0};

		Storage<StaticMeshComponent> m_staticMeshStorage;

		struct PerFrameData {
			ConstantBuffer<hlsl::Globals> m_globalConstants;
			StructuredBuffer<hlsl::Instance> m_instanceBuffer;
		};

		ComPtr<ID3D12RootSignature> m_rootSignature;
		ComPtr<ID3D12PipelineState> m_pipelineState;

		std::vector<PerFrameData> m_perFrameData;
		int m_currentBuffer = 0;

		std::shared_ptr<GpuUploader> m_uploader;
		std::vector<Attribute> m_vertexAttributes;

		static Expected<ComPtr<ID3D12RootSignature>> CreateRootSignature(ID3D12Device& device);

	public:
		StaticMeshRenderer();

		inline std::span<Attribute const> GetVertexFormat() const {
			return m_vertexAttributes;
		}

		inline void RegisterInterfaces(InterfaceCollection& queryable) {
			m_staticMeshStorage.RegisterInterfaces(queryable);
			queryable.Register<IResourceManager<Mesh>>(this);
		}

		inline void RegisterSignalHandlers(SignalHandlerCollection& signals) {
			m_staticMeshStorage.RegisterSignalHandlers(signals);
		}

		inline ModuleResult ProcessSignals() {
			return m_staticMeshStorage.ProcessSignals();
		}

		Error Startup(
			ID3D12Device& device,
			std::shared_ptr<GpuUploader> uploader,
			DirectX::RenderTargetState rts,
			int bufferCount);

		Error Render(
			ID3D12Device& device,
			ID3D12GraphicsCommandList& commandList,
			hlsl::Globals const& globals,
			IStorageAccessor<Transform> const& transforms);

		void Shutdown();

		std::pair<resource_id_t, ResHandle<Mesh>> NewResource(
			std::optional<std::string_view> path = std::nullopt);

		Error Finalize(resource_id_t resourceId, MeshPrivate&& privateData);

		ResHandle<Mesh> Load(std::string_view path) override;
		ResHandle<Mesh> Create(typename Mesh::CreationData&& data) override;
	};
}
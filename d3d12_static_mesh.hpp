#pragma once

#include <unordered_map>
#include <filesystem>
#include <DirectXTK12/RenderTargetState.h>

#include "renderer.hpp"
#include "d3d12_common.hpp"
#include "storage.hpp"

namespace okami {
	struct MeshImpl {
		GPUBuffer m_vertexBuffer;
		GPUBuffer m_indexBuffer;
	};

	class StaticMeshRenderer : public IResourceManager<Mesh> {
	private:
		std::unordered_map<std::filesystem::path, resource_id_t> m_meshes;
		std::unordered_map<resource_id_t, MeshImpl> m_meshesById;
		Storage<StaticMeshComponent> m_staticMeshStorage;

		struct PerFrameData {
			ConstantBuffer<hlsl::Globals> m_globalConstants;
			StructuredBuffer<hlsl::Instance> m_instanceBuffer;
		};

		ComPtr<ID3D12RootSignature> m_rootSignature;
		ComPtr<ID3D12PipelineState> m_pipelineState;

	public:
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
			DirectX::RenderTargetState rts,
			int bufferCount);

		Error Render(
			ID3D12Device& device,
			ID3D12GraphicsCommandList& commandList,
			hlsl::Globals const& globals,
			IStorageAccessor<Transform> const& transforms);

		ResHandle<Mesh const> Load(std::string_view path) override;
		ResHandle<Mesh const> Create(typename Mesh::CreationData&& data) override;
	};
}
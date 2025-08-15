#pragma once

#include <DirectXTK12/RenderTargetState.h>

#include "renderer.hpp"
#include "d3d12_common.hpp"
#include "d3d12_upload.hpp"
#include "storage.hpp"
#include "d3d12_geometry.hpp"

namespace okami {
	class StaticMeshRenderer {
	private:
		std::shared_ptr<GeometryManager> m_manager;

		Storage<StaticMeshComponent> m_staticMeshStorage;

		struct PerFrameData {
			UploadBuffer<hlsl::Globals> m_globalConstants;
			UploadBuffer<hlsl::Instance> m_instanceBuffer;
		};

		ComPtr<ID3D12RootSignature> m_rootSignature;
		ComPtr<ID3D12PipelineState> m_pipelineState;

		std::vector<PerFrameData> m_perFrameData;
		int m_currentBuffer = 0;

		std::shared_ptr<GpuUploader> m_uploader;

		static Expected<ComPtr<ID3D12RootSignature>> CreateRootSignature(ID3D12Device& device);

		StaticMeshRenderer() = default;

	public:
		OKAMI_NO_COPY(StaticMeshRenderer);
		OKAMI_NO_MOVE(StaticMeshRenderer);

		inline void Register(InterfaceCollection& queryable, SignalHandlerCollection& signals) {
			m_staticMeshStorage.RegisterInterfaces(queryable);
			m_staticMeshStorage.RegisterSignalHandlers(signals);
		}

		inline ModuleResult ProcessSignals() {
			return m_staticMeshStorage.ProcessSignals();
		}

		static Expected<std::shared_ptr<StaticMeshRenderer>> Create(
			ID3D12Device& device,
			std::shared_ptr<GeometryManager> manager,
			DirectX::RenderTargetState rts,
			int bufferCount);

		Error Render(
			ID3D12Device& device,
			ID3D12GraphicsCommandList& commandList,
			hlsl::Globals const& globals,
			IStorageAccessor<Transform> const& transforms);
	};
}
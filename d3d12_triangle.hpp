#pragma once

#include "shaders/common.fxh"

#include "engine.hpp"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <DirectXTK12/RenderTargetState.h>
#include <memory>
#include <expected>

#include "d3d12_common.hpp"
#include "storage.hpp"
#include "renderer.hpp"
#include "transform.hpp"

namespace okami {
	using Microsoft::WRL::ComPtr;

	class TriangleRenderer {
	private:
		struct PerFrameData {
			ShaderConstants<hlsl::Globals> m_globalConstants;
			ShaderConstants<hlsl::Instance> m_instanceConstants;
		};

		ComPtr<ID3D12RootSignature> m_rootSignature;
		ComPtr<ID3D12PipelineState> m_pipelineState;
		Storage<DummyTriangleComponent> m_dummyTriangleStorage;

		std::vector<PerFrameData> m_perFrameData;
		int m_currentBuffer = 0;
		
		static std::expected<ComPtr<ID3D12RootSignature>, Error> CreateRootSignature(ID3D12Device& device);

	public:

		inline void RegisterInterfaces(InterfaceCollection& queryable) {
			m_dummyTriangleStorage.RegisterInterfaces(queryable);
		}

		inline void RegisterSignalHandlers(SignalHandlerCollection& signals) {
			m_dummyTriangleStorage.RegisterSignalHandlers(signals);
		}

		inline void Register(
			InterfaceCollection& queryable,
			SignalHandlerCollection& signals) {
			m_dummyTriangleStorage.RegisterInterfaces(queryable);
		}

		inline ModuleResult ProcessSignals() {
			return m_dummyTriangleStorage.ProcessSignals();
		}

		Error Startup(
			ID3D12Device& device,
			DirectX::RenderTargetState rts,
			int bufferCount);

		Error Render(
			ID3D12Device& device,
			ID3D12GraphicsCommandList& commandList,
			CameraInfo const& camera,
			IStorageAccessor<Transform> const& transforms);
	};
}
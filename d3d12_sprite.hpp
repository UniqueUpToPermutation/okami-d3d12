#pragma once

#ifdef USE_D3D12

#include <DirectXTK12/RenderTargetState.h>

#include "shaders/sprite.fxh"
#include "shaders/common.fxh"
#include "d3d12_common.hpp"
#include "d3d12_texture.hpp"
#include "renderer.hpp"
#include "storage.hpp"

namespace okami {
    class SpriteRenderer {
    private:
        Storage<SpriteComponent> m_staticSpriteStorage;

		struct PerFrameData {
			UploadBuffer<hlsl::Globals> m_globalConstants;
			UploadBuffer<hlsl::SpriteInstance> m_instanceBuffer;
		};

		ComPtr<ID3D12RootSignature> m_rootSignature;
		ComPtr<ID3D12PipelineState> m_pipelineState;

        std::shared_ptr<TextureManager> m_textureManager;
        std::shared_ptr<DescriptorPool> m_samplerPool;

        DescriptorPool::Handle m_samplerHandle;

		std::vector<PerFrameData> m_perFrameData;
		int m_currentBuffer = 0;

		static Expected<ComPtr<ID3D12RootSignature>> CreateRootSignature(ID3D12Device& device);

		SpriteRenderer() = default;

	public:
		OKAMI_NO_COPY(SpriteRenderer);
		OKAMI_NO_MOVE(SpriteRenderer);

		inline void Register(InterfaceCollection& queryable, SignalHandlerCollection& signals) {
			m_staticSpriteStorage.RegisterInterfaces(queryable);
			m_staticSpriteStorage.RegisterSignalHandlers(signals);
		}

		inline ModuleResult ProcessSignals() {
			return m_staticSpriteStorage.ProcessSignals();
		}

		static Expected<std::shared_ptr<SpriteRenderer>> Create(
			ID3D12Device& device,
			std::shared_ptr<TextureManager> manager,
            std::shared_ptr<DescriptorPool> samplerPool,
			DirectX::RenderTargetState rts,
			int bufferCount);

		Error Render(
			ID3D12Device& device,
			ID3D12GraphicsCommandList& commandList,
			hlsl::Globals const& globals,
			IStorageAccessor<Transform> const& transforms);
    };
}

#endif
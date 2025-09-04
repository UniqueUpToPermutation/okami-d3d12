#pragma once

#ifdef USE_D3D12

#include <optional>
#include <expected>
#include <memory>

#include <GLFW/glfw3.h>
#include <directxtk12/RenderTargetState.h>

#include "../engine.hpp"

#include "d3d12_descriptor_pool.hpp"

namespace okami {
	class ImGuiImpl {
	private:
		std::shared_ptr<DescriptorPool> m_srvPool;

	public:
		ImGuiImpl() = default;
		OKAMI_NO_COPY(ImGuiImpl);
		OKAMI_NO_MOVE(ImGuiImpl);

		static std::expected<std::unique_ptr<ImGuiImpl>, Error> Create(
			ID3D12Device* device,
			ID3D12CommandQueue* commandQueue,
			std::shared_ptr<DescriptorPool> srvPool,
			GLFWwindow* window,
			int framesInFlight,
			DirectX::RenderTargetState rts);

		void OnFrameBegin();
		void Render(ID3D12GraphicsCommandList* cl);
		void Shutdown();

		~ImGuiImpl();
	};
}

#endif
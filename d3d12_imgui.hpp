#pragma once

#include <optional>
#include <expected>
#include <memory>

#include <GLFW/glfw3.h>
#include <directxtk12/RenderTargetState.h>

#include "engine.hpp"
#include "d3d12_descriptor_pool.hpp"

namespace okami {
	struct ImGuiImpl {
		std::optional<DescriptorPool> m_imguiHeap;

		static std::expected<std::unique_ptr<ImGuiImpl>, Error> Create(
			ID3D12Device* device,
			ID3D12CommandQueue* commandQueue,
			GLFWwindow* window,
			int framesInFlight,
			DirectX::RenderTargetState rts);

		void OnFrameBegin();
		void Render(ID3D12GraphicsCommandList* cl);
		void Shutdown();

		~ImGuiImpl();
	};
}
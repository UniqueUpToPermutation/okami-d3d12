#include <expected>
#include <algorithm>
#include <array>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <glog/logging.h>
#include <wrl/client.h>
#include <directxtk12/DescriptorHeap.h>
#include <directxtk12/GraphicsMemory.h>
#include <directxtk12/GeometricPrimitive.h>
#include <directxtk12/SpriteBatch.h>
#include <directxtk12/ResourceUploadBatch.h>
#include <directxtk12/DirectXHelpers.h>
#include <directxtk12/PrimitiveBatch.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_dx12.h>

#include "engine.hpp"
#include "config.hpp"
#include "d3d12_descriptor_pool.hpp"
#include "d3d12_primitive_renderer.hpp"
#include "d3d12_imgui.hpp"

constexpr std::string_view kBufferCountKey = "renderer.buffer_count";
constexpr std::string_view kBackbufferWidthKey = "renderer.backbuffer_width";
constexpr std::string_view kBackbufferHeightKey = "renderer.backbuffer_height";
constexpr std::string_view kSyncIntervalKey = "renderer.sync_interval";

constexpr int kDefaultBufferCount = 2; // Double buffering
constexpr const char* kDefaultWindowTitle = "Okami Renderer";
constexpr int kDefaultBackbufferWidth = 1280;
constexpr int kDefaultBackbufferHeight = 720;
constexpr int kDefaultSyncInterval = 1; // VSync enabled

// Depth buffer format
constexpr DXGI_FORMAT kBackbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT kDepthBufferFormat = DXGI_FORMAT_D32_FLOAT;

using namespace Microsoft::WRL;
using namespace DirectX;
using namespace DirectX::SimpleMath;

using namespace okami;

struct FrameContext
{
	ID3D12CommandAllocator* CommandAllocator;
	UINT64 FenceValue;
};

struct PerFrameData {
	ComPtr<ID3D12Resource> m_renderTarget;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	UINT64 m_fenceValue = 0;

	static std::expected<PerFrameData, Error> Create(
		ID3D12Device* device,
		int bufferIndex,
		IDXGISwapChain3* swapChain,
		DirectX::DescriptorHeap& rtvHeap) {
		PerFrameData frameData;

		// 1. Get the back buffer from the swap chain
		ComPtr<ID3D12Resource> renderTarget;
		HRESULT hr = swapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(renderTarget.GetAddressOf()));
		if (FAILED(hr) || !renderTarget) {
			return std::unexpected(Error("Failed to get swap chain buffer for render target"));
		}

		// 2. Create RTV for the back buffer
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap.GetCpuHandle(bufferIndex);
		device->CreateRenderTargetView(renderTarget.Get(), nullptr, rtvHandle);

		frameData.m_renderTarget = renderTarget;

		// 3. Create command allocator for this frame
		hr = device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(frameData.m_commandAllocator.GetAddressOf())
		);
		if (FAILED(hr) || !frameData.m_commandAllocator) {
			return std::unexpected(Error("Failed to create command allocator for frame"));
		}

		// 4. Create command list for this frame
		hr = device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			frameData.m_commandAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(frameData.m_commandList.GetAddressOf())
		);
		if (FAILED(hr) || !frameData.m_commandList) {
			return std::unexpected(Error("Failed to create command list for frame"));
		}
		frameData.m_commandList->Close();

		return frameData;
	}

	void WaitOnFence(ID3D12Fence* fence, HANDLE eventHandle) {
		if (fence->GetCompletedValue() < m_fenceValue) {
			fence->SetEventOnCompletion(m_fenceValue, eventHandle);
			WaitForSingleObject(eventHandle, INFINITE);
		}
	}
};

class RendererModule final :
	public IEngineModule,
	public IRenderer {
private:
	GLFWwindow* m_window = nullptr;
	ComPtr<ID3D12Device> m_d3d12Device;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12Fence> m_fence;
	std::vector<PerFrameData> m_perFrameData;

	HANDLE m_eventHandle = NULL;
	UINT64 m_currentFrame = 0;
	UINT m_syncInterval = kDefaultSyncInterval;

	std::optional<DirectX::GraphicsMemory> m_graphicsMemory;

	std::optional<DirectX::DescriptorHeap> m_rtvHeap;
	std::optional<DirectX::DescriptorHeap> m_dsvHeap;

	ComPtr<ID3D12Resource> m_depthStencilBuffer;
	std::optional<DirectX::SpriteBatch> m_spriteBatch;
	std::optional<DirectX::ResourceUploadBatch> m_resourceUploadBatch;
	std::optional<PrimitiveRenderer> m_primitiveRenderer;

	std::unique_ptr<ImGuiImpl> m_imguiImpl;

public:

	RendererModule() = default;

	RenderTargetState GetBackbufferRenderTargetState() const {
		DXGI_SWAP_CHAIN_DESC swapChainDesc;
		if (!m_swapChain) {
			throw std::runtime_error("Swap chain is not initialized");
		}
		m_swapChain->GetDesc(&swapChainDesc);
		return RenderTargetState(&swapChainDesc, m_depthStencilBuffer->GetDesc().Format);
	}

	void RegisterInterfaces(InterfaceCollection& queryable) override {
		queryable.Register<IRenderer>(this);
	}

	void RegisterSignalHandlers(ISignalBus& eventBus) override {
	}

	Error Startup(IInterfaceQueryable& queryable, ISignalBus& eventBus) override {
		// Store the event handle in the member variable
		m_eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!m_eventHandle) {
			return Error("Failed to create event handle for synchronization");
		}

		IConfigModule* config = queryable.Query<IConfigModule>();

		if (!glfwInit()) {
			return Error("Failed to initialize GLFW");
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		int backbufferWidth = config->GetInt(kBackbufferWidthKey).value_or(kDefaultBackbufferWidth);
		int backbufferHeight = config->GetInt(kBackbufferHeightKey).value_or(kDefaultBackbufferHeight);
		m_syncInterval = config->GetInt(kSyncIntervalKey).value_or(kDefaultSyncInterval);

		GLFWwindow* window = glfwCreateWindow(
			backbufferWidth, backbufferHeight, kDefaultWindowTitle, nullptr, nullptr);
		if (!window) {
			return Error("Failed to create GLFW window");
		}

		m_window = window;

		// Create D3D12 device using the HWND from the GLFW window
		HWND hwnd = glfwGetWin32Window(m_window);
		if (!hwnd) {
			return Error("Failed to get Win32 window handle from GLFW window");
		}

#if defined(_DEBUG)
		LOG(INFO) << "Enabling D3D12 debug layer";
		// Enable the D3D12 debug layer if in a debug build
		{
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())))) {
				debugController->EnableDebugLayer();
			}
		}
#endif

		ID3D12Device* d3d12Device = nullptr;
		HRESULT hr = D3D12CreateDevice(
			nullptr, // default adapter
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&d3d12Device)
		);
		if (FAILED(hr) || !d3d12Device) {
			return Error("Failed to create D3D12 device");
		}

		m_d3d12Device = ComPtr<ID3D12Device>(d3d12Device);

		// In Startup, after m_d3d12Device assignment:
		// 1. Create DXGI factory
		ComPtr<IDXGIFactory7> dxgiFactory;
		HRESULT hrFactory = CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
		if (FAILED(hrFactory)) {
			return Error("Failed to create DXGI factory");
		}

		// 2. Create command queue
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		ID3D12CommandQueue* commandQueueRaw = nullptr;
		HRESULT hrQueue = m_d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueueRaw));
		if (FAILED(hrQueue) || !commandQueueRaw) {
			return Error("Failed to create D3D12 command queue");
		}
		m_commandQueue = ComPtr<ID3D12CommandQueue>(commandQueueRaw);

		// 3. Describe swap chain
		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height);

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = config->GetInt(kBufferCountKey).value_or(kDefaultBufferCount);
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = kBackbufferFormat;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		// 4. Create swap chain for HWND
		ComPtr<IDXGISwapChain1> swapChain1;
		hr = dxgiFactory->CreateSwapChainForHwnd(
			m_commandQueue.Get(),
			hwnd,
			&swapChainDesc,
			nullptr,
			nullptr,
			swapChain1.GetAddressOf()
		);
		if (FAILED(hr) || !swapChain1) {
			glfwDestroyWindow(m_window);
			glfwTerminate();
			return Error("Failed to create swap chain");
		}

		// 5. Query IDXGISwapChain3
		IDXGISwapChain3* swapChain3 = nullptr;
		hr = swapChain1->QueryInterface(IID_PPV_ARGS(&swapChain3));
		if (FAILED(hr) || !swapChain3) {
			return Error("Failed to query IDXGISwapChain3");
		}
		m_swapChain = ComPtr<IDXGISwapChain3>(swapChain3);

		// 6. Create descriptor heap for RTVs
		try {
			m_rtvHeap = DirectX::DescriptorHeap(
				m_d3d12Device.Get(),
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
				D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
				swapChainDesc.BufferCount
			);
		}
		catch (std::exception ex) {
			return Error(ex.what());
		}

		// 7. Create descriptor heap for DSV (depth-stencil view)
		try {
			m_dsvHeap = DirectX::DescriptorHeap(
				m_d3d12Device.Get(),
				D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
				D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
				1 // Only need one depth buffer
			);
		}
		catch (std::exception ex) {
			return Error(ex.what());
		}

		// 8. Create depth buffer
		CD3DX12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			kDepthBufferFormat,
			width,
			height,
			1, // Array size
			1, // Mip levels
			1, // Sample count
			0, // Sample quality
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);

		CD3DX12_CLEAR_VALUE depthOptimizedClearValue(kDepthBufferFormat, 1.0f, 0);

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		hr = m_d3d12Device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&m_depthStencilBuffer)
		);
		if (FAILED(hr)) {
			return Error("Failed to create depth stencil buffer");
		}

		// 9. Create depth stencil view
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = kDepthBufferFormat;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCpuHandle(0);
		m_d3d12Device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, dsvHandle);

		// Create per-frame data
		for (uint32_t i = 0; i < swapChainDesc.BufferCount; ++i) {
			auto result = PerFrameData::Create(
				m_d3d12Device.Get(),
				i,
				m_swapChain.Get(),
				*m_rtvHeap
			);
			if (!result) {
				return result.error();
			}
			else {
				m_perFrameData.push_back(std::move(result.value()));
			}
		}

		// 10. Create fence for GPU synchronization
		hr = m_d3d12Device->CreateFence(
			0, // Initial value
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(m_fence.GetAddressOf())
		);
		if (FAILED(hr) || !m_fence) {
			return Error("Failed to create D3D12 fence");
		}

		// Initialize graphics memory before creating triangle
		m_graphicsMemory = DirectX::GraphicsMemory(m_d3d12Device.Get());
		m_resourceUploadBatch = DirectX::ResourceUploadBatch(m_d3d12Device.Get());

		m_primitiveRenderer = PrimitiveRenderer::Create(
			m_d3d12Device.Get(),
			GetBackbufferRenderTargetState());
		if (!m_primitiveRenderer) {
			return Error("Failed to create PrimitiveRenderer");
		}

		auto imgui = ImGuiImpl::Create(
			m_d3d12Device.Get(),
			m_commandQueue.Get(),
			m_window,
			static_cast<int>(m_perFrameData.size()),
			GetBackbufferRenderTargetState()
		);
		if (!imgui) {
			return Error("Failed to create ImGui implementation");
		}
		m_imguiImpl = std::move(imgui.value());

		return Error{}; // Success
	}

	void Shutdown(IInterfaceQueryable& queryable, ISignalBus& eventBus) override {
		for (auto& frameData : m_perFrameData) {
			frameData.WaitOnFence(m_fence.Get(), m_eventHandle);
			frameData.m_commandAllocator->Reset();
			frameData.m_commandList->Reset(frameData.m_commandAllocator.Get(), nullptr);
		}

		m_imguiImpl.reset();
		m_primitiveRenderer.reset();
		m_depthStencilBuffer.Reset();
		m_dsvHeap.reset();
		m_resourceUploadBatch.reset();
		m_graphicsMemory.reset();
		m_fence.Reset();
		m_perFrameData.clear();
		m_rtvHeap.reset();
		m_commandQueue.Reset();
		m_swapChain.Reset();

#if defined(_DEBUG)
		if (m_d3d12Device) {
			ComPtr<ID3D12DebugDevice> debugDevice;
			if (SUCCEEDED(m_d3d12Device->QueryInterface(IID_PPV_ARGS(&debugDevice)))) {
				debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
			}
		}
#endif
		m_d3d12Device.Reset();

		if (m_window) {
			glfwDestroyWindow(m_window);
			m_window = nullptr;
		}
		glfwTerminate();

		if (m_eventHandle) {
			CloseHandle(m_eventHandle);
			m_eventHandle = NULL;
		}
	}

	void OnFrameBegin(Time const& time, ISignalBus& signalBus) override {
		glfwPollEvents();

		if (glfwWindowShouldClose(m_window)) {
			signalBus.Publish(SignalExit{});
		}

		if (m_imguiImpl) {
			m_imguiImpl->OnFrameBegin();
		}

		bool open = true;
		ImGui::ShowDemoWindow(&open);
	}

	ModuleResult HandleSignals(Time const&, ISignalBus& signalBus) override {
		// No specific signals to handle in this module
		return {};
	}

	void Render() override {
		if (!m_swapChain || !m_commandQueue || !m_d3d12Device) {
			return;
		}

		// Get current back buffer index
		UINT backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
		auto& frameData = m_perFrameData[backBufferIndex];

		// Wait on the fence for the current frame
		frameData.WaitOnFence(m_fence.Get(), m_eventHandle);
		frameData.m_commandAllocator->Reset();
		frameData.m_commandList->Reset(frameData.m_commandAllocator.Get(), nullptr);

		int backbufferWidth, backbufferHeight;
		glfwGetFramebufferSize(m_window, &backbufferWidth, &backbufferHeight);

		// Set up viewport and scissor rectangle
		D3D12_VIEWPORT viewport = {};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(backbufferWidth);
		viewport.Height = static_cast<float>(backbufferHeight);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		D3D12_RECT scissorRect = {};
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = backbufferWidth;
		scissorRect.bottom = backbufferHeight;

		frameData.m_commandList->RSSetViewports(1, &viewport);
		frameData.m_commandList->RSSetScissorRects(1, &scissorRect);

		// Transition back buffer to render target
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = frameData.m_renderTarget.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		frameData.m_commandList->ResourceBarrier(1, &barrier);

		// Set render targets (color + depth)
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCpuHandle(backBufferIndex);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCpuHandle(0);
		frameData.m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

		// Clear the back buffer and depth buffer
		const float clearColor[4] = { 0.1f, 0.1f, 0.3f, 1.0f };
		frameData.m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		frameData.m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// Draw triangle and lines with depth testing
		m_primitiveRenderer->BeginTriangles(frameData.m_commandList.Get());

		// Triangle with varying Z values to test depth
		std::array<VertexPositionColor, 3> triangleVertices = {
			VertexPositionColor{ { 0.5f, 0.75f, 0.5f }, { 1.0f, 1.0f, 0.0f } },
			VertexPositionColor{ { 0.75f, 0.25f, 0.0f }, { 0.0f, 1.0f, 1.0f } },
			VertexPositionColor{ { 0.25f, 0.25f, 0.8f }, { 1.0f, 0.0f, 1.0f } }
		};
		m_primitiveRenderer->DrawTriangles(frameData.m_commandList.Get(), triangleVertices.data(), triangleVertices.size());
		m_primitiveRenderer->End();

		m_primitiveRenderer->BeginLines(frameData.m_commandList.Get());
		// Line strip with varying Z values
		std::array<VertexPositionColor, 6> lineVertices = {
			VertexPositionColor{ { -0.5f, 0.75f, 0.3f }, { 1.0f, 0.0f, 0.0f } },
			VertexPositionColor{ { -0.75f, 0.25f, 0.6f }, { 0.0f, 1.0f, 1.0f } },
			VertexPositionColor{ { -0.75f, 0.25f, 0.6f }, { 0.0f, 1.0f, 1.0f } },
			VertexPositionColor{ { -0.25f, 0.25f, 0.1f }, { 1.0f, 1.0f, 1.0f } },
			VertexPositionColor{ { -0.25f, 0.25f, 0.1f }, { 1.0f, 1.0f, 1.0f } },
			VertexPositionColor{ { -0.5f, 0.75f, 0.3f }, { 1.0f, 0.0f, 0.0f } },
		};
		m_primitiveRenderer->DrawLines(frameData.m_commandList.Get(), lineVertices.data(), lineVertices.size());
		m_primitiveRenderer->End();

		// Draw IMGUI if initialized
		if (m_imguiImpl) {
			m_imguiImpl->Render(frameData.m_commandList.Get());
		}

		// Transition back buffer to present
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		frameData.m_commandList->ResourceBarrier(1, &barrier);

		// Close and execute command list
		frameData.m_commandList->Close();
		ID3D12CommandList* cmdLists[] = { frameData.m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(1, cmdLists);

		// Submit graphics memory to GPU
		m_graphicsMemory->Commit(m_commandQueue.Get());

		// Signal the fence for this frame
		m_commandQueue->Signal(m_fence.Get(), ++m_currentFrame);
		frameData.m_fenceValue = m_currentFrame;

		// Present
		m_swapChain->Present(m_syncInterval, 0);
	}

	std::string_view GetName() const override {
		return "D3D12 Renderer";
	}
};

std::unique_ptr<IEngineModule> D3D12RendererModuleFactory::operator() () {
	return std::make_unique<RendererModule>();
}
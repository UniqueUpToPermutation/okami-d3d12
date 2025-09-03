#ifdef USE_D3D12

#include <expected>
#include <algorithm>
#include <array>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <glog/logging.h>
#include <wrl/client.h>
#include <directxtk12/DescriptorHeap.h>
#include <comdef.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_dx12.h>

#include "engine.hpp"
#include "config.hpp"
#include "d3d12_descriptor_pool.hpp"
#include "d3d12_imgui.hpp"
#include "d3d12_triangle.hpp"
#include "d3d12_static_mesh.hpp"
#include "d3d12_upload.hpp"
#include "d3d12_geometry.hpp"
#include "d3d12_texture.hpp"
#include "d3d12_sprite.hpp"

#include <glog/logging.h>

// Depth buffer format
constexpr DXGI_FORMAT kBackbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT kDepthBufferFormat = DXGI_FORMAT_D32_FLOAT;

using namespace Microsoft::WRL;
using namespace DirectX;
using namespace DirectX::SimpleMath;

using namespace okami;

struct RendererConfig {
	int bufferCount = 2;
	std::string windowTitle = "Okami Renderer";
	int backbufferWidth = 1280;
	int backbufferHeight = 720;
	int syncInterval = 1; // VSync enabled

	OKAMI_CONFIG(renderer) {
		OKAMI_CONFIG_FIELD(bufferCount);
		OKAMI_CONFIG_FIELD(windowTitle);
		OKAMI_CONFIG_FIELD(backbufferWidth);
		OKAMI_CONFIG_FIELD(backbufferHeight);
		OKAMI_CONFIG_FIELD(syncInterval);
	}
};

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
		int m_bufferIndex,
		IDXGISwapChain3* swapChain,
		DirectX::DescriptorHeap& rtvHeap) {
		PerFrameData frameData;

		// 1. Get the back buffer from the swap chain
		ComPtr<ID3D12Resource> renderTarget;
		HRESULT hr = swapChain->GetBuffer(m_bufferIndex, IID_PPV_ARGS(&renderTarget));
		if (FAILED(hr) || !renderTarget) {
			return std::unexpected(Error("Failed to get swap chain buffer for render target"));
		}

		// 2. Create RTV for the back buffer
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap.GetCpuHandle(m_bufferIndex);
		device->CreateRenderTargetView(renderTarget.Get(), nullptr, rtvHandle);

		frameData.m_renderTarget = renderTarget;

		// 3. Create command allocator for this frame
		hr = device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&frameData.m_commandAllocator)
		);
		if (FAILED(hr) || !frameData.m_commandAllocator) {
			return std::unexpected(Error("Failed to create command allocator for frame"));
		}
		frameData.m_commandAllocator->SetName(L"Okami D3D12 Command Allocator");

		// 4. Create command list for this frame
		hr = device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			frameData.m_commandAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(&frameData.m_commandList)
		);
		if (FAILED(hr) || !frameData.m_commandList) {
			return std::unexpected(Error("Failed to create command list for frame"));
		}
		frameData.m_commandList->SetName(L"Okami D3D12 Command List");
		frameData.m_commandList->Close();

		return frameData;
	}

	// Overload for headless mode - creates offscreen render target
	static std::expected<PerFrameData, Error> CreateOffscreen(
		ID3D12Device* device,
		int m_bufferIndex,
		int width,
		int height,
		DirectX::DescriptorHeap& rtvHeap) {
		PerFrameData frameData;

		// 1. Create offscreen render target texture
		CD3DX12_RESOURCE_DESC renderTargetDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			kBackbufferFormat, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		float color[4] = { 0.25f, 0.25f, 0.75f, 1.0f };
		CD3DX12_CLEAR_VALUE clearValue(kBackbufferFormat, color);
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

		HRESULT hr = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&renderTargetDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			&clearValue,
			IID_PPV_ARGS(&frameData.m_renderTarget)
		);
		if (FAILED(hr) || !frameData.m_renderTarget) {
			return std::unexpected(Error("Failed to create offscreen render target"));
		}

		// 2. Create RTV for the render target
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap.GetCpuHandle(m_bufferIndex);
		device->CreateRenderTargetView(frameData.m_renderTarget.Get(), nullptr, rtvHandle);

		// 3. Create command allocator for this frame
		hr = device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&frameData.m_commandAllocator)
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
			IID_PPV_ARGS(&frameData.m_commandList)
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
	ComPtr<ID3D12CommandQueue> m_copyCommandQueue;
	ComPtr<ID3D12Fence> m_fence;
	std::vector<PerFrameData> m_perFrameData;

	HANDLE m_eventHandle = NULL;
	UINT64 m_currentFrame = 0;
	RendererConfig m_config;

	std::optional<DirectX::DescriptorHeap> m_rtvHeap;
	std::optional<DirectX::DescriptorHeap> m_dsvHeap;

	std::shared_ptr<DescriptorPool> m_srvPool;
	std::shared_ptr<DescriptorPool> m_samplerPool;

	ComPtr<ID3D12Resource> m_depthStencilBuffer;

	std::unique_ptr<ImGuiImpl> m_imguiImpl;

	// Headless mode support
	bool m_headlessMode = false;
	ComPtr<ID3D12Resource> m_readbackBuffer;

	std::shared_ptr<GpuUploader> m_uploader;

	TriangleRenderer m_triangleRenderer;

	std::shared_ptr<GeometryManager> m_meshManager;
	std::shared_ptr<TextureManager> m_textureManager;

	std::shared_ptr<StaticMeshRenderer> m_staticMeshRenderer;
	std::shared_ptr<SpriteRenderer> m_spriteRenderer;

	IStorageAccessor<Transform>* m_transforms = nullptr;
	Storage<Camera> m_storage;
	entity_t m_activeCamera = kNullEntity;

public:
	RendererModule(bool headless = false) :
		m_headlessMode(headless) {
	}

	RenderTargetState GetBackbufferRenderTargetState() const {
		if (m_headlessMode) {
			// For headless mode, create render target state manually
			return RenderTargetState(kBackbufferFormat, m_depthStencilBuffer->GetDesc().Format);
		}
		else {
			DXGI_SWAP_CHAIN_DESC swapChainDesc;
			if (!m_swapChain) {
				throw std::runtime_error("Swap chain is not initialized");
			}
			m_swapChain->GetDesc(&swapChainDesc);
			return RenderTargetState(&swapChainDesc, m_depthStencilBuffer->GetDesc().Format);
		}
	}

	void Register(InterfaceCollection& queryable, SignalHandlerCollection& signals) override {
		queryable.Register<IRenderer>(this);

		RegisterConfig<RendererConfig>(queryable, LOG_WRAP(WARNING));

		m_storage.RegisterInterfaces(queryable);
		m_triangleRenderer.RegisterInterfaces(queryable);

		m_storage.RegisterSignalHandlers(signals);
		m_triangleRenderer.RegisterSignalHandlers(signals);
	}

	Error Startup(
		InterfaceCollection& queryable, 
		SignalHandlerCollection& handlers,
		ISignalBus& eventBus) override {
		m_transforms = queryable.QueryStorage<Transform>();
		if (m_transforms == nullptr) {
			return Error("Transform storage not found!");
		}
		
		// Get configuration
		m_config = ReadConfig<RendererConfig>(queryable, LOG_WRAP(WARNING));

		// Store the event handle in the member variable
		m_eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!m_eventHandle) {
			return Error("Failed to create event handle for synchronization");
		}

		// Initialize GLFW only if not in headless mode
		if (!m_headlessMode) {
			if (!glfwInit()) {
				return Error("Failed to initialize GLFW");
			}

			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

			GLFWwindow* window = glfwCreateWindow(
				m_config.backbufferWidth,
				m_config.backbufferHeight,
				m_config.windowTitle.c_str(),
				nullptr, nullptr);
			if (!window) {
				return Error("Failed to create GLFW window");
			}

			m_window = window;
		}

		// Create D3D12 device
#ifndef NDEBUG
		LOG(INFO) << "Enabling D3D12 debug layer";
		// Enable the D3D12 debug layer if in a debug build
		{
			ComPtr<ID3D12Debug> debugController;
			HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()));
			if (SUCCEEDED(hr)) {
				debugController->EnableDebugLayer();
			} else {
				LOG(ERROR) << _com_error(hr).ErrorMessage();
			}
		}
#endif

		ID3D12Device* d3d12Device = nullptr;
		HRESULT hr = D3D12CreateDevice(
			nullptr, // default adapter
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&m_d3d12Device)
		);
		if (FAILED(hr) || !m_d3d12Device) {
			LOG(ERROR) << _com_error(hr).ErrorMessage();
			return Error("Failed to create D3D12 device!");
		}
		m_d3d12Device->SetName(L"Okami D3D12 Device");

		// Create command queue
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		HRESULT hrQueue = m_d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
		if (FAILED(hrQueue) || !m_commandQueue) {
			LOG(ERROR) << _com_error(hr).ErrorMessage();
			return Error("Failed to create D3D12 command queue");
		}
		m_commandQueue->SetName(L"Okami D3D12 Command Queue");

		// Create copy command queue
		D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {};
		copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
		copyQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		HRESULT hrCopyQueue = m_d3d12Device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&m_copyCommandQueue));
		if (FAILED(hrCopyQueue) || !m_copyCommandQueue) {
			return Error("Failed to create D3D12 copy command queue");
		}
		m_copyCommandQueue->SetName(L"Okami D3D12 Copy Command Queue");

		if (!m_headlessMode) {
			// Create swap chain for windowed mode
			ComPtr<IDXGIFactory7> dxgiFactory;
			HRESULT hrFactory = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
			if (FAILED(hrFactory)) {
				LOG(ERROR) << _com_error(hrFactory).ErrorMessage();
				return Error("Failed to create DXGI factory");
			}

			HWND hwnd = glfwGetWin32Window(m_window);
			if (!hwnd) {
				return Error("Failed to get Win32 window handle from GLFW window");
			}

			int width, height;
			glfwGetFramebufferSize(m_window, &width, &height);

			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			swapChainDesc.BufferCount = m_config.bufferCount;
			swapChainDesc.Width = width;
			swapChainDesc.Height = height;
			swapChainDesc.Format = kBackbufferFormat;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapChainDesc.SampleDesc.Count = 1;

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

			hr = swapChain1->QueryInterface(IID_PPV_ARGS(&m_swapChain));
			if (FAILED(hr) || !m_swapChain) {
				return Error("Failed to query IDXGISwapChain3");
			}
		}

		// Create descriptor heap for RTVs
		try {
			m_rtvHeap = DirectX::DescriptorHeap(
				m_d3d12Device.Get(),
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
				D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
				m_config.bufferCount
			);
		}
		catch (std::exception ex) {
			return Error(ex.what());
		}

		// Create descriptor heap for DSV (depth-stencil view)
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

		// Create depth buffer
		CD3DX12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			kDepthBufferFormat,
			m_config.backbufferWidth,
			m_config.backbufferHeight,
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
		m_depthStencilBuffer->SetName(L"Okami D3D12 Depth Stencil Buffer");

		// Create depth stencil view
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = kDepthBufferFormat;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCpuHandle(0);
		m_d3d12Device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, dsvHandle);

		// Create per-frame data
		for (int i = 0; i < m_config.bufferCount; ++i) {
			std::expected<PerFrameData, Error> result;
			if (m_headlessMode) {
				result = PerFrameData::CreateOffscreen(
					m_d3d12Device.Get(),
					i,
					m_config.backbufferWidth,
					m_config.backbufferHeight,
					*m_rtvHeap
				);
			}
			else {
				result = PerFrameData::Create(
					m_d3d12Device.Get(),
					i,
					m_swapChain.Get(),
					*m_rtvHeap
				);
			}

			if (!result) {
				return result.error();
			}
			else {
				m_perFrameData.push_back(std::move(result.value()));
			}
		}

		// Create readback buffer for headless mode
		if (m_headlessMode) {
			const UINT64 readbackBufferSize = m_config.backbufferWidth * m_config.backbufferHeight * 4; // 4 bytes per pixel (RGBA)

			CD3DX12_RESOURCE_DESC readbackDesc = CD3DX12_RESOURCE_DESC::Buffer(readbackBufferSize);
			CD3DX12_HEAP_PROPERTIES readbackHeapProps(D3D12_HEAP_TYPE_READBACK);

			hr = m_d3d12Device->CreateCommittedResource(
				&readbackHeapProps,
				D3D12_HEAP_FLAG_NONE,
				&readbackDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&m_readbackBuffer)
			);
			if (FAILED(hr)) {
				return Error("Failed to create readback buffer");
			}
			m_readbackBuffer->SetName(L"Okami D3D12 Readback Buffer");
		}

		// Create fence for GPU synchronization
		hr = m_d3d12Device->CreateFence(
			0, // Initial value
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&m_fence)
		);
		if (FAILED(hr) || !m_fence) {
			return Error("Failed to create D3D12 fence");
		}
		m_fence->SetName(L"Okami D3D12 Fence");

		// Create descriptor pools for SRV
		auto srvPool = DescriptorPool::Create(
			m_d3d12Device.Get(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			64, // Arbitrary size, can be adjusted
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		);
		if (!srvPool) {
			return Error("Failed to create SRV descriptor pool");
		}
		m_srvPool = std::make_shared<DescriptorPool>(std::move(srvPool.value()));

		// Create descriptor pools for samplers
		auto samplerPool = DescriptorPool::Create(
			m_d3d12Device.Get(),
			D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
			16, // Arbitrary size, can be adjusted
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		);
		if (!samplerPool) {
			return Error("Failed to create sampler descriptor pool");
		}
		m_samplerPool = std::make_shared<DescriptorPool>(std::move(samplerPool.value()));

		// Only create ImGui in windowed mode
		if (!m_headlessMode) {
			auto imgui = ImGuiImpl::Create(
				m_d3d12Device.Get(),
				m_commandQueue.Get(),
				m_srvPool,
				m_window,
				static_cast<int>(m_perFrameData.size()),
				GetBackbufferRenderTargetState()
			);
			if (!imgui) {
				return Error("Failed to create ImGui implementation");
			}
			m_imguiImpl = std::move(imgui.value());
		}

		// Initialize GPU uploader
		auto uploader = GpuUploader::Create(*m_d3d12Device.Get());
		if (!uploader) {
			return Error("Failed to create GpuUploader");
		}
		m_uploader = std::make_shared<GpuUploader>(std::move(uploader.value()));

		// Initialize triangle renderer
		if (m_triangleRenderer.Startup(
			*m_d3d12Device.Get(),
			GetBackbufferRenderTargetState(),
			static_cast<int>(m_perFrameData.size())
		).IsError()) {
			return Error("Failed to initialize TriangleRenderer");
		}

		// Initialize the mesh manager
		m_meshManager = std::make_shared<GeometryManager>(m_uploader);
		m_meshManager->Register(queryable);

		// Initialize the texture manager
		auto manager = TextureManager::Create(*m_d3d12Device.Get(), m_uploader);
		if (!manager) {
			return Error("Failed to create TextureManager");
		}
		m_textureManager = manager.value();
		m_textureManager->Register(queryable);

		// Initialize static mesh renderer
		auto staticMeshRenderer = StaticMeshRenderer::Create(
			*m_d3d12Device.Get(),
			m_meshManager,
			GetBackbufferRenderTargetState(),
			static_cast<int>(m_perFrameData.size())
		);
		if (!staticMeshRenderer) {
			return Error("Failed to create StaticMeshRenderer");
		}
		m_staticMeshRenderer = std::move(staticMeshRenderer.value());
		m_staticMeshRenderer->Register(queryable, handlers);

		// Initialize sprite renderer
		auto spriteRenderer = SpriteRenderer::Create(
			*m_d3d12Device.Get(),
			m_textureManager,
			m_samplerPool,
			GetBackbufferRenderTargetState(),
			static_cast<int>(m_perFrameData.size())
		);
		if (!spriteRenderer) {
			return Error("Failed to create SpriteRenderer");
		}
		m_spriteRenderer = std::move(spriteRenderer.value());
		m_spriteRenderer->Register(queryable, handlers);

		return Error{}; // Success
	}

	void Shutdown(IInterfaceQueryable& queryable, ISignalBus& eventBus) override {
		// Signal the fence one last time
		m_commandQueue->Signal(m_fence.Get(), ++m_currentFrame);

		// Wait for GPU to be finished with resources
		if (m_fence->GetCompletedValue() < m_currentFrame) {
			m_fence->SetEventOnCompletion(m_currentFrame, m_eventHandle);
			WaitForSingleObject(m_eventHandle, INFINITE);
		}

		if (m_eventHandle) {
			CloseHandle(m_eventHandle);
			m_eventHandle = NULL;
		}

		m_uploader->Stop();
		m_uploader.reset();

		m_staticMeshRenderer.reset();
		m_triangleRenderer.Shutdown();

		m_imguiImpl.reset();
		m_readbackBuffer.Reset();
		m_depthStencilBuffer.Reset();

		m_textureManager.reset();
		m_meshManager.reset();
		m_staticMeshRenderer.reset();
		m_spriteRenderer.reset();
		
		m_fence.Reset();
		m_perFrameData.clear();

		m_srvPool.reset();
		m_samplerPool.reset();
		m_dsvHeap.reset();
		m_rtvHeap.reset();

		m_copyCommandQueue.Reset();
		m_commandQueue.Reset();

		m_swapChain.Reset();

#ifndef NDEBUG
		ComPtr<ID3D12DebugDevice> debugDevice;
		if (m_d3d12Device) {
			HRESULT hr = m_d3d12Device->QueryInterface(IID_PPV_ARGS(&debugDevice));
			if (FAILED(hr)) {
				LOG(ERROR) << "Failed to get ID3D12DebugDevice interface";
			}
		}
#endif

		m_d3d12Device.Reset();

#ifndef NDEBUG
		if (debugDevice) {
			debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
			debugDevice.Reset();
		}
#endif

		if (!m_headlessMode) {
			if (m_window) {
				glfwDestroyWindow(m_window);
				m_window = nullptr;
			}
			glfwTerminate();
		}
	}

	void UploadResources() override {
		// Process any pending uploads
		while (true) {
			auto commandList = m_uploader->GetExecutableCommandListIfAny();
			if (!commandList) break;

			ID3D12CommandList* lists[] = { commandList->m_commandList.Get() };
			m_copyCommandQueue->ExecuteCommandLists(1, lists);
			m_copyCommandQueue->Signal(
				commandList->m_fenceToSignal.Get(),
				commandList->m_fenceValue);
		}
		m_uploader->FetchAndFinalizeTasks();
	}

	void OnFrameBegin(Time const& time, ISignalBus& signalBus, EntityTree& world) override {
		if (!m_headlessMode) {
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
	}

	ModuleResult HandleSignals(Time const&, ISignalBus& signalBus) override {
		ModuleResult result;
		result.Union(m_storage.ProcessSignals());
		result.Union(m_triangleRenderer.ProcessSignals());
		result.Union(m_staticMeshRenderer->ProcessSignals());
		result.Union(m_spriteRenderer->ProcessSignals());
		return result;
	}

	std::pair<std::optional<Camera>, Transform> GetActiveCameraAndTransform() const {
		auto const& storage = m_storage.GetStorage<Camera>();
		
		if (m_activeCamera == kNullEntity) {
			LOG_FIRST_N(WARNING, 1) << "Active camera entity not set! Using first camera!";

			if (storage.begin() == storage.end()) {
				LOG_FIRST_N(WARNING, 1) << "No cameras found in storage!";
				return { {}, {} };
			}
			else {
				return { storage.begin()->second, m_transforms->GetOr(storage.begin()->first, Transform::Identity()) };
			}
		}
		auto cameraIt = storage.find(m_activeCamera);
		if (cameraIt == storage.end()) {
			LOG_FIRST_N(WARNING, 1) << "Active camera entity not found: " << m_activeCamera;
			return { {}, {} };
		}
		else {
			return { cameraIt->second, m_transforms->GetOr(m_activeCamera, Transform::Identity()) };
		}
	}

	Error Render() override {
		if (!m_commandQueue || !m_d3d12Device) {
			return {};
		}

		// Get current back buffer index
		UINT backBufferIndex;
		if (m_headlessMode) {
			backBufferIndex = 0; // Use first buffer for headless mode
		}
		else {
			if (!m_swapChain) return {};
			backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
		}

		auto& frameData = m_perFrameData[backBufferIndex];

		// Wait on the fence for the current frame
		frameData.WaitOnFence(m_fence.Get(), m_eventHandle);
		frameData.m_commandAllocator->Reset();
		frameData.m_commandList->Reset(frameData.m_commandAllocator.Get(), nullptr);

		// Perform necessary resource transitions
		m_meshManager->TransitionMeshes(*frameData.m_commandList.Get());
		m_textureManager->TransitionTextures(*m_d3d12Device.Get(), *frameData.m_commandList.Get());

		// Set up viewport and scissor rectangle
		D3D12_VIEWPORT viewport = {};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(m_config.backbufferWidth);
		viewport.Height = static_cast<float>(m_config.backbufferHeight);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		D3D12_RECT scissorRect = {};
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = m_config.backbufferWidth;
		scissorRect.bottom = m_config.backbufferHeight;

		frameData.m_commandList->RSSetViewports(1, &viewport);
		frameData.m_commandList->RSSetScissorRects(1, &scissorRect);

		// Transition render target to render target state
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = frameData.m_renderTarget.Get();
		if (m_headlessMode) {
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}
		else {
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		if (!m_headlessMode) {
			// Only transition if not already in render target state
			frameData.m_commandList->ResourceBarrier(1, &barrier);
		}

		// Set render targets (color + depth)
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCpuHandle(backBufferIndex);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCpuHandle(0);
		frameData.m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

		// Clear the back buffer and depth buffer
		const float clearColor[4] = { 0.1f, 0.1f, 0.3f, 1.0f };
		frameData.m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		frameData.m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// Get the active camera
		auto [camera, transform] = GetActiveCameraAndTransform();
		hlsl::Globals globals{
			.m_camera = ToHLSLCamera(camera, transform, m_config.backbufferWidth, m_config.backbufferHeight)
		};

		m_triangleRenderer.Render(
			*m_d3d12Device.Get(),
			*frameData.m_commandList.Get(),
			globals,
			*m_transforms
		);

		m_staticMeshRenderer->Render(
			*m_d3d12Device.Get(),
			*frameData.m_commandList.Get(),
			globals,
			*m_transforms
		);

		m_spriteRenderer->Render(
			*m_d3d12Device.Get(),
			*frameData.m_commandList.Get(),
			globals,
			*m_transforms
		);

		// Draw IMGUI if initialized 
		if (m_imguiImpl && !m_headlessMode) {
			m_imguiImpl->Render(frameData.m_commandList.Get());
		}

		// Transition render target for present or copy
		if (m_headlessMode) {
			// Transition to copy source for readback
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
			frameData.m_commandList->ResourceBarrier(1, &barrier);
		}
		else {
			// Transition back buffer to present
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			frameData.m_commandList->ResourceBarrier(1, &barrier);
		}

		// Close and execute command list
		frameData.m_commandList->Close();
		ID3D12CommandList* cmdLists[] = { frameData.m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(1, cmdLists);

		// Signal the fence for this frame
		m_commandQueue->Signal(m_fence.Get(), ++m_currentFrame);
		frameData.m_fenceValue = m_currentFrame;

		// Present or save to file
		if (!m_headlessMode) {
			// Present
			m_swapChain->Present(m_config.syncInterval, 0);
		}

		return {};
	}

	Error SaveToFile(const std::string& filename) override {
		if (!m_headlessMode || !m_readbackBuffer) {
			LOG(ERROR) << "SaveToFile can only be called in headless mode";
			return Error("SaveToFile can only be called in headless mode");
		}

		// Wait for GPU to finish rendering
		UINT backBufferIndex = 0;
		auto& frameData = m_perFrameData[backBufferIndex];
		frameData.WaitOnFence(m_fence.Get(), m_eventHandle);

		// Create a command list for the copy operation
		ComPtr<ID3D12CommandAllocator> copyAllocator;
		HRESULT hr = m_d3d12Device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(copyAllocator.GetAddressOf())
		);
		if (FAILED(hr)) {
			LOG(ERROR) << "Failed to create copy command allocator";
			return Error("Failed to create copy command allocator");
		}

		ComPtr<ID3D12GraphicsCommandList> copyCommandList;
		hr = m_d3d12Device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			copyAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(copyCommandList.GetAddressOf())
		);
		if (FAILED(hr)) {
			LOG(ERROR) << "Failed to create copy command list";
			return Error("Failed to create copy command list");
		}

		// Copy render target to readback buffer
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
		UINT numRows;
		UINT64 rowSizeInBytes;
		UINT64 totalBytes;

		D3D12_RESOURCE_DESC renderTargetDesc = frameData.m_renderTarget->GetDesc();
		m_d3d12Device->GetCopyableFootprints(&renderTargetDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

		CD3DX12_TEXTURE_COPY_LOCATION src(frameData.m_renderTarget.Get(), 0);
		CD3DX12_TEXTURE_COPY_LOCATION dst(m_readbackBuffer.Get(), footprint);

		copyCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

		copyCommandList->Close();

		// Execute copy command
		ID3D12CommandList* cmdLists[] = { copyCommandList.Get() };
		m_commandQueue->ExecuteCommandLists(1, cmdLists);

		// Signal fence and wait for completion
		UINT64 copyFenceValue = ++m_currentFrame;
		m_commandQueue->Signal(m_fence.Get(), copyFenceValue);

		if (m_fence->GetCompletedValue() < copyFenceValue) {
			m_fence->SetEventOnCompletion(copyFenceValue, m_eventHandle);
			WaitForSingleObject(m_eventHandle, INFINITE);
		}

		// Map readback buffer and read the data
		void* mappedData = nullptr;
		hr = m_readbackBuffer->Map(0, nullptr, &mappedData);
		OKAMI_DEFER(m_readbackBuffer->Unmap(0, nullptr));

		if (FAILED(hr)) {
			LOG(ERROR) << "Failed to map readback buffer";
			return Error("Failed to map readback buffer")	;
		}

		TextureInfo info{
			.type = TextureType::TEXTURE_2D,
			.format = TextureFormat::RGBA8,
			.width = static_cast<uint32_t>(m_config.backbufferWidth),
			.height = static_cast<uint32_t>(m_config.backbufferHeight),
			.depth = 1,
			.arraySize = 1,
			.mipLevels = 1
		};

		RawTexture texture(info);
		std::memcpy(texture.GetData().data(), mappedData, texture.GetData().size());
		return texture.SavePNG(filename);
	}

	void SetHeadlessMode(bool headless) override {
		if (m_perFrameData.size() > 0) {
			LOG(WARNING) << "Cannot change headless mode after initialization";
			return;
		}
		m_headlessMode = headless;
	}

	std::string_view GetName() const override {
		return "D3D12 Renderer";
	}

	void SetActiveCamera(entity_t e) override {
		m_activeCamera = e;
		if (m_storage.GetStorage<Camera>().find(e) == m_storage.GetStorage<Camera>().end()) {
			LOG(WARNING) << "Entity " << e << " is not a valid camera";
		}
	}

	entity_t GetActiveCamera() const override {
		return m_activeCamera;
	}
};

std::unique_ptr<IEngineModule> D3D12RendererModuleFactory::operator() () {
	return std::make_unique<RendererModule>();
}

#endif
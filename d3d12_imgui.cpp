#include "d3d12_imgui.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_dx12.h>

using namespace okami;

std::expected<std::unique_ptr<ImGuiImpl>, Error> ImGuiImpl::Create(
	ID3D12Device* device,
	ID3D12CommandQueue* commandQueue,
	GLFWwindow* window,
	int framesInFlight,
	DirectX::RenderTargetState rts) {

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	float scalex, scaley;
	glfwGetWindowContentScale(window, &scalex, &scaley);
	float scale = std::min<float>(scalex, scaley);

	// Setup scaling
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	io.FontGlobalScale = scale;

	// Setup Platform/Renderer backends
	if (!ImGui_ImplGlfw_InitForOther(window, true)) {
		return std::unexpected(Error("Failed to initialize ImGui GLFW backend"));
	}

	std::unique_ptr<ImGuiImpl> imgui = std::make_unique<ImGuiImpl>();
	auto pool = DescriptorPool::Create(
		device,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		256, // Arbitrary size for SRV descriptors
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);
	if (!pool) {
		return std::unexpected(pool.error());
	}
	imgui->m_imguiHeap = std::move(*pool);

	// Initialize ImGui DX12 backend
	ImGui_ImplDX12_InitInfo info = {};
	info.Device = device;
	info.CommandQueue = commandQueue;
	info.SrvDescriptorHeap = imgui->m_imguiHeap->m_heap.Get();
	info.UserData = imgui.get();
	info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle) {
			auto impl = reinterpret_cast<ImGuiImpl*>(info->UserData);
			auto newIdx = impl->m_imguiHeap->Alloc();
			*out_cpu_desc_handle = impl->m_imguiHeap->GetCpuHandle(newIdx);
			*out_gpu_desc_handle = impl->m_imguiHeap->GetGpuHandle(newIdx);
		};
	info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle) {
			auto impl = reinterpret_cast<ImGuiImpl*>(info->UserData);
			impl->m_imguiHeap->Free(cpu_desc_handle, gpu_desc_handle);
		};
	info.NumFramesInFlight = framesInFlight;
	info.RTVFormat = rts.rtvFormats[0];
	info.DSVFormat = rts.dsvFormat;

	ImGui_ImplDX12_Init(&info);

	return imgui;
}

void ImGuiImpl::OnFrameBegin() {
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGuiImpl::Render(ID3D12GraphicsCommandList* cl) {
	ImGui::Render();
	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData && drawData->Valid) {
		cl->SetDescriptorHeaps(1, m_imguiHeap->m_heap.GetAddressOf());
		ImGui_ImplDX12_RenderDrawData(drawData, cl); // Command list will be set later
	}
}

void ImGuiImpl::Shutdown() {
	if (m_imguiHeap) {
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		m_imguiHeap.reset();
	}
}

ImGuiImpl::~ImGuiImpl() {
	Shutdown();
}
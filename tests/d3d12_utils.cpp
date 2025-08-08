#include "d3d12_utils.hpp"

#include <glog/logging.h>

using namespace okami;

void D3D12Test::SetUp() {
    // Create D3D12 device
#ifndef NDEBUG
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
        throw std::runtime_error("Failed to create D3D12 device");
    }

    m_device = ComPtr<ID3D12Device>(d3d12Device);

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ID3D12CommandQueue* commandQueueRaw = nullptr;
    HRESULT hrQueue = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueueRaw));
    if (FAILED(hrQueue) || !commandQueueRaw) {
        throw std::runtime_error("Failed to create D3D12 command queue");
    }
    m_commandQueue = ComPtr<ID3D12CommandQueue>(commandQueueRaw);

    // Create command queue
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    hrQueue = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueueRaw));
    if (FAILED(hrQueue) || !commandQueueRaw) {
        throw std::runtime_error("Failed to create D3D12 command queue");
    }
    m_copyQueue = ComPtr<ID3D12CommandQueue>(commandQueueRaw);

    // Create command allocator
    ID3D12CommandAllocator* commandAllocatorRaw = nullptr;
    hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocatorRaw));
    if (FAILED(hr) || !commandAllocatorRaw) {
        throw std::runtime_error("Failed to create D3D12 command allocator");
    }
    m_commandAllocator = ComPtr<ID3D12CommandAllocator>(commandAllocatorRaw);

    // Create command list
    ID3D12GraphicsCommandList* commandListRaw = nullptr;
    hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, 
                                      m_commandAllocator.Get(), nullptr, 
                                      IID_PPV_ARGS(&commandListRaw));
    if (FAILED(hr) || !commandListRaw) {
        throw std::runtime_error("Failed to create D3D12 command list");
    }
    m_commandList = ComPtr<ID3D12GraphicsCommandList>(commandListRaw);
    m_commandList->Close(); // Close the command list to prepare it for use

    // Create fence
	ID3D12Fence* fenceRaw = nullptr;
    hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fenceRaw));
    if (FAILED(hr) || !fenceRaw) {
        throw std::runtime_error("Failed to create D3D12 fence");
    }
    m_fence = ComPtr<ID3D12Fence>(fenceRaw);
    m_fenceValue = 0;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == 0) {
        throw std::runtime_error("Failed to create fence event");
	}
}

void D3D12Test::TearDown() {
    if (m_fence->GetCompletedValue() < m_fenceValue) {
        m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, 10000);

        if (m_fence->GetCompletedValue() < m_fenceValue) {
            throw std::runtime_error("Wait for GPU fence timed out!");
		}
    }

    // Release Direct3D 12 resources
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = 0;
	}
    m_fence.Reset();
    m_copyQueue.Reset();
    m_commandList.Reset();
    m_commandAllocator.Reset();
    m_commandQueue.Reset();
    m_device.Reset();
}
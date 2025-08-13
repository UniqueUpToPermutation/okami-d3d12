#include "d3d12_utils.hpp"

#include <glog/logging.h>

using namespace okami;

void D3D12Test::SetUp() {
    // Create D3D12 device
#ifndef NDEBUG
    // Enable the D3D12 debug layer if in a debug build
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
        }
    }
#endif

    HRESULT hr = D3D12CreateDevice(
        nullptr, // default adapter
        D3D_FEATURE_LEVEL_12_0,
        IID_PPV_ARGS(&m_device)
    );
    if (FAILED(hr) || !m_device) {
        throw std::runtime_error("Failed to create D3D12 device");
    }
    m_device->SetName(L"TestD3D12Device");

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    HRESULT hrQueue = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
    if (FAILED(hrQueue) || !m_commandQueue) {
        throw std::runtime_error("Failed to create D3D12 command queue");
    }
    m_commandQueue->SetName(L"TestD3D12CommandQueue");

    // Create command queue
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    hrQueue = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_copyQueue));
    if (FAILED(hrQueue) || !m_copyQueue) {
        throw std::runtime_error("Failed to create D3D12 command queue");
    }
    m_copyQueue->SetName(L"TestD3D12CopyQueue");

    // Create command allocator
    hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
    if (FAILED(hr) || !m_commandAllocator) {
        throw std::runtime_error("Failed to create D3D12 command allocator");
    }
    m_commandAllocator->SetName(L"TestD3D12CommandAllocator");

    // Create command list
    hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, 
                                      m_commandAllocator.Get(), nullptr, 
                                      IID_PPV_ARGS(&m_commandList));
    if (FAILED(hr) || !m_commandList) {
        throw std::runtime_error("Failed to create D3D12 command list");
    }
    m_commandList->Close(); // Close the command list to prepare it for use
    m_commandList->SetName(L"TestD3D12CommandList");

    // Create fence
    hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr) || !m_fence) {
        throw std::runtime_error("Failed to create D3D12 fence");
    }
    m_fence->SetName(L"TestD3D12Fence");
    m_fenceValue = 0;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == 0) {
        throw std::runtime_error("Failed to create fence event");
	}
}

void D3D12Test::TearDown() {
    m_commandQueue->Signal(m_fence.Get(), ++m_fenceValue);

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

#ifndef NDEBUG
		ComPtr<ID3D12DebugDevice> debugDevice;
		if (m_device) {
			HRESULT hr = m_device->QueryInterface(IID_PPV_ARGS(&debugDevice));
			if (FAILED(hr)) {
				LOG(ERROR) << "Failed to get ID3D12DebugDevice interface";
			}
		}
#endif

		m_device.Reset();

#ifndef NDEBUG
		if (debugDevice) {
			debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
			debugDevice.Reset();
		}
#endif
}
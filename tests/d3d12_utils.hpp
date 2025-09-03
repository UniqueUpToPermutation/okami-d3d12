#pragma once
 
#if defined(USE_D3D12)

#include <gtest/gtest.h>

#include <directx/d3dx12.h>
#include <wrl/client.h>

namespace okami {
    using Microsoft::WRL::ComPtr;
}

class D3D12Test : public ::testing::Test {
protected:
    okami::ComPtr<ID3D12Device> m_device;
    okami::ComPtr<ID3D12CommandQueue> m_commandQueue;
    okami::ComPtr<ID3D12CommandQueue> m_copyQueue;
    okami::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    okami::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    okami::ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent = 0;
	UINT64 m_fenceValue = 0;

    void SetUp() override;
    void TearDown() override;
};

#endif // USE_D3D12
#include "d3d12_upload2.hpp"

using namespace okami::_2;
using namespace okami;

GpuUploaderCommandListLock::~GpuUploaderCommandListLock() {
    if (m_resources) {
        m_resources->ReleaseLock(*this);
    }
}

Expected<std::unique_ptr<GpuUploaderResources>> GpuUploaderResources::Create(
    ID3D12Device& device,
    std::shared_ptr<IUploaderThread> uploader) {
    auto res = std::make_unique<GpuUploaderResources>();

    // Create fence for synchronization
    HRESULT hr = device.CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&res->m_fence));
    OKAMI_UNEXPECTED_RETURN_IF(FAILED(hr), Error("Failed to create fence for GpuUploader"));

    // Create initial batch data
    constexpr int BATCH_COUNT = 2;

    for (int i = 0; i < BATCH_COUNT; ++i) {
        Batch batch;

        // Create command allocator
        hr = device.CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&batch.m_commandAllocator));
        OKAMI_UNEXPECTED_RETURN_IF(FAILED(hr), Error("Failed to create command allocator for GpuUploader"));
        
        // Create command list
        hr = device.CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, 
                                    batch.m_commandAllocator.Get(), nullptr, 
                                    IID_PPV_ARGS(&batch.m_commandList));
        OKAMI_UNEXPECTED_RETURN_IF(FAILED(hr), Error("Failed to create command list for GpuUploader"));

        // Close the command list initially
        batch.m_commandList->Close();

        batch.m_commandList->SetName(L"GpuUploaderCommandList");
        batch.m_commandAllocator->SetName(L"GpuUploaderCommandAllocator");

        res->m_batches.emplace_back(std::move(batch));
    }

    return res;
}

GpuUploaderResources::Batch& GpuUploaderResources::GetBatch(UINT64 index) {
    return m_batches[index % m_batches.size()];
}

std::optional<GpuUploaderCommandListLock> GpuUploaderResources::GetExecutableCommandListIfAny() {
    if (m_currentReadIndex < m_currentWriteIndex) {
        auto& batch = GetBatch(m_currentReadIndex);
        if (batch.m_commandList) {
            return GpuUploaderCommandListLock(
                this,
                batch.m_commandList,
                *batch.m_fenceValue,
                m_fence
            );
        }
    }
    return std::nullopt;
}

void GpuUploaderResources::ReleaseLock(GpuUploaderCommandListLock& lock) {
    ++m_currentReadIndex;
    m_uploader->Kick();
}
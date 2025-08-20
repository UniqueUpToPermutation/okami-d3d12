#pragma once

#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <chrono>

#include "common.hpp"
#include "uploader.hpp"
#include "d3d12_common.hpp"

namespace okami::_2 {
    class GpuUploaderCommandListLock {
    private:
        GpuUploaderResources* m_resources;

    public:
        ComPtr<ID3D12GraphicsCommandList> m_commandList;
        UINT64 m_fenceValue;
        ComPtr<ID3D12Fence> m_fenceToSignal;

        ~GpuUploaderCommandListLock();

        inline GpuUploaderCommandListLock(
            GpuUploaderResources* resources,
            ComPtr<ID3D12GraphicsCommandList> commandList,
            UINT64 fenceValue,
            ComPtr<ID3D12Fence> fenceToSignal)
            : m_resources(resources),
              m_commandList(commandList),
              m_fenceValue(fenceValue),
              m_fenceToSignal(fenceToSignal) {}

        inline GpuUploaderCommandListLock(GpuUploaderCommandListLock&& other) {
            m_commandList = std::move(other.m_commandList);
            m_fenceValue = other.m_fenceValue;
            m_fenceToSignal = std::move(other.m_fenceToSignal);
            m_resources = other.m_resources;
            other.m_resources = nullptr;
        }

        inline GpuUploaderCommandListLock& operator=(GpuUploaderCommandListLock&& other) {
            if (this != &other) {
                m_commandList = std::move(other.m_commandList);
                m_fenceValue = other.m_fenceValue;
                m_fenceToSignal = std::move(other.m_fenceToSignal);
                m_resources = other.m_resources;
                other.m_resources = nullptr;
            }
            return *this;
        }

        OKAMI_NO_COPY(GpuUploaderCommandListLock);
    };

    class GpuUploaderResources {
    private:
        std::shared_ptr<IUploaderThread> m_uploader = nullptr;

        struct Batch {
            ComPtr<ID3D12CommandAllocator> m_commandAllocator;
            ComPtr<ID3D12GraphicsCommandList> m_commandList;
            std::optional<UINT64> m_fenceValue = std::nullopt;
        };

        ComPtr<ID3D12Fence> m_fence;
        ComPtr<ID3D12Device> m_device;

        std::vector<Batch> m_batches;
        std::atomic<UINT64> m_currentWriteIndex{0};
        std::atomic<UINT64> m_currentReadIndex{0};

        Batch& GetBatch(UINT64 index) {
            return m_batches[index % m_batches.size()];
        }

    public:
        static Expected<std::unique_ptr<GpuUploaderResources>> Create(
            ID3D12Device& device,
            std::shared_ptr<IUploaderThread> uploader);

        // Gets a command list that is ready to be executed.
        std::optional<GpuUploaderCommandListLock> GetExecutableCommandListIfAny();
    
        void ReleaseLock(GpuUploaderCommandListLock& lock);
    };
}
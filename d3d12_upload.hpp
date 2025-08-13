#pragma once

#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <chrono>

#include "common.hpp"
#include "d3d12_common.hpp"

namespace okami {
    class GpuUploaderTask {
    private:
        UINT64 m_fenceValue;
        Error m_result;

    public:
        // Called on I/O thread
        virtual Error Execute(ID3D12Device& device, ID3D12GraphicsCommandList& commandList) = 0;
        
        // Called on main thread after task has completed uploading to GPU
        virtual Error Finalize() = 0;

        inline Error GetError() const {
            return m_result;
        }

        virtual ~GpuUploaderTask() = default;

        friend struct GpuUploaderImpl;
    };

    struct GpuUploaderImpl;

    struct GpuUploaderImplDelete {
        void operator()(GpuUploaderImpl* impl);
    };

    class GpuUploaderCommandListLock {
    private:
        GpuUploaderImpl* m_uploader = nullptr;

    public:
        ComPtr<ID3D12GraphicsCommandList> m_commandList;
        UINT64 m_fenceValue;
        ComPtr<ID3D12Fence> m_fenceToSignal;

        ~GpuUploaderCommandListLock();

        inline GpuUploaderCommandListLock(
            GpuUploaderImpl* uploader,
            ComPtr<ID3D12GraphicsCommandList> commandList,
            UINT64 fenceValue,
            ComPtr<ID3D12Fence> fenceToSignal)
            : m_uploader(uploader),
              m_commandList(std::move(commandList)),
              m_fenceValue(fenceValue),
              m_fenceToSignal(std::move(fenceToSignal)) {}

        inline GpuUploaderCommandListLock(GpuUploaderCommandListLock&& other) {
            m_commandList = std::move(other.m_commandList);
            m_fenceValue = other.m_fenceValue;
            m_fenceToSignal = std::move(other.m_fenceToSignal);
            m_uploader = other.m_uploader;
            other.m_uploader = nullptr;
        }

        inline GpuUploaderCommandListLock& operator=(GpuUploaderCommandListLock&& other) {
            if (this != &other) {
                m_commandList = std::move(other.m_commandList);
                m_fenceValue = other.m_fenceValue;
                m_fenceToSignal = std::move(other.m_fenceToSignal);
                m_uploader = other.m_uploader;
                other.m_uploader = nullptr;
            }
            return *this;
        }

        OKAMI_NO_COPY(GpuUploaderCommandListLock);
    };

    class GpuUploader {
    private:
        std::unique_ptr<GpuUploaderImpl, GpuUploaderImplDelete> m_impl;

    public:
        // Creates a GpuUploader instance and launches the worker thread.
        // The caller is responsible for calling Stop() to clean up the thread.
        static Expected<GpuUploader> Create(ID3D12Device& device);

        void Stop();

        // Submits a task to the GPU uploader queue.
        void SubmitTask(std::unique_ptr<GpuUploaderTask> task);

        // Gets a command list that is ready to be executed.
        std::optional<GpuUploaderCommandListLock> GetExecutableCommandListIfAny();

        // Finalizes all tasks and cleans up associated resources.
        // Returns the number of tasks still on the GPU.
        size_t FetchAndFinalizeTasks();
    };
}
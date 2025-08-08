#include "d3d12_upload.hpp"
#include <glog/logging.h>
#include <condition_variable>

#include <concurrentqueue/concurrentqueue.h>

using namespace okami;

namespace okami {

struct GpuUploaderImpl {
    std::thread m_thread;
    std::atomic<bool> m_shouldExit = false;
    
    // Condition variable for task notification
    std::condition_variable m_taskCondition;
    std::mutex m_taskMutex;

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

    moodycamel::ConcurrentQueue<std::unique_ptr<GpuUploaderTask>> m_taskQueue;
    moodycamel::ConcurrentQueue<std::unique_ptr<GpuUploaderTask>> m_tasksNeedFinalize;
    std::queue<std::unique_ptr<GpuUploaderTask>> m_tasksOnGpu;

    ~GpuUploaderImpl() {
        Stop();
    }

    static Expected<std::unique_ptr<GpuUploaderImpl, GpuUploaderImplDelete>> Create(ID3D12Device& device) { 
        auto uploader = std::unique_ptr<GpuUploaderImpl, GpuUploaderImplDelete>(new GpuUploaderImpl());
        uploader->m_device = ComPtr<ID3D12Device>(&device);

        // Create fence for synchronization
        HRESULT hr = device.CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&uploader->m_fence));
        if (FAILED(hr)) {
            return std::unexpected(Error("Failed to create fence for GpuUploader"));
        }
        
        // Create initial batch data
        constexpr int BATCH_COUNT = 2;

        for (int i = 0; i < BATCH_COUNT; ++i) {
            Batch batch;

            // Create command allocator
            hr = device.CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&batch.m_commandAllocator));
            if (FAILED(hr)) {
                return std::unexpected(Error("Failed to create command allocator for GpuUploader"));
            }
            
            // Create command list
            hr = device.CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, 
                                        batch.m_commandAllocator.Get(), nullptr, 
                                        IID_PPV_ARGS(&batch.m_commandList));
            if (FAILED(hr)) {
                return std::unexpected(Error("Failed to create command list for GpuUploader"));
            }
            
            // Close the command list initially
            batch.m_commandList->Close();

            uploader->m_batches.emplace_back(std::move(batch));
        }
        
        // Start the worker thread
        uploader->m_thread = std::thread(&GpuUploaderImpl::ThreadFunc, uploader.get());

        return uploader;
    }

    Batch& GetBatch(UINT64 index) {
        return m_batches[index % m_batches.size()];
    }

    std::optional<GpuUploaderCommandList> GetExecutableCommandListIfAny() {
        if (m_currentReadIndex < m_currentWriteIndex) {
            auto& batch = m_batches[m_currentReadIndex];
            if (batch.m_commandList) {
                ++m_currentReadIndex;
                Kick();
                return GpuUploaderCommandList{
                    .m_commandList = batch.m_commandList,
                    .m_fenceValue = *batch.m_fenceValue,
                    .m_fenceToSignal = m_fence
                };
            }
        }
        return std::nullopt;
    }

    void ThreadFunc() {
        LOG(INFO) << "GpuUploader thread started";

        while (m_shouldExit.load() == false) {
            auto currentIndex = m_currentWriteIndex.load();
            auto fenceValue = currentIndex + 1;
            auto& batch = GetBatch(currentIndex);

            // Reset command allocator and list
            batch.m_commandAllocator->Reset();
            batch.m_commandList->Reset(batch.m_commandAllocator.Get(), nullptr);

            while (m_shouldExit.load() == false) {
                // Wait for a task to be available using condition variable
                std::unique_ptr<GpuUploaderTask> task;
                
                // Try to get a task without blocking first
                if (!m_taskQueue.try_dequeue(task)) {
                    // No task available, wait for notification
                    std::unique_lock<std::mutex> lock(m_taskMutex);
                    m_taskCondition.wait(lock, [this] { 
                        std::unique_ptr<GpuUploaderTask> tempTask;
                        return m_shouldExit.load() || m_taskQueue.size_approx() > 0;
                    });
                    
                    // If we're exiting, break out
                    if (m_shouldExit.load()) {
                        break;
                    }
                    
                    // Try to dequeue again after waking up
                    if (!m_taskQueue.try_dequeue(task)) {
                        continue;
                    }
                }

                // Perform the task
                task->m_fenceValue = fenceValue;
                task->Execute(*m_device.Get(), *batch.m_commandList.Get());
                m_tasksNeedFinalize.enqueue(std::move(task));

                bool moveToNextBatch = [&]() {
                    auto& nextBatch = GetBatch(currentIndex + 1);

                    // Next batch is not ready, still executing task on GPU
                    if (nextBatch.m_fenceValue.has_value() &&
                        nextBatch.m_fenceValue.value() < m_fence->GetCompletedValue()) {
                        return false;
                    }
                    // Nobody is waiting for the current batch, so we can continue working.
                    if (m_currentReadIndex.load() < currentIndex) {
                        return false;
                    }

                    return true;
                }();

                // If we can move to the next batch, close the current command list
                if (moveToNextBatch) {
                    break;
                }
            }

            batch.m_commandList->Close();
            batch.m_fenceValue = fenceValue;
            m_currentWriteIndex++;
        }

        LOG(INFO) << "GpuUploader thread exiting";
    }

    void Kick() {
        // Notify the worker thread that a task is available
        {
            std::lock_guard<std::mutex> lock(m_taskMutex);
        }
        m_taskCondition.notify_one();
    }

    void SubmitTask(std::unique_ptr<GpuUploaderTask> task) {
        // Enqueue the task for processing
        m_taskQueue.enqueue(std::move(task));
        Kick();
    }

    void Stop() {
        m_shouldExit.store(true);
        Kick();
        
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    // Executes on main thread to finalize tasks
    // and clean up resources
    size_t FetchAndFinalizeTasks() {
        std::unique_ptr<GpuUploaderTask> task;
        while (m_tasksNeedFinalize.try_dequeue(task)) {
            m_tasksOnGpu.push(std::move(task));
        }
        if (!m_tasksOnGpu.empty()) {
            // Check if top task has completed and finalize it
            while (!m_tasksOnGpu.empty() && m_tasksOnGpu.front()->m_fenceValue <= m_fence->GetCompletedValue()) {
                auto task = std::move(m_tasksOnGpu.front());
                m_tasksOnGpu.pop();
                task->Finalize();
            }
        }
        return m_tasksOnGpu.size();
    }
};

void GpuUploaderImplDelete::operator()(GpuUploaderImpl* impl) {
    if (impl) {
        delete impl;
    }
}

Expected<GpuUploader> GpuUploader::Create(ID3D12Device& device) {
    auto impl = GpuUploaderImpl::Create(device);
    if (!impl.has_value()) {
        return std::unexpected(impl.error());
    }
    GpuUploader uploader;
    uploader.m_impl = std::move(impl.value());
    return uploader;
}

std::optional<GpuUploaderCommandList> GpuUploader::GetExecutableCommandListIfAny() {
    return m_impl->GetExecutableCommandListIfAny();
}

size_t GpuUploader::FetchAndFinalizeTasks() {
    return m_impl->FetchAndFinalizeTasks();
}

void GpuUploader::SubmitTask(std::unique_ptr<GpuUploaderTask> task) {
    m_impl->SubmitTask(std::move(task));
}

void GpuUploader::Stop() {
    m_impl->Stop();
}

} // namespace okami

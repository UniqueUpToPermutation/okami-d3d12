#include "uploader.hpp"

#include <glog/logging.h>
#include <mutex>
#include <condition_variable>
#include <atomic>

using namespace okami;

class ContentLoaderThread final :
    public IUploaderThread {
private:
    std::thread m_thread;
    std::atomic<bool> m_shouldExit = false;
    std::vector<std::shared_ptr<IUploader>> m_uploaders;
    
    // Condition variable for task notification
    std::condition_variable m_taskCondition;
    std::mutex m_taskMutex;

public:
    ContentLoaderThread() {
        m_thread = std::thread(&ContentLoaderThread::ThreadFunc, this);
    }

    ~ContentLoaderThread() {
        Stop();
    }

    Error AddUploader(std::shared_ptr<IUploader> uploader) override {
        m_uploaders.push_back(std::move(uploader));
    }

    std::shared_ptr<IUploader> GetNextNonidleUploader() {
        for (auto& uploader : m_uploaders) {
            if (uploader && uploader->HasPendingUploads()) {
                return uploader;
            }
        }
        return nullptr;
    }

    void ThreadFunc() {
        LOG(INFO) << "Content thread started";

        while (m_shouldExit.load() == false) {
            if (auto uploader = GetNextNonidleUploader()) {
                uploader->Execute();
            } else {
                std::unique_lock<std::mutex> lock(m_taskMutex);
                m_taskCondition.wait(lock, [this] {
                    return m_shouldExit.load() || GetNextNonidleUploader() != nullptr;
                });
            }
        }

        LOG(INFO) << "Content thread exiting";
    }

    void Kick() override {
        // Notify the worker thread that a task is available
        {
            std::lock_guard<std::mutex> lock(m_taskMutex);
        }
        m_taskCondition.notify_one();
    }

    void Stop() override {
        m_shouldExit.store(true);
        Kick(); // Wake up the thread if it's waiting
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
};

Expected<std::unique_ptr<IUploaderThread>> okami::CreateUploaderThread() {
    try {
        return std::make_unique<ContentLoaderThread>();
    } catch (const std::exception& e) {
        LOG(ERROR) << "Failed to create content loader thread: " << e.what();
        return std::unexpected(Error(e.what()));
    }
}
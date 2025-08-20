#pragma once

#include <vector>

#include "common.hpp"

namespace okami {
    class IUploader {
    public:
        virtual bool HasPendingUploads() const = 0;
        virtual void Execute() = 0;
    };

    class IUploaderThread {
    public:
        virtual Error AddUploader(std::shared_ptr<IUploader> uploader) = 0;
        virtual void Kick() = 0;
        virtual void Stop() = 0;

        virtual ~IUploaderThread() = default;
    };

    Expected<std::unique_ptr<IUploaderThread>> CreateUploaderThread();
}
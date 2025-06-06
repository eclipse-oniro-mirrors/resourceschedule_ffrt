/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef FFRT_CONCURRENT_QUEUE_H
#define FFRT_CONCURRENT_QUEUE_H

#include "base_queue.h"

namespace ffrt {
class ConcurrentQueue : public BaseQueue {
public:
    explicit ConcurrentQueue(const int maxConcurrency = 1)
        : maxConcurrency_(maxConcurrency)
    {
        dequeFunc_ = QueueStrategy<QueueTask>::DequeSingleByPriority;
    }
    ~ConcurrentQueue() override;

    int Push(QueueTask* task) override;
    QueueTask* Pull() override;
    bool GetActiveStatus() override
    {
        return concurrency_.load();
    }

    int GetQueueType() const override
    {
        return ffrt_queue_concurrent;
    }

    void Stop() override;
    bool SetLoop(Loop* loop);
    int Remove() override;
    int Remove(const char* name) override;
    int Remove(const QueueTask* task) override;
    bool HasTask(const char* name) override;

    inline bool ClearLoop()
    {
        if (loop_ == nullptr) {
            return false;
        }

        loop_ = nullptr;
        return true;
    }

    bool IsOnLoop() override
    {
        return isOnLoop_.load();
    }

private:
    int PushDelayTaskToTimer(QueueTask* task);

    Loop* loop_ { nullptr };
    std::atomic_bool isOnLoop_ { false };

    int maxConcurrency_ {1};
    std::atomic_int concurrency_ {0};
    std::multimap<uint64_t, QueueTask*> whenMapVec_[4];
};

std::unique_ptr<BaseQueue> CreateConcurrentQueue(const ffrt_queue_attr_t* attr);
} // namespace ffrt
#endif // FFRT_CONCURRENT_QUEUE_H

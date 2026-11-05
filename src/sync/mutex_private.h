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

#ifndef _MUTEX_PRIVATE_H_
#define _MUTEX_PRIVATE_H_

#include "sync/sync.h"

#include "internal_inc/osal.h"
#include "dfx/log/ffrt_log_api.h"
#ifdef FFRT_OH_EVENT_RECORD
#include "hisysevent.h"
#endif

static FFRT_NOINLINE void MutexEmptyLogPrint()
{
    FFRT_LOGE("mutex should not be empty");
}

namespace ffrt {
class mutexBase {
public:
    mutexBase() = default;
    virtual ~mutexBase() = default;
    virtual void lock() {}
    virtual void unlock() {}
    virtual bool try_lock() { return false; }
};

class mutexPrivate : public mutexBase {
    std::atomic<int> l = sync_detail::UNLOCK;
    fast_mutex wlock;
    LinkedList list;

    void wait();
public:
    void wake();
    FFRT_INLINE void lock_slow()
    {
        if (l.load(std::memory_order_relaxed) == sync_detail::WAIT) {
            wait();
        }
        while (l.exchange(sync_detail::WAIT, std::memory_order_acquire) != sync_detail::UNLOCK) {
            wait();
        }
    }

    mutexPrivate() {}
    mutexPrivate(mutexPrivate const &) = delete;
    void operator = (mutexPrivate const &) = delete;

    bool try_lock() override;
    void lock() override;
    void unlock() override;
};

class RecursiveMutexPrivate : public mutexBase {
public:
    void lock() override;
    void unlock() override;
    bool try_lock() override;

    RecursiveMutexPrivate() = default;
    ~RecursiveMutexPrivate() = default;
    RecursiveMutexPrivate(RecursiveMutexPrivate const&) = delete;
    void operator = (RecursiveMutexPrivate const&) = delete;

private:
    std::pair<uint64_t, uint32_t> taskLockNums = std::make_pair(UINT64_MAX, 0);
    fast_mutex fMutex;
    mutexPrivate mt;
};

} // namespace ffrt

#endif

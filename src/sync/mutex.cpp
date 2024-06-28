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

#include <unistd.h>
#include "cpp/mutex.h"
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <map>
#include <functional>
#include "sync/sync.h"
#include "eu/co_routine.h"
#include "internal_inc/osal.h"
#include "internal_inc/types.h"
#include "sync/mutex_private.h"
#include "dfx/log/ffrt_log_api.h"
#include "ffrt_trace.h"
#include "tm/cpu_task.h"

namespace ffrt {
bool mutexPrivate::try_lock()
{
    int v = sync_detail::UNLOCK;
    bool ret = l.compare_exchange_strong(v, sync_detail::LOCK, std::memory_order_acquire, std::memory_order_relaxed);
#ifdef FFRT_MUTEX_DEADLOCK_CHECK
    if (ret) {
        uint64_t task = ExecuteCtx::Cur()->task ? reinterpret_cast<uint64_t>(ExecuteCtx::Cur()->task) : GetTid();
        MutexGraph::Instance().AddNode(task, 0, false);
        owner.store(task, std::memory_order_relaxed);
    }
#endif
    return ret;
}

void mutexPrivate::lock()
{
#ifdef FFRT_MUTEX_DEADLOCK_CHECK
    uint64_t task;
    uint64_t ownerTask;
    task = ExecuteCtx::Cur()->task ? reinterpret_cast<uint64_t>(ExecuteCtx::Cur()->task) : GetTid();
    ownerTask = owner.load(std::memory_order_relaxed);
    if (ownerTask) {
        MutexGraph::Instance().AddNode(task, ownerTask, true);
    } else {
        MutexGraph::Instance().AddNode(task, 0, false);
    }
#endif
    int v = sync_detail::UNLOCK;
    if (l.compare_exchange_strong(v, sync_detail::LOCK, std::memory_order_acquire, std::memory_order_relaxed)) {
        goto lock_out;
    }
    if (l.load(std::memory_order_relaxed) == sync_detail::WAIT) {
        wait();
    }
    while (l.exchange(sync_detail::WAIT, std::memory_order_acquire) != sync_detail::UNLOCK) {
        wait();
    }

lock_out:
#ifdef FFRT_MUTEX_DEADLOCK_CHECK
    owner.store(task, std::memory_order_relaxed);
#endif
    return;
}

bool RecursiveMutexPrivate::try_lock()
{
    auto ctx = ExecuteCtx::Cur();
    auto task = ctx->task;
    if ((!USE_COROUTINE) || (task == nullptr)) {
        fMutex.lock();
        if (taskLockNums.first == UINT64_MAX) {
            fMutex.unlock();
            mt.lock();
            fMutex.lock();
            taskLockNums = std::make_pair(GetTid(), 1);
            fMutex.unlock();
            return true;
        }

        if (taskLockNums.first == GetTid()) {
            taskLockNums.second += 1;
            fMutex.unlock();
            return true;
        }

        fMutex.unlock();
        return false;
    }

    fMutex.lock();
    if (taskLockNums.first == UINT64_MAX) {
        fMutex.unlock();
        mt.lock();
        fMutex.lock();
        taskLockNums = std::make_pair(task->gid | 0x8000000000000000, 1);
        fMutex.unlock();
        return true;
    }

    if (taskLockNums.first == (task->gid | 0x8000000000000000)) {
        taskLockNums.second += 1;
        fMutex.unlock();
        return true;
    }

    fMutex.unlock();
    return false;
}

void RecursiveMutexPrivate::lock()
{
    auto ctx = ExecuteCtx::Cur();
    auto task = ctx->task;
    if ((!USE_COROUTINE) || (task == nullptr)) {
        fMutex.lock();
        if (taskLockNums.first != GetTid()) {
            fMutex.unlock();
            mt.lock();
            fMutex.lock();
            taskLockNums = std::make_pair(GetTid(), 1);
            fMutex.unlock();
            return;
        }

        taskLockNums.second += 1;
        fMutex.unlock();
        return;
    }

    fMutex.lock();
    if (taskLockNums.first != (task->gid | 0x8000000000000000)) {
        fMutex.unlock();
        mt.lock();
        fMutex.lock();
        taskLockNums = std::make_pair(task->gid | 0x8000000000000000, 1);
        fMutex.unlock();
        return;
    }

    taskLockNums.second += 1;
    fMutex.unlock();
}

void RecursiveMutexPrivate::unlock()
{
    auto ctx = ExecuteCtx::Cur();
    auto task = ctx->task;
    if ((!USE_COROUTINE) || (task == nullptr)) {
        fMutex.lock();
        if (taskLockNums.first != GetTid()) {
            fMutex.unlock();
            return;
        }

        if (taskLockNums.second == 1) {
            taskLockNums = std::make_pair(UINT64_MAX, 0);
            fMutex.unlock();
            mt.unlock();
            return;
        }

        taskLockNums.second -= 1;
        fMutex.unlock();
        return;
    }

    fMutex.lock();
    if (taskLockNums.first != (task->gid | 0x8000000000000000)) {
        fMutex.unlock();
        return;
    }

    if (taskLockNums.second == 1) {
        taskLockNums = std::make_pair(UINT64_MAX, 0);
        fMutex.unlock();
        mt.unlock();
        return;
    }

    taskLockNums.second -= 1;
    fMutex.unlock();
}

void mutexPrivate::unlock()
{
#ifdef FFRT_MUTEX_DEADLOCK_CHECK
    uint64_t ownerTask = owner.load(std::memory_order_relaxed);
    owner.store(0, std::memory_order_relaxed);
    MutexGraph::Instance().RemoveNode(ownerTask);
#endif
    if (l.exchange(sync_detail::UNLOCK, std::memory_order_release) == sync_detail::WAIT) {
        wake();
    }
}

void mutexPrivate::wait()
{
    auto ctx = ExecuteCtx::Cur();
    auto task = ctx->task;
    bool legacyMode = LegacyMode(task);
    if (!USE_COROUTINE || task == nullptr || legacyMode) {
        wlock.lock();
        if (l.load(std::memory_order_relaxed) != sync_detail::WAIT) {
            wlock.unlock();
            return;
        }
        list.PushBack(ctx->wn.node);
        std::unique_lock<std::mutex> lk(ctx->wn.wl);
        if (legacyMode) {
            task->coRoutine->blockType = BlockType::BLOCK_THREAD;
            ctx->wn.task = task;
        }
        wlock.unlock();
        ctx->wn.cv.wait(lk);
        return;
    } else {
        FFRT_BLOCK_TRACER(task->gid, mtx);
        CoWait([this](CPUEUTask* task) -> bool {
            wlock.lock();
            if (l.load(std::memory_order_relaxed) != sync_detail::WAIT) {
                wlock.unlock();
                return false;
            }
            list.PushBack(task->fq_we.node);
            wlock.unlock();
            // The ownership of the task belongs to ReadyTaskQueue, and the task cannot be accessed any more.
            return true;
        });
    }
}

void mutexPrivate::wake()
{
    wlock.lock();
    if (list.Empty()) {
        wlock.unlock();
        return;
    }
    WaitEntry* we = list.PopFront(&WaitEntry::node);
    if (we == nullptr) {
        wlock.unlock();
        return;
    }
    CPUEUTask* task = we->task;
    bool blockThread = BlockThread(task);
    if (!USE_COROUTINE || we->weType == 2 || blockThread) {
        WaitUntilEntry* wue = static_cast<WaitUntilEntry*>(we);
        std::unique_lock lk(wue->wl);
        if (blockThread) {
            task->coRoutine->blockType = BlockType::BLOCK_COROUTINE;
            we->task = nullptr;
        }
        wlock.unlock();
        wue->cv.notify_one();
    } else {
        wlock.unlock();
        CoRoutineFactory::CoWakeFunc(task, false);
    }
}
} // namespace ffrt

#ifdef __cplusplus
extern "C" {
#endif
API_ATTRIBUTE((visibility("default")))
int ffrt_mutex_init(ffrt_mutex_t* mutex, const ffrt_mutexattr_t* attr)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_error_inval;
    }
    if (attr != nullptr) {
        FFRT_LOGE("only support normal mutex");
        return ffrt_error;
    }
    static_assert(sizeof(ffrt::mutexPrivate) <= ffrt_mutex_storage_size,
        "size must be less than ffrt_mutex_storage_size");

    new (mutex)ffrt::mutexPrivate();
    return ffrt_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_recursive_mutex_init(ffrt_mutex_t* mutex, const ffrt_mutexattr_t* attr)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_error_inval;
    }
    if (attr != nullptr) {
        FFRT_LOGE("only support normal mutex");
        return ffrt_error;
    }
    static_assert(sizeof(ffrt::RecursiveMutexPrivate) <= ffrt_mutex_storage_size,
        "size must be less than ffrt_mutex_storage_size");

    new (mutex)ffrt::RecursiveMutexPrivate();
    return ffrt_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_mutex_lock(ffrt_mutex_t* mutex)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_error_inval;
    }
    auto p = reinterpret_cast<ffrt::mutexPrivate*>(mutex);
    p->lock();
    return ffrt_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_recursive_mutex_lock(ffrt_mutex_t* mutex)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_error_inval;
    }
    auto p = reinterpret_cast<ffrt::RecursiveMutexPrivate*>(mutex);
    p->lock();
    return ffrt_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_mutex_unlock(ffrt_mutex_t* mutex)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_error_inval;
    }
    auto p = reinterpret_cast<ffrt::mutexPrivate*>(mutex);
    p->unlock();
    return ffrt_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_recursive_mutex_unlock(ffrt_mutex_t* mutex)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_error_inval;
    }
    auto p = reinterpret_cast<ffrt::RecursiveMutexPrivate*>(mutex);
    p->unlock();
    return ffrt_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_mutex_trylock(ffrt_mutex_t* mutex)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_error_inval;
    }
    auto p = reinterpret_cast<ffrt::mutexPrivate*>(mutex);
    return p->try_lock() ? ffrt_success : ffrt_error_busy;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_recursive_mutex_trylock(ffrt_mutex_t* mutex)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_error_inval;
    }
    auto p = reinterpret_cast<ffrt::RecursiveMutexPrivate*>(mutex);
    return p->try_lock() ? ffrt_success : ffrt_error_busy;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_mutex_destroy(ffrt_mutex_t* mutex)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_error_inval;
    }
    auto p = reinterpret_cast<ffrt::mutexPrivate*>(mutex);
    p->~mutexPrivate();
    return ffrt_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_recursive_mutex_destroy(ffrt_mutex_t* mutex)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_error_inval;
    }
    auto p = reinterpret_cast<ffrt::RecursiveMutexPrivate*>(mutex);
    p->~RecursiveMutexPrivate();
    return ffrt_success;
}

#ifdef __cplusplus
}
#endif

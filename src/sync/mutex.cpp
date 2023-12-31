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
#include "core/task_ctx.h"
#include "eu/co_routine.h"
#include "internal_inc/osal.h"
#include "sync/mutex_private.h"
#include "dfx/log/ffrt_log_api.h"
#include "dfx/trace/ffrt_trace.h"

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

void mutexPrivate::unlock()
{
#ifdef FFRT_MUTEX_DEADLOCK_CHECK
    owner.store(0, std::memory_order_relaxed);
    uint64_t task = ExecuteCtx::Cur()->task ? reinterpret_cast<uint64_t>(ExecuteCtx::Cur()->task) : GetTid();
    MutexGraph::Instance().RemoveNode(task);
#endif
    if (l.exchange(sync_detail::UNLOCK, std::memory_order_release) == sync_detail::WAIT) {
        wake();
    }
}

void mutexPrivate::wait()
{
    auto ctx = ExecuteCtx::Cur();
    auto task = ctx->task;
    if (task == nullptr) {
        wlock.lock();
        if (l.load(std::memory_order_relaxed) != sync_detail::WAIT) {
            wlock.unlock();
            return;
        }
        list.PushBack(ctx->wn.node);
        std::unique_lock<std::mutex> lk(ctx->wn.wl);
        wlock.unlock();
        ctx->wn.cv.wait(lk);
        return;
    } else {
        FFRT_BLOCK_TRACER(task->gid, mtx);
        CoWait([this](TaskCtx* inTask) -> bool {
            wlock.lock();
            if (l.load(std::memory_order_relaxed) != sync_detail::WAIT) {
                wlock.unlock();
                return false;
            }
            list.PushBack(inTask->fq_we.node);
            wlock.unlock();
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
    TaskCtx* task = we->task;
    if (we->weType == 2) {
        WaitUntilEntry* wue = static_cast<WaitUntilEntry*>(we);
        std::unique_lock lk(wue->wl);
        wlock.unlock();
        wue->cv.notify_one();
    } else {
        wlock.unlock();
        CoWake(task, false);
    }
}
} // namespace ffrt

#ifdef __cplusplus
extern "C" {
#endif
API_ATTRIBUTE((visibility("default")))
int ffrt_mutex_init(ffrt_mutex_t *mutex, const ffrt_mutexattr_t* attr)
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
int ffrt_mutex_lock(ffrt_mutex_t* mutex)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_error_inval;
    }
    auto p = (ffrt::mutexPrivate*)mutex;
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
    auto p = (ffrt::mutexPrivate*)mutex;
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
    auto p = (ffrt::mutexPrivate*)mutex;
    return p->try_lock() ? ffrt_success : ffrt_error_busy;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_mutex_destroy(ffrt_mutex_t* mutex)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_error_inval;
    }
    auto p = (ffrt::mutexPrivate*)mutex;
    p->~mutexPrivate();
    return ffrt_success;
}
#ifdef __cplusplus
}
#endif

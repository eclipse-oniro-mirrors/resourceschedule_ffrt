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

#include "wait_queue.h"
#include "sched/execute_ctx.h"
#include "eu/co_routine.h"
#include "dfx/log/ffrt_log_api.h"
#include "ffrt_trace.h"
#include "sync/mutex_private.h"
#include "tm/cpu_task.h"

namespace ffrt {
TaskWithNode::TaskWithNode()
{
    auto ctx = ExecuteCtx::Cur();
    task = ctx->task;
}

void WaitQueue::ThreadWait(WaitUntilEntry* wn, mutexPrivate* lk, bool legacyMode, CPUEUTask* task)
{
    wqlock.lock();
    if (legacyMode) {
        task->blockType = BlockType::BLOCK_THREAD;
        wn->task = task;
    }
    push_back(wn);
    wqlock.unlock();
    {
        std::unique_lock<std::mutex> nl(wn->wl);
        lk->unlock();
        wn->cv.wait(nl);
    }
    wqlock.lock();
    remove(wn);
    wqlock.unlock();
    lk->lock();
}
bool WaitQueue::ThreadWaitUntil(WaitUntilEntry* wn, mutexPrivate* lk,
    const TimePoint& tp, bool legacyMode, CPUEUTask* task)
{
    bool ret = false;
    wqlock.lock();
    if (legacyMode) {
        task->blockType = BlockType::BLOCK_THREAD;
        wn->task = task;
    }
    push_back(wn);
    wqlock.unlock();
    {
        std::unique_lock<std::mutex> nl(wn->wl);
        lk->unlock();
        if (wn->cv.wait_until(nl, tp) == std::cv_status::timeout) {
            ret = true;
        }
    }
    wqlock.lock();
    remove(wn);
    wqlock.unlock();
    lk->lock();
    return ret;
}

void WaitQueue::SuspendAndWait(mutexPrivate* lk)
{
    ExecuteCtx* ctx = ExecuteCtx::Cur();
    CPUEUTask* task = ctx->task;
    if (ThreadWaitMode(task)) {
        ThreadWait(&ctx->wn, lk, LegacyMode(task), task);
        return;
    }
    task->wue = new WaitUntilEntry(task);
    FFRT_BLOCK_TRACER(task->gid, cnd);
    CoWait([&](CPUEUTask* task) -> bool {
        wqlock.lock();
        push_back(task->wue);
        lk->unlock(); // Unlock needs to be in wqlock protection, guaranteed to be executed before lk.lock after CoWake
        wqlock.unlock();
        // The ownership of the task belongs to WaitQueue list, and the task cannot be accessed any more.
        return true;
    });
    delete task->wue;
    task->wue = nullptr;
    lk->lock();
}

bool WeTimeoutProc(WaitQueue* wq, WaitUntilEntry* wue)
{
    wq->wqlock.lock();
    bool toWake = true;

    // two kinds: 1) notify was not called, timeout grabbed the lock first
    if (wue->status.load(std::memory_order_acquire) == we_status::INIT) {
        // timeout processes wue first, cv will not be processed again, thmeout is reponsible for destorying wue
        wq->remove(wue);
        delete wue;
        wue = nullptr;
    } else {
        // 2) notify enters the critical section, first writes the notify status, and then releases the lock
        // notify is responsible for destorying wue;
        wue->status.store(we_status::TIMEOUT_DONE, std::memory_order_release);
        toWake = false;
    }
    wq->wqlock.unlock();
    return toWake;
}

bool WaitQueue::SuspendAndWaitUntil(mutexPrivate* lk, const TimePoint& tp) noexcept
{
    bool ret = false;
    ExecuteCtx* ctx = ExecuteCtx::Cur();
    CPUEUTask* task = ctx->task;
    if (ThreadWaitMode(task))  {
        return ThreadWaitUntil(&ctx->wn, lk, tp, LegacyMode(task), task);
    }
    task->wue = new WaitUntilEntry(task);
    task->wue->hasWaitTime = true;
    task->wue->tp = tp;
    task->wue->cb = ([&](WaitEntry* we) {
        WaitUntilEntry* wue = static_cast<WaitUntilEntry*>(we);
        ffrt::CPUEUTask* task = wue->task;
        if (!WeTimeoutProc(this, wue)) {
            return;
        }
        FFRT_LOGD("task(%d) timeout out", task->gid);
        CoRoutineFactory::CoWakeFunc(task, true);
    });
    FFRT_BLOCK_TRACER(task->gid, cnt);
    CoWait([&](CPUEUTask* task) -> bool {
        WaitUntilEntry* we = task->wue;
        wqlock.lock();
        push_back(we);
        lk->unlock(); // Unlock needs to be in wqlock protection, guaranteed to be executed before lk.lock after CoWake
        // The ownership of the task belongs to WaitQueue list, and the task cannot be accessed any more.
        if (DelayedWakeup(we->tp, we, we->cb)) {
            wqlock.unlock();
            return true;
        } else {
            wqlock.unlock();
            if (!WeTimeoutProc(this, we)) {
                return true;
            }
            task->wakeupTimeOut = true;
            return false;
        }
    });
    ret = task->wakeupTimeOut;
    task->wue = nullptr;
    task->wakeupTimeOut = false;
    lk->lock();
    return ret;
}

bool WaitQueue::WeNotifyProc(WaitUntilEntry* we)
{
    if (!we->hasWaitTime) {
        // For wait task without timeout, we will be deleted after the wait task wakes up
        return true;
    }

    WaitEntry *dwe = static_cast<WaitEntry*>(we);
    if (!DelayedRemove(we->tp, dwe)) {
        // Deletion of timer failed during the notify process, indicating that timer cb has been executed at this time
        // waiting for cb execution to complete, adn marking notify as being processed.
        we->status.store(we_status::NOTIFING, std::memory_order_release);
        wqlock.unlock();
        while (we->status.load(std::memory_order_acquire) != we_status::TIMEOUT_DONE) {
        }
        wqlock.lock();
    }

    delete we;
    return true;
}

void WaitQueue::NotifyOne() noexcept
{
    wqlock.lock();
    while (!empty()) {
        WaitUntilEntry* we = pop_front();
        if (we == nullptr) {
            break;
        }
        CPUEUTask* task = we->task;
        if (ThreadNotifyMode(task) || we->weType == 2) {
            std::unique_lock<std::mutex> lk(we->wl);
            if (BlockThread(task)) {
                task->blockType = BlockType::BLOCK_COROUTINE;
                we->task = nullptr;
            }
            wqlock.unlock();
            we->cv.notify_one();
        } else {
            if (!WeNotifyProc(we)) {
                continue;
            }
            wqlock.unlock();
            CoRoutineFactory::CoWakeFunc(task, false);
        }
        return;
    }
    wqlock.unlock();
}

void WaitQueue::NotifyAll() noexcept
{
    wqlock.lock();
    while (!empty()) {
        WaitUntilEntry* we = pop_front();
        if (we == nullptr) {
            break;
        }
        CPUEUTask* task = we->task;
        if (ThreadNotifyMode(task) || we->weType == 2) {
            std::unique_lock<std::mutex> lk(we->wl);
            if (BlockThread(task)) {
                task->blockType = BlockType::BLOCK_COROUTINE;
                we->task = nullptr;
            }
            wqlock.unlock();
            we->cv.notify_one();
        } else {
            if (!WeNotifyProc(we)) {
                continue;
            }
            wqlock.unlock();
            CoRoutineFactory::CoWakeFunc(task, false);
        }
        wqlock.lock();
    }
    wqlock.unlock();
}
} // namespace ffrt

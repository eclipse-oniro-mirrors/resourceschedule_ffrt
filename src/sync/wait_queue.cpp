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
#include "core/task_ctx.h"
#include "eu/co_routine.h"
#include "dfx/log/ffrt_log_api.h"
#include "dfx/trace/ffrt_trace.h"
#include "sync/mutex_private.h"

namespace ffrt {
TaskWithNode::TaskWithNode()
{
    auto ctx = ExecuteCtx::Cur();
    task = ctx->task;
}

void WaitQueue::ThreadWait(WaitUntilEntry* wn, mutexPrivate* lk)
{
    wqlock.lock();
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
bool WaitQueue::ThreadWaitUntil(WaitUntilEntry* wn, mutexPrivate* lk, const TimePoint& tp)
{
    bool ret = false;
    wqlock.lock();
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
    TaskCtx* task = ctx->task;
    if (ctx->task == nullptr) {
        ThreadWait(&ctx->wn, lk);
        return;
    }
    task->wue = new WaitUntilEntry(task);
    FFRT_BLOCK_TRACER(task->gid, cnd);
    CoWait([&](TaskCtx* inTask) -> bool {
        wqlock.lock();
        push_back(inTask->wue);
        lk->unlock(); // Unlock needs to be in wqlock protection, guaranteed to be executed before lk.lock after CoWake
        wqlock.unlock();
        return true;
    });
    delete task->wue;
    task->wue = nullptr;
    lk->lock();
}

bool WeTimeoutProc(WaitUntilEntry* wue)
{
    int expected = we_status::INIT;
    if (!atomic_compare_exchange_strong_explicit(
        &wue->status, &expected, we_status::TIMEOUT, std::memory_order_seq_cst, std::memory_order_seq_cst)) {
        // The critical point wue->status has been written, notify will no longer access wue, it can be deleted
        delete wue;
        return false;
    }

    return true;
}

bool WaitQueue::SuspendAndWaitUntil(mutexPrivate* lk, const TimePoint& tp) noexcept
{
    bool ret = false;
    ExecuteCtx* ctx = ExecuteCtx::Cur();
    TaskCtx* task = ctx->task;
    if (task == nullptr) {
        return ThreadWaitUntil(&ctx->wn, lk, tp);
    }

    task->wue = new WaitUntilEntry(task);
    task->wue->hasWaitTime = true;
    task->wue->tp = tp;
    task->wue->cb = ([](WaitEntry* we) {
        WaitUntilEntry* wue = static_cast<WaitUntilEntry*>(we);
        ffrt::TaskCtx* task = wue->task;
        if (!WeTimeoutProc(wue)) {
            return;
        }
        FFRT_LOGD("task(%s) timeout out", task->label.c_str());
        CoWake(task, true);
    });
    FFRT_BLOCK_TRACER(task->gid, cnt);
    CoWait([&](TaskCtx* inTask) -> bool {
        WaitUntilEntry* we = inTask->wue;
        wqlock.lock();
        push_back(we);
        lk->unlock(); // Unlock needs to be in wqlock protection, guaranteed to be executed before lk.lock after CoWake
        wqlock.unlock();
        if (DelayedWakeup(we->tp, we, we->cb)) {
            return true;
        } else {
            if (!WeTimeoutProc(we)) {
                return true;
            }
            inTask->wakeupTimeOut = true;
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
        return true;
    }

    auto expected = we_status::INIT;
    if (!atomic_compare_exchange_strong_explicit(
        &we->status, &expected, we_status::NOTIFIED, std::memory_order_seq_cst, std::memory_order_seq_cst)) {
        // The critical point we->status has been written, notify will no longer access we, it can be deleted
        delete we;
        return false;
    }

    return true;
}

void WaitQueue::NotifyOne() noexcept
{
    wqlock.lock();
    if (empty()) {
        wqlock.unlock();
        return;
    }
    WaitUntilEntry* we = pop_front();
    TaskCtx* task = we->task;
    if (we->weType == 2) {
        std::unique_lock<std::mutex> lk(we->wl);
        wqlock.unlock();
        we->cv.notify_one();
    } else {
        wqlock.unlock();
        if (!WeNotifyProc(we)) {
            return;
        }
        CoWake(task, false);
    }
}
void WaitQueue::NotifyAll() noexcept
{
    wqlock.lock();
    while (!empty()) {
        WaitUntilEntry* we = pop_front();
        TaskCtx* task = we->task;
        if (we->weType == 2) {
            std::unique_lock<std::mutex> lk(we->wl);
            wqlock.unlock();
            we->cv.notify_one();
        } else {
            wqlock.unlock();
            if (!WeNotifyProc(we)) {
                continue;
            }
            CoWake(task, false);
        }
        wqlock.lock();
    }
    wqlock.unlock();
}
} // namespace ffrt

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
#include "queue_task.h"
#include "ffrt_trace.h"
#include "dfx/log/ffrt_log_api.h"
#include "c/task.h"
#include "util/slab.h"
#include "tm/task_factory.h"

namespace {
constexpr uint64_t MIN_SCHED_TIMEOUT = 100000; // 0.1s
}
namespace ffrt {
QueueTask::QueueTask(QueueHandler* handler, const task_attr_private* attr, bool insertHead)
    : handler_(handler), insertHead_(insertHead)
{
    type = ffrt_queue_task;
    if (handler) {
        if (attr) {
            label = handler->GetName() + "_" + attr->name_ + "_" + std::to_string(gid);
        } else {
            label = handler->GetName() + "_" + std::to_string(gid);
        }
    }

    fq_we.task = reinterpret_cast<CPUEUTask*>(this);
    uptime_ = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());

    if (attr) {
        delay_ = attr->delay_;
        qos_ = attr->qos_;
        uptime_ += delay_;
        prio_ = attr->prio_;
        stack_size = std::max(attr->stackSize_, MIN_STACK_SIZE);
        if (delay_ && attr->timeout_) {
            FFRT_LOGW("task [gid=%llu] not support delay and timeout at the same time, timeout ignored", gid);
        } else if (attr->timeout_) {
            schedTimeout_ = std::max(attr->timeout_, MIN_SCHED_TIMEOUT); // min 0.1s
        }
    }

    FFRT_LOGD("ctor task [gid=%llu], delay=%lluus, type=%lu, prio=%d, timeout=%luus", gid, delay_, type, prio_,
        schedTimeout_);
}

QueueTask::~QueueTask()
{
    FFRT_LOGD("dtor task [gid=%llu]", gid);
}

void QueueTask::Destroy()
{
    // release user func
    auto f = reinterpret_cast<ffrt_function_header_t*>(func_storage);
    f->destroy(f);
    // free serial task object
    DecDeleteRef();
}

void QueueTask::Notify()
{
    std::unique_lock lock(mutex_);
    isFinished_.store(true);
    if (onWait_) {
        waitCond_.notify_all();
    }
}

void QueueTask::Execute()
{
    IncDeleteRef();
    FFRT_LOGD("Execute stask[%lu], name[%s]", gid, label.c_str());
    if (isFinished_.load()) {
        FFRT_LOGE("task [gid=%llu] is complete, no need to execute again", gid);
        DecDeleteRef();
        return;
    }

    handler_->Dispatch(this);
    FFRT_TASKDONE_MARKER(gid);
    DecDeleteRef();
}

void QueueTask::Wait()
{
    std::unique_lock lock(mutex_);
    onWait_ = true;
    while (!isFinished_.load()) {
        waitCond_.wait(lock);
    }
}

void QueueTask::FreeMem()
{
    TaskFactory<QueueTask>::Free(this);
}

uint32_t QueueTask::GetQueueId() const
{
    return handler_->GetQueueId();
}
} // namespace ffrt

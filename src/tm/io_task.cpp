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

#include "io_task.h"
#include "dfx/trace_record/ffrt_trace_record.h"
#include "dfx/trace/ffrt_trace.h"
#include "dfx/bbox/bbox.h"
#include "tm/task_factory.h"

namespace ffrt {
IOTask::IOTask(const ffrt_io_callable_t& work, const task_attr_private* attr) : work(work)
{
    this->type = ffrt_io_task;
    this->qos_ = (attr == nullptr || attr->qos_ == qos_inherit) ? QoS()
        : QoS(attr->qos_);
    fq_we.task = reinterpret_cast<CPUEUTask*>(this);
}

void IOTask::FreeMem()
{
    TaskFactory<IOTask>::Free(this);
}

void IOTask::Execute()
{
    FFRTTraceRecord::TaskExecute<ffrt_io_task>(qos_);
    FFRT_EXECUTOR_TASK_BEGIN(this);
    status = ExecTaskStatus::ET_EXECUTING;
    ffrt_coroutine_ptr_t coroutine = work.exec;
    ffrt_coroutine_ret_t ret = coroutine(work.data);
    if (ret == ffrt_coroutine_ready) {
        status = ExecTaskStatus::ET_FINISH;
        work.destroy(work.data);
        DecDeleteRef();
        FFRT_EXECUTOR_TASK_END();
        return;
    }
    FFRT_EXECUTOR_TASK_BLOCK_MARKER(this);
    status = ExecTaskStatus::ET_PENDING;
#ifdef FFRT_BBOX_ENABLE
    TaskPendingCounterInc();
#endif
    FFRT_EXECUTOR_TASK_END();
    FFRTTraceRecord::TaskDone<ffrt_io_task>(qos_);
}
} // namespace ffrt
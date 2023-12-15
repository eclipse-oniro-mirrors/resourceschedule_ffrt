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

#ifndef FFRT_CPU_TASK_H
#define FFRT_CPU_TASK_H


#include <string>
#include <functional>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <atomic>
#include <string>
#include <set>
#include <list>
#include <memory>
#include "sched/task_state.h"
#include "sched/interval.h"
#include "eu/co_routine.h"
#include "core/task_attr_private.h"
#include "core/task_io.h"
#include "util/task_deleter.h"

namespace ffrt {
struct VersionCtx;
class SCPUEUTask;
class TaskBase {
public:
    uintptr_t reserved = 0;
    uintptr_t type = 0;
    WaitEntry fq_we; // used on fifo fast que
    TaskBase();
    const uint64_t gid; // global unique id in this process
};

#ifdef FFRT_IO_TASK_SCHEDULER
class UserDefinedTask : public TaskBase {
    ffrt_io_callable_t work;
    ExecTaskStatus status;
};
#endif

class CPUEUTask : public TaskBase, public TaskDeleter {
public:
    CPUEUTask(const task_attr_private* attr, CPUEUTask* parent, const uint64_t& id, const QoS &qos);
    WaitUntilEntry* wue;
    bool wakeupTimeOut = false;
    SkipStatus skipped = SkipStatus::SUBMITTED;
    TaskStatus status = TaskStatus::PENDING;

    uint8_t func_storage[ffrt_auto_managed_function_storage_size]; // 函数闭包、指针或函数对象
    CPUEUTask* parent = nullptr;
    const uint64_t rank = 0x0;
    CoRoutine* coRoutine = nullptr;
    std::vector<std::string> traceTag;
    std::mutex lock; // used in coroute

    TaskState state;

    /* The current number of child nodes does not represent the real number of child nodes,
     * because the dynamic graph child nodes will grow to assist in the generation of id
     */
    std::atomic<uint64_t> childNum {0};

    std::string label; // used for debug

    QoS qos;
    void SetQos(QoS& newQos);
    uint64_t reserved[8];

    void freeMem() override;

    virtual void RecycleTask() = 0;
    inline bool IsRoot()
    {
        if (parent == nullptr) {
            return true;
        }
        return false;
    }

    int UpdateState(TaskState::State taskState)
    {
        return TaskState::OnTransition(taskState, this);
    }

    int UpdateState(TaskState::State taskState, TaskState::Op&& op)
    {
        return TaskState::OnTransition(taskState, this, std::move(op));
    }

    void SetTraceTag(const char* name)
    {
        traceTag.emplace_back(name);
    }

    void ClearTraceTag()
    {
        if (!traceTag.empty()) {
            traceTag.pop_back();
        }
    }

#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    static void DumpTask(CPUEUTask* task, std::string& stackInfo, uint8_t flag = 0); /* 0:hilog others:hiview */
#endif
};
} /* namespace ffrt */
#endif
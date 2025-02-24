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

#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
#include <sstream>
#include "backtrace_local.h"
#endif
#include "dfx/trace_record/ffrt_trace_record.h"
#include "dm/dependence_manager.h"
#include "util/slab.h"
#include "internal_inc/osal.h"
#include "internal_inc/types.h"
#include "tm/cpu_task.h"

namespace ffrt {
static inline const char* DependenceStr(Dependence d)
{
    static const char* m[] = {
        "DEPENDENCE_INIT",
        "DATA_DEPENDENCE",
        "CALL_DEPENDENCE",
        "CONDITION_DEPENDENCE",
    };
    return m[static_cast<uint64_t>(d)];
}

SCPUEUTask::SCPUEUTask(const task_attr_private *attr, CPUEUTask *parent, const uint64_t &id,
    const QoS &qos)
    : CPUEUTask(attr, parent, id, qos)
{
}

void SCPUEUTask::DecDepRef()
{
    if (--dataRefCnt.submitDep == 0) {
        FFRT_LOGD("Undependency completed, enter ready queue, task[%lu], name[%s]", gid, label.c_str());
        FFRTTraceRecord::TaskEnqueue<ffrt_normal_task>(GetQos());
        this->UpdateState(TaskState::READY);
    }
}

void SCPUEUTask::DecChildRef()
{
    SCPUEUTask* parent = reinterpret_cast<SCPUEUTask*>(this->parent);
    FFRT_TRACE_SCOPE(2, taskDecChildRef);
    std::unique_lock<decltype(parent->mutex_)> lck(parent->mutex_);
    parent->childRefCnt--;
    if (parent->childRefCnt != 0) {
        return;
    }
    if (FFRT_UNLIKELY(parent->IsRoot())) {
        RootTask *root = static_cast<RootTask *>(parent);
        if (root->thread_exit) {
            lck.unlock();
            delete root;
            return;
        }
    }

    if (!parent->IsRoot() && parent->status == TaskStatus::RELEASED && parent->childRefCnt == 0) {
        FFRT_LOGD("free CPUEUTask:%s gid=%lu", parent->label.c_str(), parent->gid);
        lck.unlock();
        parent->DecDeleteRef();
        return;
    }
    if (parent->dependenceStatus != Dependence::CALL_DEPENDENCE) {
        return;
    }
    parent->dependenceStatus = Dependence::DEPENDENCE_INIT;

    if (ThreadNotifyMode(parent) || parent->IsRoot()) {
        if (BlockThread(parent)) {
            parent->blockType = BlockType::BLOCK_COROUTINE;
        }
        parent->waitCond_.notify_all();
    } else {
        parent->UpdateState(TaskState::READY);
    }
}

void SCPUEUTask::DecWaitDataRef()
{
    FFRT_TRACE_SCOPE(2, taskDecWaitData);
    {
        std::lock_guard<decltype(mutex_)> lck(mutex_);
        if (--dataRefCnt.waitDep != 0) {
            return;
        }
        if (dependenceStatus != Dependence::DATA_DEPENDENCE) {
            return;
        }
        dependenceStatus = Dependence::DEPENDENCE_INIT;
    }

    if (ThreadNotifyMode(this) || IsRoot()) {
        if (BlockThread(this)) {
            blockType = BlockType::BLOCK_COROUTINE;
        }
        waitCond_.notify_all();
    } else {
        FFRTTraceRecord::TaskEnqueue<ffrt_normal_task>(GetQos());
        this->UpdateState(TaskState::READY);
    }
}

void SCPUEUTask::RecycleTask()
{
    std::unique_lock<decltype(mutex_)> lck(mutex_);
    if (childRefCnt == 0) {
        FFRT_LOGD("free SCPUEUTask:%s gid=%lu", label.c_str(), gid);
        lck.unlock();
        DecDeleteRef();
        return;
    } else {
        status = TaskStatus::RELEASED;
    }
}

void SCPUEUTask::MultiDependenceAdd(Dependence depType)
{
    FFRT_LOGD("task(%s) ADD_DEPENDENCE(%s)", this->label.c_str(), DependenceStr(depType));
    dependenceStatus = depType;
}
} /* namespace ffrt */

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

#include <cstring>
#include <sys/stat.h>
#include "eu/cpu_monitor.h"
#include "eu/cpu_manager_interface.h"
#include "sched/scheduler.h"
#include "sched/workgroup_internal.h"
#include "eu/qos_interface.h"
#include "eu/cpuworker_manager.h"
#ifdef FFRT_IO_TASK_SCHEDULER
#include "queue/queue.h"
#endif

namespace ffrt {
bool CPUWorkerManager::IncWorker(const QoS& qos)
{
    std::unique_lock<std::shared_mutex> lock(groupCtl[qos()].tgMutex);
    if (tearDown) {
        return false;
    }

    auto worker = std::unique_ptr<WorkerThread>(new (std::nothrow) CPUWorker(qos, {
        std::bind(&CPUWorkerManager::PickUpTask, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::NotifyTaskPicked, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerIdleAction, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerRetired, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerPrepare, this, std::placeholders::_1),
#ifdef FFRT_IO_TASK_SCHEDULER
        std::bind(&CPUWorkerManager::TryPoll, this, std::placeholders::_1, std::placeholders::_2),
        std::bind(&CPUWorkerManager::StealTaskBatch, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::PickUpTaskBatch, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::TryMoveLocal2Global, this, std::placeholders::_1),
#endif
    }));
    if (worker == nullptr) {
        FFRT_LOGE("Inc CPUWorker: create worker\n");
        return false;
    }
    worker->WorkerSetup(worker.get());
    WorkerJoinTg(qos, worker->Id());
    groupCtl[qos()].threads[worker.get()] = std::move(worker);
    return true;
}

void CPUWorkerManager::WakeupWorkers(const QoS& qos)
{
    if (tearDown) {
        return;
    }

    auto& ctl = sleepCtl[qos()];
    ctl.cv.notify_one();
}

int CPUWorkerManager::GetTaskCount(const QoS& qos)
{
    auto& sched = FFRTScheduler::Instance()->GetScheduler(qos);
    return sched.RQSize();
}

CPUEUTask* CPUWorkerManager::PickUpTask(WorkerThread* thread)
{
    if (tearDown) {
        return nullptr;
    }

    auto& sched = FFRTScheduler::Instance()->GetScheduler(thread->GetQos());
    auto lock = GetSleepCtl(static_cast<int>(thread->GetQos()));
    std::lock_guard lg(*lock);
    return sched.PickNextTask();
}

#ifdef FFRT_IO_TASK_SCHEDULER
CPUEUTask* CPUWorkerManager::PickUpTaskBatch(WorkerThread* thread)
{
    if (tearDown) {
        return nullptr;
    }

    auto& sched = FFRTScheduler::Instance()->GetScheduler(thread->GetQos());
    auto lock = GetSleepCtl(static_cast<int>(thread->GetQos()));
    std::lock_guard lg(*lock);
    CPUEUTask* task = sched.PickNextTask();
    if (task == nullptr) return nullptr;
    struct queue_s *queue = &(((CPUWorker *)(thread))->local_fifo);
    int expected_task = (GetTaskCount(thread->GetQos()) / monitor->WakedWorkerNum(thread->GetQos()));
    for (int i = 0; i < expected_task; i++) {
        if (queue_length(queue) == queue_capacity(queue)) {
            return task;
        }
        CPUEUTask* task2local = sched.PickNextTask();
        if (task2local == nullptr) {
            return task;
        }
        if (((CPUWorker *)thread)->priority_task == nullptr) {
            ((CPUWorker *)thread)->priority_task = task2local;
        } else {
            int ret = queue_pushtail(queue, task2local);
            if (ret != 0) {
                return task;
            }
        }
    }
    return task;
}

bool InsertTask(void* task, int qos)
{
    ffrt_executor_task_t* task_ = (ffrt_executor_task_t *)task;
    LinkedList* node = (LinkedList *)(&task_->wq);
    return FFRTScheduler::Instance()->InsertNode(node, ffrt::QoS(qos));
}

void CPUWorkerManager::TryMoveLocal2Global(WorkerThread* thread)
{
    if (tearDown) {
        return;
    }
    struct queue_s *queue = &(((CPUWorker *)(thread))->local_fifo);
    if (queue_length(queue) == queue_capacity(queue)) {
        queue_pophead_to_gqueue_batch(queue, queue_length(queue) / 2, thread->GetQos(), InsertTask);
    }
}

unsigned int CPUWorkerManager::StealTaskBatch(WorkerThread* thread)
{
    if (tearDown) {
        return 0;
    }
    static_assert(STEAL_BUFFER_SIZE == LOCAL_QUEUE_SIZE / 2);
    if (GetStealingWorkers(thread->GetQos()) > groupCtl[thread->GetQos()].threads.size() / 2) {
        return 0;
    }
    std::shared_lock<std::shared_mutex> lck(groupCtl[thread->GetQos()].tgMutex);
    AddStealingWorker(thread->GetQos());
    std::unordered_map<WorkerThread*, std::unique_ptr<WorkerThread>>::iterator iter =
        groupCtl[thread->GetQos()].threads.begin();
    while (iter != groupCtl[thread->GetQos()].threads.end()) {
        struct queue_s *queue = &(((CPUWorker *)(iter->first))->local_fifo);
        unsigned int queue_len = queue_length(queue);
        if (iter->first != thread && queue_len > 1) {
            unsigned int buf_len = queue_pophead_pushtail_batch(queue, &(((CPUWorker *)thread)->local_fifo),
                queue_len / 2);
            SubStealingWorker(thread->GetQos());
            return buf_len;
        }
        iter++;
    }
    SubStealingWorker(thread->GetQos());
    return 0;
}

PollerRet CPUWorkerManager::TryPoll(const WorkerThread* thread, int timeout)
{
    if (tearDown) {
        return PollerRet::RET_NULL;
    }

    auto& pollerMtx = pollersMtx[thread->GetQos()];
    if (pollerMtx.try_lock()) {
        if (timeout == -1) {
            monitor->IntoPollWait(thread->GetQos());
        }
        PollerRet ret = PollerProxy::Instance()->GetPoller(thread->GetQos()).PollOnce(timeout);
        if (timeout == -1) {
            monitor->OutOfPollWait(thread->GetQos());
        }
        pollerMtx.unlock();
        return ret;
    }
    return PollerRet::RET_NULL;
}

void CPUWorkerManager::NotifyLocalTaskAdded(const QoS& qos)
{
    if (stealWorkers[qos()].load(std::memory_order_relaxed) == 0) {
        monitor->Notify(qos, TaskNotifyType::TASK_LOCAL);
    }
}
#endif

void CPUWorkerManager::NotifyTaskPicked(const WorkerThread* thread)
{
    monitor->Notify(thread->GetQos(), TaskNotifyType::TASK_PICKED);
}

void CPUWorkerManager::WorkerRetired(WorkerThread* thread)
{
    pid_t pid = thread->Id();
    int qos = static_cast<int>(thread->GetQos());

    {
        std::unique_lock<std::shared_mutex> lck(groupCtl[qos].tgMutex);
        thread->SetExited(true);
        thread->Detach();
        auto worker = std::move(groupCtl[qos].threads[thread]);
        size_t ret = groupCtl[qos].threads.erase(thread);
        if (ret != 1) {
            FFRT_LOGE("erase qos[%d] thread failed, %d elements removed", qos, ret);
        }
        WorkerLeaveTg(qos, pid);
        worker = nullptr;
    }
}

void CPUWorkerManager::NotifyTaskAdded(const QoS& qos)
{
    monitor->Notify(qos, TaskNotifyType::TASK_ADDED);
}

CPUWorkerManager::CPUWorkerManager()
{
    groupCtl[qos_deadline_request].tg = std::unique_ptr<ThreadGroup>(new ThreadGroup());
}

void CPUWorkerManager::WorkerJoinTg(const QoS& qos, pid_t pid)
{
    if (qos == qos_user_interactive) {
        (void)JoinWG(pid);
        return;
    }
    auto& tgwrap = groupCtl[qos()];
    if (!tgwrap.tg) {
        return;
    }

    if ((tgwrap.tgRefCount) == 0) {
        return;
    }

    tgwrap.tg->Join(pid);
}

void CPUWorkerManager::WorkerLeaveTg(const QoS& qos, pid_t pid)
{
    if (qos == qos_user_interactive) {
        return;
    }
    auto& tgwrap = groupCtl[qos()];
    if (!tgwrap.tg) {
        return;
    }

    if ((tgwrap.tgRefCount) == 0) {
        return;
    }

    tgwrap.tg->Leave(pid);
}
} // namespace ffrt

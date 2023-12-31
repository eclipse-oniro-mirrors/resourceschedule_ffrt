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

#ifndef CPU_MONITOR_H
#define CPU_MONITOR_H

#include <atomic>
#include <vector>
#include <functional>
#include <mutex>
#include "sched/qos.h"
#include "cpp/mutex.h"
#include "eu/cpu_manager_interface.h"

namespace ffrt {
struct WorkerCtrl {
    size_t hardLimit = 0;
    size_t maxConcurrency = 0;
    size_t workerManagerID = 0;
    int executionNum = 0;
    int sleepingWorkerNum = 0;
    std::mutex lock;
};

class CPUMonitor {
public:
    CPUMonitor(CpuMonitorOps&& ops);
    CPUMonitor(const CPUMonitor&) = delete;
    CPUMonitor& operator=(const CPUMonitor&) = delete;
    ~CPUMonitor();
    uint32_t GetMonitorTid() const;
    void HandleBlocked(const QoS& qos);
    void DecExeNumRef(const QoS& qos);
    void IncSleepingRef(const QoS& qos);
    void DecSleepingRef(const QoS& qos);
    void IntoSleep(const QoS& qos);
    void WakeupCount(const QoS& qos);
    void TimeoutCount(const QoS& qos);
    void RegWorker(const QoS& qos);
    void UnRegWorker();
    void Notify(const QoS& qos, TaskNotifyType notifyType);

    uint32_t monitorTid = 0;

private:
    size_t CountBlockedNum(const QoS& qos);
    void SetupMonitor();
    void StartMonitor();
    void Poke(const QoS& qos);

    std::thread* monitorThread;
    CpuMonitorOps ops;
    WorkerCtrl ctrlQueue[QoS::Max()];
};
}
#endif /* CPU_MONITOR_H */

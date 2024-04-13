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

#include "c/executor_task.h"
#include "internal_inc/osal.h"
#include "dfx/log/ffrt_log_api.h"
#include "util/ffrt_facade.h"

#ifdef FFRT_IO_TASK_SCHEDULER
API_ATTRIBUTE((visibility("default")))
int ffrt_epoll_ctl(ffrt_qos_t qos, int op, int fd, uint32_t events, void* data, ffrt_poller_cb cb)
{
    int ret = 0;
    ffrt::QoS pollerQos = ffrt::QoS(qos);
    if (op == EPOLL_CTL_ADD) {
        ret = ffrt::PollerProxy::Instance()->GetPoller(pollerQos).AddFdEvent(events, fd, data, cb);
        if (ret == 0) {
            ffrt::FFRTFacade::GetEUInstance().NotifyLocalTaskAdded(pollerQos);
        }
        return ret;
    } else if (op == EPOLL_CTL_DEL) {
        return ffrt::PollerProxy::Instance()->GetPoller(pollerQos).DelFdEvent(fd);
    } else if (op == EPOLL_CTL_MOD) {
        FFRT_LOGE("EPOLL_CTL_MOD not supported yet");
        return -1;
    } else {
        FFRT_LOGE("EPOLL_CTL op invalid");
        return -1;
    }
}

API_ATTRIBUTE((visibility("default")))
void ffrt_poller_wakeup(ffrt_qos_t qos)
{
    ffrt::PollerProxy::Instance()->GetPoller(ffrt::QoS(qos)).WakeUp();
}

API_ATTRIBUTE((visibility("default")))
uint8_t ffrt_epoll_get_count(ffrt_qos_t qos)
{
    return ffrt::PollerProxy::Instance()->GetPoller(ffrt::QoS(qos)).GetPollCount();
}
#endif
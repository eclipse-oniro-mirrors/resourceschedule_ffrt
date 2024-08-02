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

#include "rtg_perf_ctrl.h"
#include "dfx/log/ffrt_log_api.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cerrno>
#include <securec.h>

static int OpenPerfCtrl(void)
{
    static bool perfCtrlAvailable = true;
    int fd = -1;

    if (!perfCtrlAvailable) {
        return -1;
    }

    fd = open("/dev/hisi_perf_ctrl", O_RDWR);
    if (fd < 0) {
        FFRT_LOGW("open perf_ctrl failed");
        perfCtrlAvailable = false;
    }

    return fd;
}

void SetTaskRtg(pid_t tid, unsigned int grpId)
{
    struct rtg_group_task data = {tid, grpId, 0};
    int fd = OpenPerfCtrl();
    if (fd < 0) {
        return;
    }

    if (ioctl(fd, PERF_CTRL_SET_TASK_RTG, &data)) {
        FFRT_LOGW("Error set rtg %d,%u. %s", tid, grpId, strerror(errno));
        close(fd);
        return;
    }
    close(fd);
}

void SetRtgStatus(unsigned long long status)
{
    int fd = OpenPerfCtrl();
    if (fd < 0) {
        return;
    }

    if (ioctl(fd, PERF_CTRL_SET_FRAME_STATUS, &status)) {
        FFRT_LOGW("Error set rtg status=%llu. %s", status, strerror(errno));
        close(fd);
        return;
    }
    close(fd);
}

void SetRtgQos(int qos) // MHZ
{
    int fd = OpenPerfCtrl();
    if (fd < 0) {
        return;
    }

    if (ioctl(fd, PERF_CTRL_SET_FRAME_RATE, &qos)) {
        FFRT_LOGW("Error set rtg qos=%d. %s", qos, strerror(errno));
        close(fd);
        return;
    }
    close(fd);
}

void SetRtgLoadMode(unsigned int grpId, bool utilEnabled, bool freqEnabled)
{
    struct rtg_load_mode load_mode;

    memset_s(&load_mode, sizeof(struct rtg_load_mode), 0, sizeof(struct rtg_load_mode));
    load_mode.grpId = grpId;
    load_mode.utilEnabled = !!utilEnabled;
    load_mode.freqEnabled = !!freqEnabled;

    int fd = OpenPerfCtrl();
    if (fd < 0) {
        return;
    }

    if (ioctl(fd, PERF_CTRL_SET_RTG_LOAD_MODE, &load_mode)) {
        FFRT_LOGW("Error set rtg load_mode %d:%d/%d. %s", load_mode.grpId, load_mode.utilEnabled,
            load_mode.freqEnabled, strerror(errno));
        close(fd);
        return;
    }
    close(fd);
}

void set_task_min_util(pid_t tid, unsigned int util)
{
    struct task_config cfg = {tid, util};
    int fd = OpenPerfCtrl();
    if (fd < 0) {
        return;
    }

    if (ioctl(fd, PERF_CTRL_SET_TASK_MIN_UTIL, &cfg)) {
        FFRT_LOGW("Error set min util %d,%u. %s", tid, util, strerror(errno));
        close(fd);
        return;
    }
    close(fd);
}

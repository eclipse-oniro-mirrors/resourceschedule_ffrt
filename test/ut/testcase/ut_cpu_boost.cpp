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

#include <random>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <cstdbool>
#include <sys/ioctl.h>
#include <gtest/gtest.h>
#ifndef WITH_NO_MOCKER
#include <mockcpp/mockcpp.hpp>
#include "util/cpu_boost_wrapper.h"
#endif
#include "ffrt_inner.h"
#include "dfx/log/ffrt_log_api.h"
#include "c/ffrt_cpu_boost.h"
#include "../common.h"

using namespace std;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;

#define IOCTL_SET_CPU_BOOST_CONFIG	_IOWR('x', 51, struct CpuBoostCfg)
struct CpuBoostCfg {
    int pid;             // process id
    int id;              // code part context id (0-32), shouldn't be duplicate
    unsigned int size;   // this context using how much ddr size set 0x100000(1MB) as default
    unsigned int port;   // 0 as default I/D cache cpu boost work on the same time
    unsigned int offset; // how many ahead used by cpu boost prefecher
#ifndef __OHOS__
    bool ffrt;
#endif
};

int InitCfg(int ctxId)
{
    int fd;
 
    struct CpuBoostCfg cfg = {
        .pid = getpid(),
        .id = ctxId,
        .size = 1048576,
        .port = 0,
        .offset = 256,
#ifndef __OHOS__
        .ffrt = true,
#endif
    };

    printf("get para: pid-%d id-%d size-0x%x port-%u offset-%u.\n",
        cfg.pid, cfg.id, cfg.size, cfg.port, cfg.offset);

    fd = open("/dev/hisi_perf_ctrl", O_RDWR);
    if (fd < 0) {
        printf("open /dev/hisi_perf_ctrl failed.\n");
        return -1;
    }

    if (ioctl(fd, IOCTL_SET_CPU_BOOST_CONFIG, &cfg) == -1) {
        printf("Error %d (%s) in IOCTL_SET_CPU_BOOST_CONFIG\n", errno, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    printf("cpu boost cfg finished.\n");
    return 0;
}

class CpuBoostTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

HWTEST_F(CpuBoostTest, FFRTCpuBoostApiSuccess, TestSize.Level0)
{
    int i = 0;
    InitCfg(1);
    ffrt_cpu_boost_start(1);
    i++;
    ffrt_cpu_boost_end(1);
    EXPECT_EQ(i, 1);
}
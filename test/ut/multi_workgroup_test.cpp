/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include <thread>
#include <sched/workgroup_internal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "dfx/log/ffrt_log_api.h"
#include "common.h"

#define GET_TID() syscall(SYS_gettid)

using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;

class WorkgroupInternalTest : public testing::Test {
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

/**
 * @tc.name: JoinWGTest
 * @tc.desc: Test whether the JoinWG interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(WorkgroupInternalTest, JoinWGTest, TestSize.Level0)
{
    int tid = GET_TID();
    bool ret = JoinWG(tid, 0);
    EXPECT_TRUE(ret);
}

/**
 * @tc.name: WorkgroupCreateTest
 * @tc.desc: Test whether the WorkgroupCreate interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(WorkgroupInternalTest, WorkgroupCreateTest, TestSize.Level0)
{
    long interval = 10000;
    (void)setuid(1000);
    struct WorkGroup *ret = WorkgroupCreate(interval, 0);
    EXPECT_NE(ret, nullptr);
    WorkgroupClear(ret);

    (void)setuid(3039);
    struct WorkGroup *ret1 = WorkgroupCreate(interval, 0);
    EXPECT_NE(ret1, nullptr);
    WorkgroupClear(ret1);

    (void)setuid(0);
    struct WorkGroup *ret2 = WorkgroupCreate(interval, 0);
    EXPECT_NE(ret2, nullptr);
    WorkgroupClear(ret2);
}

/**
 * @tc.name: WorkgroupClearTest
 * @tc.desc: Test whether the WorkgroupClear interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(WorkgroupInternalTest, WorkgroupClearTest, TestSize.Level0)
{
    struct WorkGroup* wg = nullptr;

    int ret = WorkgroupClear(wg);
    EXPECT_EQ(0, ret);

    long interval = 10000;
    (void)setuid(1000);
    struct WorkGroup *wg2 = WorkgroupCreate(interval, 0);
    int ret2 = WorkgroupClear(wg2);
    EXPECT_EQ(0, ret2);

    (void)setuid(3039);
    struct WorkGroup *wg3 = WorkgroupCreate(interval, 0);
    int ret3 = WorkgroupClear(wg3);
    EXPECT_EQ(0, ret3);

    (void)setuid(0);
    struct WorkGroup *wg4 = WorkgroupCreate(interval, 0);
    int ret4 = WorkgroupClear(wg4);
    EXPECT_EQ(0, ret4);
}

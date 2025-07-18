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

#include <gtest/gtest.h>
#include <chrono>
#include "c/thread.h"
#include "ffrt_inner.h"
#include "../common.h"

using namespace ffrt;
using namespace std;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

class CVTest : public testing::Test {
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

HWTEST_F(CVTest, conditonV_wait_for_test2, TestSize.Level0)
{
    ffrt::condition_variable cond;
    ffrt::mutex lock_;
    ffrt::cv_status status;

    ffrt::submit(
        [&]() {
            std::unique_lock lck(lock_);
            status = cond.wait_for(lck, 100ms);
            EXPECT_EQ(status, ffrt::cv_status::timeout);
        },
        {}, {});

    ffrt::wait();
}

HWTEST_F(CVTest, conditonV_wait_for_test3, TestSize.Level0)
{
    ffrt::condition_variable cond;
    ffrt::mutex lock_;
    ffrt::cv_status status;

    std::unique_lock lck(lock_);
    status = cond.wait_for(lck, 100ms);
    EXPECT_EQ(status, ffrt::cv_status::timeout);
}

HWTEST_F(CVTest, conditonV_nullptr_test, TestSize.Level0)
{
    int ret = 0;

    ret = ffrt_cond_init(nullptr, nullptr);
    EXPECT_NE(ret, 0);
    ret = ffrt_cond_signal(nullptr);
    EXPECT_NE(ret, 0);
    ret = ffrt_cond_broadcast(nullptr);
    EXPECT_NE(ret, 0);
    ret = ffrt_cond_wait(nullptr, nullptr);
    EXPECT_NE(ret, 0);
    ret = ffrt_cond_timedwait(nullptr, nullptr, nullptr);
    EXPECT_NE(ret, 0);
    ffrt_cond_destroy(nullptr);
}

class MutexTest : public testing::Test {
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

HWTEST_F(MutexTest, try_lock_test, TestSize.Level0)
{
    int val = -1;
    ffrt::mutex lock;
    lock.lock();
    val = lock.try_lock();
    EXPECT_EQ(val, 0);
    lock.unlock();
    val = lock.try_lock();
    EXPECT_EQ(val, 1);
    lock.unlock();
    lock.unlock();
}

HWTEST_F(MutexTest, lock_stress_test, TestSize.Level0)
{
    // trigger lazy init
    ffrt::submit([&]() {}, {}, {});
    ffrt::wait();

    const int N = 10;
    const int M = 10;
    const int J = 10;
    ffrt::mutex lock;
    // std::mutex lock;
    int acc = 0;
    for (int i = 0; i < N; ++i) {
    ffrt::submit(
        [&]() {
        for (int j = 0; j < M; ++j) {
            lock.lock();
            acc++;
            lock.unlock();
        }
        },
        {}, {});
    }

    for (int j = 0; j < J; ++j) {
    lock.lock();
    acc++;
    lock.unlock();
    }

    ffrt::wait();
    EXPECT_EQ(acc, (M * N + J));
}

class SleepTest : public testing::Test {
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

void* thd_func(void *arg)
{
    int *counter = (int *)arg;
    (*counter)++;
    return arg;
}

int g_data = 0;

HWTEST_F(SleepTest, thread_test, TestSize.Level0)
{
    ffrt_thread_t detachThread;
    ffrt_thread_create(&detachThread, nullptr, thd_func, &g_data);
    ffrt_thread_detach(detachThread);

    int a = 0;
    ffrt_thread_t thread;
    ffrt_thread_create(&thread, nullptr, thd_func, &a);
    void* result = nullptr;
    ffrt_thread_join(thread, &result);
    EXPECT_EQ(1, a);
    EXPECT_EQ(&a, result);
}

HWTEST_F(SleepTest, thread_test2, TestSize.Level0)
{
    int a = 0;
    ffrt_thread_t thread;
    ffrt_thread_attr_t attr;
    ffrt_thread_create(nullptr, nullptr, thd_func, &a);
    EXPECT_EQ(0, a);
    EXPECT_EQ(ffrt_thread_create(&thread, nullptr, nullptr, &a), ffrt_error_inval);
    EXPECT_EQ(ffrt_thread_create(&thread, &attr, thd_func, &a), ffrt_error);

    void* result = nullptr;
    EXPECT_EQ(ffrt_thread_join(nullptr, &result), ffrt_error_inval);
    EXPECT_EQ(ffrt_thread_join(thread, nullptr), ffrt_error_inval);
    EXPECT_EQ(ffrt_thread_detach(nullptr), ffrt_error_inval);
}

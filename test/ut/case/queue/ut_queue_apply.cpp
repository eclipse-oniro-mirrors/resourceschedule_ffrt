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

#include <thread>
#include <chrono>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <gtest/gtest.h>
#include "ffrt_inner.h"
#include "c/queue_ext.h"
#include "../../common.h"
#include "queue/base_queue.h"
#include "sync/delayed_worker.h"
#define private public
#include "queue/queue_monitor.h"
#undef private
#include "util/spmc_queue.h"
#include "util/ffrt_facade.h"
#include "util/white_list.h"
#include "cpp/sleep.h"
#include "tm/task_base.h"
#include "queue_common.h"

using namespace std;
using namespace ffrt;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

class QueueApplyTest : public testing::Test {
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

/*
* 测试用例名称：ffrt_serial_queue_apply
* 测试用例描述：串行队列apply接口正常使用，重复执行同一任务
* 预置条件    ：无
* 操作步骤    ：1、设置任务与重复次数
               2、apply接口提交任务
* 预期结果    ：串行执行指定次数的任务并等待
*/
HWTEST_F(QueueApplyTest, ffrt_serial_queue_apply, TestSize.Level1)
{
    auto serialQueue = std::make_unique<ffrt::queue>("ffrt_normal_queue");
    int result = 0;
    int applyCount = 10;
    std::function<void()>&& basicFunc = [&result]() {
        OnePlusForTest(static_cast<void*>(&result));
        OnePlusForTest(static_cast<void*>(&result));
        usleep(3000);
    };

    serialQueue->submit_apply(applyCount, basicFunc);
    EXPECT_EQ(result, applyCount * 2);
    serialQueue->submit_apply(0, basicFunc);
    EXPECT_EQ(result, applyCount * 2);
}

/*
* 测试用例名称：ffrt_concurrent_queue_apply
* 测试用例描述：并发队列apply接口正常使用，重复执行同一任务
* 预置条件    ：无
* 操作步骤    ：1、设置任务与重复次数
               2、apply接口提交任务
* 预期结果    ：并发队列根据并发度执行指定次数的任务并等待
*/
HWTEST_F(QueueApplyTest, ffrt_concurrent_queue_apply, TestSize.Level1)
{
    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
        "concurrent_queue", ffrt::queue_attr().max_concurrency(8));
    auto testQueue1 = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
        "concurrent_queue", ffrt::queue_attr().max_concurrency(1));
    std::atomic<int> result = 0;
    int applyCount = 40;
    std::function<void()>&& basicFunc = [&result]() {
        result.fetch_add(1);
        result.fetch_add(1);
        usleep(3000);
    };

    testQueue->submit_apply(applyCount, basicFunc);
    EXPECT_EQ(result, applyCount * 2);
    testQueue1->submit_apply(applyCount, basicFunc);
    testQueue1->submit_apply(applyCount, std::move(basicFunc));
    testQueue1->submit_apply(applyCount, [&result]() {
        result.fetch_add(1);
    });

    EXPECT_EQ(result, applyCount * 7);
}

/*
* 测试用例名称：ffrt_serial_queue_apply_delay
* 测试用例描述：串行队列apply接口正常使用，延时只等待一次
* 预置条件    ：无
* 操作步骤    ：1、设置任务与重复次数
               2、apply接口提交任务
* 预期结果    ：串行执行apply接口延时只等待一次
*/
HWTEST_F(QueueApplyTest, ffrt_serial_queue_apply_delay, TestSize.Level1)
{
    auto serialQueue = std::make_unique<ffrt::queue>("ffrt_normal_queue");
    uint64_t delay = 500000;
    int result = 0;
    int applyCount = 10;
    std::function<void()>&& basicFunc = [&result]() {
        OnePlusForTest(static_cast<void*>(&result));
        OnePlusForTest(static_cast<void*>(&result));
    };
    uint64_t start = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()).time_since_epoch().count());
    serialQueue->submit_apply(applyCount, basicFunc, task_attr().delay(delay));
    uint64_t end = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()).time_since_epoch().count());
    EXPECT_EQ(result, applyCount * 2);
    EXPECT_LE(delay, end - start);
}

HWTEST_F(QueueApplyTest, queue_submit_perf_count, TestSize.Level0)
{
    ffrt::queue* testQueue = new ffrt::queue("test_queue");
    WarmUp(testQueue);
    uint64_t start = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()).time_since_epoch().count());
    std::string startime = FormatDateString4CntCt(start);
    FFRT_LOGI("perf_count_case_01 begin, %llu, %s\n", start, startime.c_str());
    int x = 0;
    for (int i = 0; i < 1000; ++i) {
        testQueue->submit([&x] { x += 10; });
    }
    uint64_t end = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()).time_since_epoch().count());
    auto handle = testQueue->submit_h([&] { FFRT_LOGI("perf_count_case_01 end, %llu, submit 1k cost %llu us\n",
        end, end - start); });
    testQueue->wait(handle);
    EXPECT_EQ(x, 10000);
    delete testQueue;
}

/* 测试用例名称：ffrt_queue_task_double_execute
 * 测试用例描述：测试已执行的队列任务不会再次被执行
 * 预置条件    ：无
 * 操作步骤    ：1、提交执行一个队列任务，功能是x+1
                2、同时在闭包中为当前任务增加引用计数防止释放
                3、等待任务执行完成
                4、再次直接调用任务的Execute函数
 * 预期结果    ：闭包只被执行一次
 */
HWTEST_F(QueueApplyTest, ffrt_queue_task_double_execute, TestSize.Level0)
{
    int x = 0;
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    QueueTask* task;
    std::function<void()> basicFunc = [&]() {
        task = reinterpret_cast<QueueTask*>(ExecuteCtx::Cur()->task);
        task->IncDeleteRef();
        x += 1;
    };

    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr);
    ffrt_task_attr_set_delay(&task_attr, 1000);
    ffrt_task_handle_t h =
        ffrt_queue_submit_h(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);

    ffrt_queue_wait(h);

    task->Execute();

    EXPECT_EQ(x, 1);

    task->DecDeleteRef();

    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

HWTEST_F(QueueApplyTest, ffrt_queue_dfx_test, TestSize.Level0)
{
    std::atomic_int cnt{0};
    std::atomic_int cnt_cb{0};

    ffrt::task_attr attr;
    attr.delay(1100 * 1000).timeout(1000 * 1000).timeout_callback([&] {
        cnt_cb.fetch_add(1);
    });
    ffrt::task_attr attr2;
    attr2.delay(600 * 1000).timeout(500 * 1000).timeout_callback([&] {
        cnt_cb.fetch_add(1);
    });
    queue* testQueue = new ffrt::queue("test");

    ffrt::task_handle h = testQueue->submit_h([&] {
        cnt.fetch_add(1);
    }, attr);
    testQueue->submit([&] {
        cnt.fetch_add(1);
        StallUs(1600 * 1000);
    }, attr2);
    testQueue->wait(h);
    usleep(100 * 1000);

    delete testQueue;
    EXPECT_GE(cnt_cb.load(), 1);
    EXPECT_EQ(cnt.load(), 2);
}
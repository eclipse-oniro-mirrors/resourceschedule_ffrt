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

class QueueMonitorTest : public testing::Test {
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
 * 测试用例名称 : ffrt_queue_monitor_schedule_timeout
 * 测试用例描述 : 串行队列QueueMonitor检测到任务调度超时
 * 操作步骤     : 1、创建队列
 *               2、提交多个任务占满worker，使得新串行任务等待调度
 * 预期结果    : 成功触发任务调度超时告警
 */
HWTEST_F(QueueMonitorTest, ffrt_queue_monitor_schedule_timeout, TestSize.Level0)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt_task_timeout_set_threshold(1000000);
    ffrt_set_cpu_worker_max_num(ffrt_qos_default, 4);
    ffrt::FFRTFacade::GetQueueMonitor().timeoutUs_ = 1000000;

    for (int i = 0; i < 9; i++) {
        ffrt::submit([]() {
            usleep(1100000);
        }, {}, {});
    }
    ffrt::queue* testQueue = new ffrt::queue("test_queue");

    auto t = testQueue->submit_h([&x] {
        FFRT_LOGE("task start"); x = x + 1;}, {});
    testQueue->wait(t);
    EXPECT_EQ(x, 1);
    ffrt::wait();
}

/*
 * 测试用例名称 : ffrt_queue_monitor_execute_timeout
 * 测试用例描述 : 串行队列QueueMonitor检测到任务执行超时
 * 操作步骤     : 1、创建队列
 *               2、提交执行时间长任务
 * 预期结果    : 成功触发任务执行超时告警
 */
HWTEST_F(QueueMonitorTest, ffrt_queue_monitor_execute_timeout, TestSize.Level0)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt_task_timeout_set_threshold(1000);
    ffrt::FFRTFacade::GetQueueMonitor().timeoutUs_ = 1000000;
    queue* testQueue = new queue("test_queue");
    auto t = testQueue->submit_h([&x] { x = x + 1; usleep(2000000); FFRT_LOGE("done");}, {});
    FFRT_LOGE("submitted");
    testQueue->wait(t);
    FFRTFacade::GetQueueMonitor().timeoutUs_ = 30000000;
    EXPECT_EQ(x, 1);
}

/*
 * 测试用例名称 : ffrt_queue_monitor_delay_timeout
 * 测试用例描述 : 串行队列QueueMonitor检测到任务执行超时
 * 操作步骤     : 1、创建队列
 *               2、提交执行时间长任务
 * 预期结果    : 不触发超时告警
 */
HWTEST_F(QueueMonitorTest, ffrt_queue_monitor_delay_timeout, TestSize.Level0)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt_task_timeout_set_threshold(1000);
    ffrt::FFRTFacade::GetQueueMonitor().timeoutUs_ = 1000000;
    queue* testQueue = new queue("test_queue");
    FFRT_LOGE("submit");
    auto t = testQueue->submit_h([&x] { FFRT_LOGE("delay end"); usleep(2100000);
        x = x + 1;}, task_attr().delay(1200000));
    testQueue->wait(t);
    FFRTFacade::GetQueueMonitor().timeoutUs_ = 30000000;
    EXPECT_EQ(x, 1);
}

/*
 * 测试用例名称 : ffrt_queue_monitor_cancel_timeout
 * 测试用例描述 : 串行队列QueueMonitor检测到任务执行超时
 * 操作步骤     : 1、创建队列
 *               2、提交执行时间长任务
 * 预期结果    : 不触发超时告警
 */
HWTEST_F(QueueMonitorTest, ffrt_queue_monitor_cancel_timeout, TestSize.Level0)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt_task_timeout_set_threshold(1000);
    ffrt::FFRTFacade::GetQueueMonitor().timeoutUs_ = 1000000;
    queue* testQueue = new queue("test_queue");
    FFRT_LOGE("submit");
    auto t1 = testQueue->submit_h([&x] { x = x + 1; FFRT_LOGE("start"); });
    auto t2 = testQueue->submit_h([&x] { x = x + 1; FFRT_LOGE("delay start"); }, task_attr().delay(5000000));
    testQueue->cancel(t2);
    testQueue->wait(t1);
    testQueue->wait(t2);
    usleep(1200000);
    FFRTFacade::GetQueueMonitor().timeoutUs_ = 30000000;
    EXPECT_EQ(x, 1);
}

/*
 * 测试用例名称 : ffrt_queue_monitor_two_stage_timeout
 * 测试用例描述 : 串行队列QueueMonitor检测到任务在PENDING和EXECUTING各超时一次
 * 操作步骤     : 1、创建队列
 *               2、提交执行时间长任务
 * 预期结果    : 触发PENDING和EXECUTING各一次的超时告警
 */
HWTEST_F(QueueMonitorTest, ffrt_queue_monitor_two_stage_timeout, TestSize.Level0)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt_task_timeout_set_threshold(1000);
    FFRTFacade::GetDependenceManager();
    FFRTFacade::GetQueueMonitor().timeoutUs_ = 1000000;

    ffrt_set_cpu_worker_max_num(ffrt_qos_default, 1);

    ffrt_queue_attr_t queue_attr;
    ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    ffrt::submit([] { StallUs(1300 * 1000); });
    std::function<void()>&& basicFunc = [&x] { x = x + 1; StallUs(1300 * 1000); FFRT_LOGE("done");};
    ffrt_task_handle_t task = ffrt_queue_submit_h(queue_handle,
        ffrt::create_function_wrapper(basicFunc, ffrt_function_kind_queue), nullptr);

    ffrt_queue_wait(task);
    ffrt_task_handle_destroy(task);
    ffrt_queue_destroy(queue_handle);
    ffrt_task_timeout_set_threshold(30000);
    EXPECT_EQ(x, 1);
}

HWTEST_F(QueueMonitorTest, ffrt_queue_submit_h_f, TestSize.Level0)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    int result = 0;
    ffrt_task_handle_t task = ffrt_queue_submit_h_f(queue_handle, OnePlusForTest, &result, nullptr);
    ffrt_queue_wait(task);
    ffrt_task_handle_destroy(task);
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);

    EXPECT_EQ(result, 1);
}

HWTEST_F(QueueMonitorTest, ffrt_queue_submit_f, TestSize.Level0)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    int result = 0;
    ffrt_queue_submit_f(queue_handle, OnePlusForTest, &result, nullptr);
    EXPECT_TRUE(queue_handle != nullptr);

    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);

    EXPECT_EQ(result, 1);
}

/*
 * 测试用例名称 : ffrt_queue_concurrent_recordtraffic_delay_trigger
 * 测试用例描述 : 设置并行队列的traffic_interval并生效
 * 操作步骤     : 1、创建队列
 *               2、调用ffrt_queue_attr_set_traffic_interval接口设置并行队列的流量监控窗口
 *               3、提交堆积任务
 * 预期结果    : 成功触发流量监控告警，但不触发上报
 */
HWTEST_F(QueueMonitorTest, ffrt_queue_concurrent_recordtraffic_delay_trigger, TestSize.Level1)
{
    ffrt_queue_attr_t queue_attr;
    ffrt_task_handle_t handle;
    ffrt_task_attr_t task_attr;
    ffrt_task_attr_t task_attr_1;
    std::atomic<int> result = 0;
    uint64_t concurrency = 4;
    (void)ffrt_task_attr_init(&task_attr_1);
    (void)ffrt_task_attr_init(&task_attr);
    ffrt_task_attr_set_delay(&task_attr, 1200000);
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_attr_set_max_concurrency(&queue_attr, concurrency);

    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_concurrent, "test_queue", &queue_attr);
    ffrt::QueueHandler* queueHandler = reinterpret_cast<ffrt::QueueHandler*>(queue_handle);
    queueHandler->trafficRecordInterval_ = 1000000;
    queueHandler->trafficRecord_.nextUpdateTime_ = TimeStampCntvct() + 1000000;
    std::function<void()>&& testFunc = [&result]() {
        result = result + 1;
        usleep(1100000);
    };
    std::function<void()>&& fastFunc = [&result]() {
        result = result + 1;
    };

    ffrt_queue_submit(queue_handle, create_function_wrapper(testFunc, ffrt_function_kind_queue), &task_attr_1);
    for (int i = 0; i < 30; i++) {
        ffrt_queue_submit(queue_handle, create_function_wrapper(fastFunc, ffrt_function_kind_queue), &task_attr);
    }
    usleep(1000000);
    handle = ffrt_queue_submit_h(queue_handle,
                create_function_wrapper(fastFunc, ffrt_function_kind_queue), &task_attr_1);
    ffrt_queue_wait(handle);
    ffrt_task_handle_destroy(handle);
    EXPECT_EQ(result, 2);
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称 : ffrt_queue_concurrent_monitor_schedule_timeout_all
 * 测试用例描述 : 并行队列QueueMonitor检测到任务调度超时
 * 操作步骤     : 1、创建队列
 *               2、提交多个任务占满全部worker，使得新并行任务等待调度
 * 预期结果    : 成功触发任务调度超时告警
 */
HWTEST_F(QueueMonitorTest, ffrt_queue_monitor_concurrent_schedule_timeout_all, TestSize.Level1)
{
    ffrt_task_timeout_set_cb(MyCallback);
    FFRTFacade::GetDependenceManager();
    FFRTFacade::GetQueueMonitor().timeoutUs_ = 1000000;
    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
    "concurrent_queue", ffrt::queue_attr().max_concurrency(4));
    ffrt_set_cpu_worker_max_num(ffrt_qos_default, 2);
    std::atomic<std::uint64_t> y{0};
    std::array<ffrt::task_handle, 10> handles;

    for (uint32_t i = 0; i < 5; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                usleep(2000000);
                y.fetch_add(1);
            }, ffrt::task_attr("Slow_Task")
        );
    }

    for (uint32_t i = 5; i < 10; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
                usleep(1000000);
            }, ffrt::task_attr("Normal_Task")
        );
    }

    for (uint32_t i = 0; i < 10; i++) {
        testQueue->wait(handles[i]);
    }
    EXPECT_EQ(y, 10);
}

/*
 * 测试用例名称 : ffrt_queue_concurrent_monitor_schedule_timeout_part
 * 测试用例描述 : 并行队列QueueMonitor检测到任务调度超时
 * 操作步骤     : 1、创建队列
 *               2、提交多个任务占满部分worker，使得新并行任务等待调度
 * 预期结果    : 成功触发任务调度超时告警
 */
HWTEST_F(QueueMonitorTest, ffrt_queue_monitor_concurrent_schedule_timeout_part, TestSize.Level1)
{
    ffrt_task_timeout_set_cb(MyCallback);
    FFRTFacade::GetDependenceManager();
    FFRTFacade::GetQueueMonitor().timeoutUs_ = 1000000;

    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
    "concurrent_queue", ffrt::queue_attr().max_concurrency(4));

    ffrt_set_cpu_worker_max_num(ffrt_qos_default, 2);
    std::atomic<std::uint64_t> y{0};
    std::array<ffrt::task_handle, 4> handles;

    for (uint32_t i = 0; i < 1; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
                usleep(4000000);
            }, ffrt::task_attr("Slow_Task")
        );
    }

    for (uint32_t i = 1; i < 4; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
                usleep(1000000);
            }, ffrt::task_attr("Normal_Task")
        );
    }

    for (uint32_t i = 0; i < 4; i++) {
        testQueue->wait(handles[i]);
    }

    EXPECT_EQ(y, 4);
}

/*
 * 测试用例名称 : ffrt_queue_concurrent_monitor_schedule_timeout_delay
 * 测试用例描述 : 并行队列QueueMonitor检测到任务调度超时
 * 操作步骤     : 1、创建队列
 *               2、提交多个延迟任务，使得新并行任务等待调度
 * 预期结果    : 不触发任务调度超时告警
 */
HWTEST_F(QueueMonitorTest, ffrt_queue_monitor_concurrent_schedule_timeout_delay, TestSize.Level1)
{
    ffrt_task_timeout_set_cb(MyCallback);
    FFRTFacade::GetDependenceManager();
    FFRTFacade::GetQueueMonitor().timeoutUs_ = 1000000;

    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
    "concurrent_queue", ffrt::queue_attr().max_concurrency(4));
    ffrt_set_cpu_worker_max_num(ffrt_qos_default, 3);
    std::atomic<std::uint64_t> y{0};
    std::array<ffrt::task_handle, 10> handles;

    for (uint32_t i = 0; i < 2; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
            }, ffrt::task_attr("Delayed_Task").delay(1100000)
        );
    }

    for (uint32_t i = 2; i < 10; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
            }, ffrt::task_attr("Normal_Task")
        );
    }

    for (uint32_t i = 0; i < 10; i++) {
        testQueue->wait(handles[i]);
    }

    EXPECT_EQ(y, 10);
}

/*
 * 测试用例名称 : ffrt_queue_concurrent_monitor_execute_timeout_all
 * 测试用例描述 : 并行队列QueueMonitor检测到任务执行超时
 * 操作步骤     : 1、创建队列
 *               2、提交执行时间长任务占满全部worker
 * 预期结果    : 成功触发任务执行超时告警
 */
HWTEST_F(QueueMonitorTest, ffrt_queue_monitor_concurrent_execute_timeout_all, TestSize.Level1)
{
    ffrt_task_timeout_set_cb(MyCallback);
    FFRTFacade::GetDependenceManager();
    FFRTFacade::GetQueueMonitor().timeoutUs_ = 1000000;

    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
    "concurrent_queue", ffrt::queue_attr().max_concurrency(4));
    std::atomic<std::uint64_t> y{0};
    std::array<ffrt::task_handle, 20> handles;

    for (uint32_t i = 0; i < 4; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
            }, ffrt::task_attr("Normal_Task")
        );
    }

    for (uint32_t i = 4; i < 15; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
                usleep(1100000);
            }, ffrt::task_attr("Slow_Task")
        );
    }

    for (uint32_t i = 15; i < 20; i++) {
        ffrt::submit([&]() {
            y.fetch_add(1);
            }, ffrt::task_attr("Normal_Task")
        );
    }
    for (uint32_t i = 4; i < 15; i++) {
        testQueue->wait(handles[i]);
    }
    EXPECT_EQ(y, 20);
}
/*
 * 测试用例名称 : ffrt_queue_concurrent_monitor_cancel_timeout_all
 * 测试用例描述 : 并行队列QueueMonitor检测到任务超时
 * 操作步骤     : 1、创建队列
 *               2、先提交多个延迟任务占满全部worker，再执行前取消
 * 预期结果    : 不触发任务调度超时告警
 */
HWTEST_F(QueueMonitorTest, ffrt_queue_monitor_concurrent_cancel_timeout_all, TestSize.Level1)
{
    ffrt_task_timeout_set_cb(MyCallback);
    FFRTFacade::GetDependenceManager();
    FFRTFacade::GetQueueMonitor().timeoutUs_ = 1000000;

    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
    "concurrent_queue", ffrt::queue_attr().max_concurrency(4));
    ffrt_set_cpu_worker_max_num(ffrt_qos_default, 2);
    std::atomic<std::uint64_t> y{0};
    std::array<ffrt::task_handle, 10> handles;

    for (uint32_t i = 0; i < 2; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
            }, ffrt::task_attr("Delayed_Task").delay(3000000)
        );
    }
    testQueue->cancel(handles[0]);
    testQueue->cancel(handles[1]);

    for (uint32_t i = 2; i < 10; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
            }, ffrt::task_attr("Normal_Task")
        );
    }

    for (uint32_t i = 2; i < 10; i++) {
        testQueue->wait(handles[i]);
    }

    EXPECT_EQ(y, 8);
}

/*
 * 测试用例名称 : ffrt_queue_concurrent_monitor_mixed_conditions_timeout
 * 测试用例描述 : 并行队列QueueMonitor检测到任务超时
 * 操作步骤     : 1、创建队列
 *               2、先提交多个延迟任务占满全部worker，再执行前取消
 * 预期结果    : 不触发任务超时告警
 */
HWTEST_F(QueueMonitorTest, ffrt_queue_monitor_concurrent_mixed_conditions_timeout, TestSize.Level1)
{
    ffrt_task_timeout_set_cb(MyCallback);
    FFRTFacade::GetDependenceManager();
    FFRTFacade::GetQueueMonitor().timeoutUs_ = 1000000;

    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
    "concurrent_queue", ffrt::queue_attr().max_concurrency(4));
    std::atomic<std::uint64_t> y{0};
    std::array<ffrt::task_handle, 12> handles;

    for (uint32_t i = 0; i < 2; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
            }, ffrt::task_attr("Delayed_Task").delay(3000000)
        );
    }
    testQueue->cancel(handles[0]);

    for (uint32_t i = 2; i < 7; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
                usleep(2000000);
            }, ffrt::task_attr("Slow_Task")
        );
    }

    for (uint32_t i = 7; i < 12; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
                usleep(1000000);
            }, ffrt::task_attr("Normal_Task")
        );
    }

    for (uint32_t i = 1; i < 12; i++) {
        testQueue->wait(handles[i]);
    }

    EXPECT_EQ(y, 11);

    FFRTFacade::GetQueueMonitor().timeoutUs_ = 30000000;
}

HWTEST_F(QueueMonitorTest, submit_task_while_concurrency_queue_waiting_all_test, TestSize.Level1)
{
    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
        "concurrent_queue", ffrt::queue_attr().max_concurrency(4));
    bool notify = false;
    int waitingTaskCount = 0;
    std::atomic<int> submitThreadTaskCount = 0;
    std::mutex lock;
    std::condition_variable cv;
    for (int i = 0; i < 16; i++) {
        testQueue->submit([&] {
            std::unique_lock lk(lock);
            cv.wait(lk, [&] { return notify; });
            waitingTaskCount++;
            EXPECT_EQ(submitThreadTaskCount.load(), 0);
        });
    }

    std::mutex threadMutex;
    std::thread waitingThread([&] {
        std::unique_lock tm(threadMutex);
        EXPECT_EQ(ffrt_concurrent_queue_wait_all(*reinterpret_cast<ffrt_queue_t*>(testQueue.get())), 0);
    });

    std::thread submitThread([&] {
        while (threadMutex.try_lock()) {
            threadMutex.unlock();
            std::this_thread::yield();
        }
        usleep(100 * 1000);

        EXPECT_EQ(ffrt_concurrent_queue_wait_all(*reinterpret_cast<ffrt_queue_t*>(testQueue.get())), 1);
        for (int i = 0; i < 16; i++) {
            testQueue->submit([&] {
                submitThreadTaskCount.fetch_add(1);
            });
        }
    });

    submitThread.join();
    EXPECT_EQ(submitThreadTaskCount.load(), 0);

    {
        std::unique_lock lk(lock);
        notify = true;
        cv.notify_all();
    }
    waitingThread.join();
    EXPECT_EQ(waitingTaskCount, 16);

    EXPECT_EQ(ffrt_concurrent_queue_wait_all(*reinterpret_cast<ffrt_queue_t*>(testQueue.get())), 0);
    EXPECT_EQ(submitThreadTaskCount.load(), 16);
}
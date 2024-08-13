#include <thread>
#include <chrono>
#include <gtest/gtest.h>
#include "ffrt_inner.h"
#include "c/loop.h"
#define private public
#include "sync/poller"
#undef private
#include "util.h"

using namespace std;
using namespace ffrt;
using namespace testing;

class PollerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

static void testfun(void* data)
{
    int* testData = static_cast<int*>(data);
    *testData += 1;
    printf("%d, timeout callback\n", *testData);
}
static void (*cb)(void*) = testfun;

 TEST_F(PollerTest, unregister_timer_001)
 {
    Poller poller;
    // 1、组装timeMap_
    static int result0 = 0;
    int* xf = &result0;
    void* data = xf;
    uint64_t timeout = 1;
    uint64_r sleepTime = 2500;

    // 2、创建两个线程，并发PollerOnce和Unregister
    int para = 1;
    for (int i = 0; i < 50; i++)
    {
        int timer_handle = poller.RegisterTimer(timeout, data, cb, true);
        EXPECT_FALSE(poller.timerMap_.empty());
        auto bound_pollonce = std::bind(&Poller::PollOnce, &poller, para);
        auto bound_unregister = std::bind(&Poller::UnregisterTimer, &poller, timer_handle);
        usleep(sleepTime);
        std::thread thread1(bound_pollonce);
        std::thread thread2(bound_unregister);
        thread1.join();
        thread2.join();
        EXPECT_TRUE(poller.timerMap_.empty());
        EXPECT_TRUE(poller.executedHandle_.empty());
        poller.timerMap_.clear();
        poller.executedHandle_.clear();
    }
 }
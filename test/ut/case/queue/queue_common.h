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

#ifndef FFRT_TEST_UT_CASE_QUEUE_QUEUE_COMMON_H
#define FFRT_TEST_UT_CASE_QUEUE_QUEUE_COMMON_H

#include <cstdio>
#include <cstdint>
#include <chrono>

#include "ffrt_inner.h"
#include "c/queue_ext.h"

#include "../../common.h"

namespace {
void OnePlusForTest(void* data)
{
    *(int*)data += 1;
}

void PrintForTest(void* data)
{
    printf("run no input func PrintForTest\n");
}

int fibonacci(int n)
{
    if (n == 0 || n == 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

void FibonacciTest(void* data, int fibnum)
{
    int testnum = fibonacci(fibnum);
    *(int*)data += testnum;
}

void EmptyFunction() {}

void MyCallback(uint64_t id, const char* message, uint32_t length)
{
    FFRT_LOGE("call ffrt_queue_monitor timeout_callback");
}

void StallUs(size_t us)
{
    auto start = std::chrono::system_clock::now();
    size_t passed = 0;
    while (passed < us) {
        passed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now() - start).count();
    }
}

void WarmUp(ffrt::queue* testQueue)
{
    int x = 0;
    for (int i = 0; i < 9; ++i) {
        testQueue->submit([&x] { x += 10; });
    }
    ffrt::task_handle h = testQueue->submit_h([&x] { x += 10; });
    testQueue->wait(h);
}
} // namespace

#endif // FFRT_TEST_UT_CASE_QUEUE_QUEUE_COMMON_H
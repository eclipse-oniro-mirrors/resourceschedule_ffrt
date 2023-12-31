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
#include <thread>
#include "core/task_ctx.h"
#include "core/dependence_manager.h"
#include "sched/qos.h"
#include "ffrt.h"
#include "internal_inc/types.h"

using namespace testing;
using namespace testing::ext;
using namespace ffrt;
using namespace std;

class TaskCtxTest : public testing::Test {
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

/**
 * @tc.name: ChargeQoSSubmit
 * @tc.desc: Test whether the ChargeQoSSubmit interface are normal.
 * @tc.type: FUNC
 */

HWTEST_F(TaskCtxTest, ChargeQoSSubmit, TestSize.Level1)
{
    auto func = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    TaskCtx *task = new TaskCtx(nullptr, nullptr, 0, nullptr, QoS());
    task->ChargeQoSSubmit(qos_inherit);
    EXPECT_EQ(task->qos, qos_default);
    delete task;

    auto func1 = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    TaskCtx *task1 = new TaskCtx(nullptr, nullptr, 0, nullptr, QoS(qos_user_interactive));
    auto func2 = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    TaskCtx *task2 = new TaskCtx(nullptr, task1, 0, nullptr, QoS());
    task2->ChargeQoSSubmit(qos_inherit);
    EXPECT_EQ(task2->qos, qos_user_interactive);
    delete task1;
    delete task2;

    auto func3 = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    TaskCtx *task3 = new TaskCtx(nullptr, nullptr, 0, nullptr, QoS());
    task3->ChargeQoSSubmit(qos_user_interactive);
    EXPECT_EQ(task3->qos, qos_user_interactive);
    delete task3;
}

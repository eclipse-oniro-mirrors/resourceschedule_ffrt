/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "qos_config.h"

namespace ffrt {

constexpr uint8_t OS_SCHED_LIT_AFFINITY = 0x0F;
constexpr uint8_t OS_SCHED_MID_AFFINITY = 0x70;
constexpr uint8_t OS_SCHED_BIG_AFFINITY = 0x80;
constexpr uint8_t OS_SCHED_ALL_AFFINITY = 0xFF;
constexpr uint8_t OS_SCHED_LIT_MID_AFFINITY = 0x7F;
constexpr uint8_t OS_SCHED_MID_BIG_AFFINITY = 0xF0;

constexpr uint8_t OS_SCHED_RT_SUBMAX_PRIO = 89;
constexpr uint8_t OS_SCHED_RT_MAX_PRIO = 99;
constexpr uint8_t OS_SCHED_NORMAL_MIN_PRIO = 100;
constexpr uint8_t OS_SCHED_NORMAL_MID_PRIO = 110;
constexpr uint8_t OS_SCHED_NORMAL_PRIO = 119;
constexpr uint8_t OS_SCHED_NORMAL_INIT_PRIO = 120;
constexpr uint8_t OS_SCHED_NORMAL_SUBMAX_PRIO = 130;
constexpr uint8_t OS_SCHED_NORMAL_MAX_PRIO = 139;

QosConfig::QosConfig()
{
    setPolicyDefault();
    setPolicyForeground();
    setPolicyBackground();
    setPolicySystem();
}

void QosConfig::setPolicyDefault()
{
    g_defaultQosPolicy = {
        .policyType = QOS_POLICY_DEFAULT,
        .policyFlag = QOS_FLAG_ALL,
        .policys = {
            {19, 0, 1024, OS_SCHED_ALL_AFFINITY, OS_SCHED_NORMAL_MAX_PRIO, 0, 0},
            {19, 0, 1024, OS_SCHED_ALL_AFFINITY, OS_SCHED_NORMAL_SUBMAX_PRIO, 0, 0},
            {19, 0, 1024, OS_SCHED_ALL_AFFINITY, OS_SCHED_NORMAL_INIT_PRIO, 0, 0},
            {19, 0, 1024, OS_SCHED_ALL_AFFINITY, OS_SCHED_NORMAL_MID_PRIO, 0, 0},
            {19, 0, 1024, OS_SCHED_ALL_AFFINITY, OS_SCHED_RT_MAX_PRIO, 0, 0},
            {19, 0, 1024, OS_SCHED_ALL_AFFINITY, OS_SCHED_RT_SUBMAX_PRIO, 0, 0},
            {19, 0, 1024, OS_SCHED_ALL_AFFINITY, OS_SCHED_NORMAL_INIT_PRIO, 0, 0},
        }
    };
}

void QosConfig::setPolicyForeground()
{
    g_foregroundQosPolicy = {
        .policyType = QOS_POLICY_FRONT,
        .policyFlag = QOS_FLAG_ALL,
        .policys = {
            {19, 0, 200, OS_SCHED_LIT_MID_AFFINITY, OS_SCHED_NORMAL_MAX_PRIO, 0, 0},
            {10, 0, 250, OS_SCHED_LIT_MID_AFFINITY, OS_SCHED_NORMAL_SUBMAX_PRIO, 0, 0},
            {0, 0, 1024, OS_SCHED_ALL_AFFINITY, OS_SCHED_NORMAL_INIT_PRIO, 1, 0},
            {-10, 300, 1024, OS_SCHED_ALL_AFFINITY, OS_SCHED_NORMAL_MID_PRIO, 1, 1},
            {-20, 450, 1024, OS_SCHED_ALL_AFFINITY, OS_SCHED_RT_MAX_PRIO, 1, 1},
            {-20, 450, 1024, OS_SCHED_MID_BIG_AFFINITY, OS_SCHED_RT_SUBMAX_PRIO, 1, 1},
            {0, 0, 1024, OS_SCHED_ALL_AFFINITY, OS_SCHED_NORMAL_INIT_PRIO, 1, 0},
        }
    };
}

void QosConfig::setPolicyBackground()
{
    g_backgroundQosPolicy = {
        .policyType = QOS_POLICY_BACK,
        .policyFlag = QOS_FLAG_ALL & ~QOS_FLAG_RT,
        .policys = {
            {19, 0, 150, OS_SCHED_LIT_MID_AFFINITY, OS_SCHED_NORMAL_MAX_PRIO, 0, 0},
            {19, 0, 200, OS_SCHED_LIT_MID_AFFINITY, OS_SCHED_NORMAL_SUBMAX_PRIO, 0, 0},
            {19, 0, 250, OS_SCHED_ALL_AFFINITY, OS_SCHED_NORMAL_INIT_PRIO, 0, 0},
            {19, 0, 300, OS_SCHED_ALL_AFFINITY, OS_SCHED_NORMAL_MID_PRIO, 0, 0},
            {19, 0, 350, OS_SCHED_ALL_AFFINITY, OS_SCHED_RT_MAX_PRIO, 0, 0},
            {19, 0, 350, OS_SCHED_MID_BIG_AFFINITY, OS_SCHED_RT_SUBMAX_PRIO, 0, 0},
            {19, 0, 250, OS_SCHED_ALL_AFFINITY, OS_SCHED_NORMAL_INIT_PRIO, 0, 0},
        }
    };
}

void QosConfig::setPolicySystem()
{
    g_systemServerQosPolicy = {
        .policyType = QOS_POLICY_SYSTEM_SERVER,
        .policyFlag = QOS_FLAG_ALL,
        .policys = {
            {-10, 0, 1024, OS_SCHED_MID_BIG_AFFINITY, OS_SCHED_NORMAL_PRIO, 1, 1},
            {10, 0, 250, OS_SCHED_ALL_AFFINITY, OS_SCHED_RT_MAX_PRIO, 0, 0},
            {0, 0, 1024, OS_SCHED_ALL_AFFINITY, OS_SCHED_NORMAL_INIT_PRIO, 1, 0},
            {-10, 0, 1024, OS_SCHED_ALL_AFFINITY, OS_SCHED_RT_MAX_PRIO, 1, 1},
            {-20, 0, 1024, OS_SCHED_MID_BIG_AFFINITY, OS_SCHED_RT_MAX_PRIO, 1, 1},
            {-20, 0, 1024, OS_SCHED_MID_BIG_AFFINITY, OS_SCHED_RT_MAX_PRIO, 1, 1},
            {0, 0, 1024, OS_SCHED_ALL_AFFINITY, OS_SCHED_NORMAL_INIT_PRIO, 1, 0},
        }
    };
}
}
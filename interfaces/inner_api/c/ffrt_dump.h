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
#ifndef FFRT_API_C_FFRT_DUMP_H
#define FFRT_API_C_FFRT_DUMP_H
#include <stdint.h>
#include "type_def_ext.h"

typedef enum {
    DUMP_INFO_ALL = 0,
    DUMP_TASK_STATISTIC_INFO
} ffrt_dump_cmd_t;

typedef void(*ffrt_task_timeout_cb)(uint64_t gid, const char *msg, uint32_t size);

/**
 * @brief dump ffrt信息，包括task、queue、worker相关信息.
 *
 * @param cmd 命令类型：
 *            DUMP_INFO_ALL:表示输出所有信息
 *            DUMP_TASK_STATISTIC_INFO:输出任务统计信息
 * @param buf 指向需要写入的buffer
 * @param len buffer大小
 * @return 写入buffer的字符数，不包含字符串的结尾符号\0
 *         负数：表示操作未成功
 *         0 ： 未写入buffer
 *         正数：成功写入buffer的字符数
 * @约束：
 *  1.buffer大小不足，ffrt记录信息不全(buffer用完则不再写入)
 * @规格：
 *  1.调用时机：业务、交互命令任意时机均可以调用
 *  2.影响：该功能执行过程中、执行过后均不影响系统、业务功能
 */
FFRT_C_API int ffrt_dump(ffrt_dump_cmd_t cmd, char *buf, uint32_t len);
FFRT_C_API ffrt_task_timeout_cb ffrt_task_timeout_get_cb(void);
FFRT_C_API void ffrt_task_timeout_set_cb(ffrt_task_timeout_cb cb);
FFRT_C_API uint32_t ffrt_task_timeout_get_threshold(void);
FFRT_C_API void ffrt_task_timeout_set_threshold(uint32_t threshold_ms);
#endif /* FFRT_API_C_FFRT_DUMP_H */
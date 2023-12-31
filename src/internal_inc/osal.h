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

#ifndef __OSAL_HPP__
#define __OSAL_HPP__

#include <string>

#ifdef _MSC_VER
#define API_ATTRIBUTE(attr)
#define unlikely(x) (x)
#define likely(x)  (x)
#else
#define API_ATTRIBUTE(attr) __attribute__(attr)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#define likely(x)       __builtin_expect(!!(x), 1)
#endif

#ifdef __linux__
#include <sys/syscall.h>
#include <unistd.h>
static inline unsigned int GetPid(void)
{
    return getpid();
}
static inline unsigned int GetTid(void)
{
    return syscall(SYS_gettid);
}
static inline std::string GetEnv(const char* name)
{
    char* val = std::getenv(name);
    if (val == nullptr) {
        return "";
    }
    return val;
}

#elif defined(_MSC_VER)
#include <windows.h>

static inline unsigned int GetPid(void)
{
    return GetCurrentProcessId();
}
static inline unsigned int GetTid(void)
{
    return GetCurrentThreadId();
}
static inline std::string GetEnv(const char* name)
{
    char* r = nullptr;
    size_t len = 0;

    if (_dupenv_s(&r, &len, name) == 0 && r != nullptr) {
        std::string val(r, len);
        std::free(r);
        return val;
    }
    return "";
}
#endif
#endif
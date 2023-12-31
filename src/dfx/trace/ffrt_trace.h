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

#ifndef __FFRT_TRACE_H__
#define __FFRT_TRACE_H__

#include <atomic>
#include <chrono>
#include "internal_inc/osal.h"

#ifdef FFRT_OH_TRACE_ENABLE
#include "hitrace_meter.h"
#endif

namespace ffrt {
enum TraceLevel {
    TRACE_LEVEL0 = 0,
    TRACE_LEVEL1,
    TRACE_LEVEL2,
    TRACE_LEVEL3, // lowest level, trace all
    TRACE_LEVEL_MAX,
};

class TraceLevelManager {
public:
    TraceLevelManager();
    ~TraceLevelManager() = default;

    uint64_t GetTraceLevel() const
    {
        return traceLevel_;
    }

    static inline TraceLevelManager* Instance()
    {
        static TraceLevelManager ins;
        return &ins;
    }

private:
    uint8_t traceLevel_;
};

class ScopedTrace {
public:
    ScopedTrace(uint64_t level, const char* name);
    ~ScopedTrace();

private:
    std::atomic<bool> isTraceEnable_;
};
} // namespace ffrt

#ifdef FFRT_OH_TRACE_ENABLE
#define FFRT_TRACE_BEGIN(tag) StartTrace(HITRACE_TAG_FFRT, tag, -1)
#define FFRT_TRACE_END() FinishTrace(HITRACE_TAG_FFRT)
#define FFRT_TRACE_ASYNC_BEGIN(tag, tid) StartAsyncTrace(HITRACE_TAG_FFRT, tag, tid, -1)
#define FFRT_TRACE_ASYNC_END(tag, tid) FinishAsyncTrace(HITRACE_TAG_FFRT, tag, tid)
#define FFRT_TRACE_SCOPE(level, tag) ffrt::ScopedTrace __tracer##tag(level, #tag)
#else
#define FFRT_TRACE_BEGIN(tag)
#define FFRT_TRACE_END()
#define FFRT_TRACE_SCOPE(level, tag)
#endif

// DFX Trace for FFRT Task Statistics
#ifdef FFRT_OH_TASK_STAT_ENABLE
#define FFRT_WORKER_IDLE_BEGIN_MARKER()
#define FFRT_WORKER_IDLE_END_MARKER()
#define FFRT_SUBMIT_MARKER(tag, gid) \
    { \
        FFRT_TRACE_BEGIN(("P[" + (tag) + "]|" + std::to_string(gid)).c_str()); \
        FFRT_TRACE_END(); \
    }
#define FFRT_READY_MARKER(gid) \
    { \
        FFRT_TRACE_ASYNC_END("R", gid); \
    }
#define FFRT_BLOCK_MARKER(gid) \
    { \
        FFRT_TRACE_ASYNC_END("B", gid); \
    }
#define FFRT_TASKDONE_MARKER(gid) \
    { \
        FFRT_TRACE_ASYNC_END("F", gid); \
    }
#define FFRT_FAKE_TRACE_MARKER(gid) \
    { \
        FFRT_TRACE_ASYNC_END("Co", gid); \
    }
#define FFRT_TASK_BEGIN(tag, gid) \
    { \
        FFRT_TRACE_BEGIN(("FFRT::[" + (tag) + "]|" + std::to_string(gid)).c_str()); \
    }
#define FFRT_TASK_END() \
    { \
        FFRT_TRACE_END(); \
    }
#define FFRT_BLOCK_TRACER(gid, tag) \
    { \
        FFRT_TRACE_BEGIN(("FFBK[" #tag "]|" + std::to_string(gid)).c_str()); \
        FFRT_TRACE_END(); \
    }
#define FFRT_WAKE_TRACER(gid) \
    { \
        FFRT_TRACE_BEGIN(("FFWK|" + std::to_string(gid)).c_str()); \
        FFRT_TRACE_END(); \
    }
#else
#define FFRT_WORKER_IDLE_BEGIN_MARKER()
#define FFRT_WORKER_IDLE_END_MARKER()
#define FFRT_SUBMIT_MARKER(tag, gid)
#define FFRT_READY_MARKER(gid)
#define FFRT_BLOCK_MARKER(gid)
#define FFRT_TASKDONE_MARKER(gid)
#define FFRT_FAKE_TRACE_MARKER(gid)
#define FFRT_TASK_BEGIN(tag, gid)
#define FFRT_TASK_END()
#define FFRT_BLOCK_TRACER(gid, tag)
#define FFRT_WAKE_TRACER(gid)
#endif
#endif
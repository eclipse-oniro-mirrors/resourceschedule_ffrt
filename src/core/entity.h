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

#ifndef FFRT_ENTITY_HPP
#define FFRT_ENTITY_HPP

#include <unordered_map>
#include <list>

#include "sync/sync.h"

#if (!defined _MSC_VER) // In windows forced to use thread
#define EU_COROUTINE
#endif

namespace ffrt {
struct VersionCtx;

struct Entity {
    static inline Entity* Instance()
    {
        static Entity ins;
        return &ins;
    }

    VersionCtx* VA2Ctx(const void* p, TaskCtx* task);
    void RecycleVersion();

    std::list<VersionCtx*> versionTrashcan; // VersionCtx to be deleted
    std::unordered_map<const void*, VersionCtx*> vaMap;
#ifdef MUTEX_PERF // Mutex Lock&Unlock Cycles Statistic
    xx::mutex criticalMutex_ {"DependencyManager::criticalMutex_"};
#else
    /* It is only used to ensure the consistency between multiple groups of ctx,
     * and to ensure that the status cannot be changed between the query status and the do action
     */
    fast_mutex criticalMutex_;
#endif
};
} // namespace ffrt
#endif

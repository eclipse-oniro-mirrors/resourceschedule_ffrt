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
#ifndef __FFRT_BBOX_H__
#define __FFRT_BBOX_H__
#ifdef FFRT_BBOX_ENABLE

extern void TaskSubmitCounterInc(void);
extern void TaskDoneCounterInc(void);
extern void TaskEnQueuCounterInc(void);
extern void TaskRunCounterInc(void);
extern void TaskSwitchCounterInc(void);
extern void TaskFinishCounterInc(void);
extern unsigned int GetBboxEnableState(void);

// undefine in header for non-inline to explain why stop
void BboxFreeze(void);

// define in header for inline to speedup
static inline void BboxCheckAndFreeze(void)
{
    if (GetBboxEnableState() != 0) {
        BboxFreeze();
    }
}
#else
static inline void BboxCheckAndFreeze(void)
{}
#endif /* FFRT_BBOX_ENABLE */
void backtrace(int ignoreDepth);
#endif /* __FFRT_BBOX_H__ */
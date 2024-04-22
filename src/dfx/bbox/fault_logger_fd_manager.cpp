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
#include <cstdarg>
#include <cerrno>
#include <unistd.h>
#include <string>
#include <securec.h>
#include <iostream>
#include "dfx/bbox/fault_logger_fd_manager.h"
#include "dfx/log/ffrt_log_api.h"
#include "faultloggerd_client.h"

static const int g_logBufferSize = 2048;

FaultLoggerFdManager::FaultLoggerFdManager()
{
}

FaultLoggerFdManager::~FaultLoggerFdManager()
{
    CloseFd();
}

void FaultLoggerFdManager::CloseFd()
{
    if (faultLoggerFd_ >= 0) {
        close(faultLoggerFd_);
        faultLoggerFd_ = -1;
    }
}

int FaultLoggerFdManager::InitFaultLoggerFd()
{
    if (faultLoggerFd_ == -1) {
        faultLoggerFd_ = RequestFileDescriptor(FaultLoggerType::FFRT_CRASH_LOG);
        FFRT_COND_DO_ERR((faultLoggerFd_ < 0), return faultLoggerFd_, "fail to InitFaultLoggerFd");
    }
    return faultLoggerFd_;
}

int FaultLoggerFdManager::GetFaultLoggerFd()
{
    return faultLoggerFd_;
}

void FaultLoggerFdManager::WriteFaultLogger(const char* format, ...)
{
    int fd = GetFaultLoggerFd();
    FFRT_COND_DO_ERR((fd <= 0), return, "invalid faultLoggerFd");

    char errLog[g_logBufferSize];
    va_list args;
    va_start(args, format);
    std::string formatStr(format);
    formatStr = formatStr + "\n";
    int ret = vsnprintf_s(errLog, sizeof(errLog), sizeof(errLog) - 1, formatStr.c_str(), args);
    va_end(args);
    if (ret < 0) {
        return;
    }

    std::string msg = errLog;
    int n = write(fd, msg.data(), msg.size());
    FFRT_COND_DO_ERR((n < 0), return, "fail to write faultLogger msg:%s, fd:%d, errno:%d", msg.data(), fd, errno);
}
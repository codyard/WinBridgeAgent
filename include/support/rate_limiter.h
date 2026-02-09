/*
 * Copyright (C) 2026 Codyard
 *
 * This file is part of WinBridgeAgent.
 *
 * WinBridgeAgent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * WinBridgeAgent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WinBridgeAgent. If not, see <https://www.gnu.org/licenses/\>.
 */
#ifndef CLAWDESK_RATE_LIMITER_H
#define CLAWDESK_RATE_LIMITER_H

#include <windows.h>
#include <string>
#include <map>
#include <deque>
#include <mutex>

// 简单的每-IP 滑动窗口限流器
// 默认：每个 IP 在 windowMs 毫秒内最多 maxRequests 次请求
class RateLimiter {
public:
    RateLimiter(int maxRequests = 60, DWORD windowMs = 60000);

    // 检查该 IP 是否允许请求，同时记录本次请求
    // 返回 true 表示放行，false 表示限流
    bool allow(const std::string& clientIp);

    // 清理过期条目（可定期调用，也会在 allow 中自动清理）
    void cleanup();

private:
    int maxRequests_;
    DWORD windowMs_;
    mutable std::mutex mutex_;
    std::map<std::string, std::deque<DWORD>> requests_;  // IP -> 请求时间戳列表
};

#endif // CLAWDESK_RATE_LIMITER_H

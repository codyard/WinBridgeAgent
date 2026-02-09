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
#include "support/rate_limiter.h"

RateLimiter::RateLimiter(int maxRequests, DWORD windowMs)
    : maxRequests_(maxRequests), windowMs_(windowMs) {}

bool RateLimiter::allow(const std::string& clientIp) {
    std::lock_guard<std::mutex> lock(mutex_);
    DWORD now = GetTickCount();
    auto& q = requests_[clientIp];

    // 移除窗口外的旧条目
    while (!q.empty() && (now - q.front()) > windowMs_) {
        q.pop_front();
    }

    if (static_cast<int>(q.size()) >= maxRequests_) {
        return false;  // 超限
    }

    q.push_back(now);
    return true;
}

void RateLimiter::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    DWORD now = GetTickCount();
    for (auto it = requests_.begin(); it != requests_.end(); ) {
        auto& q = it->second;
        while (!q.empty() && (now - q.front()) > windowMs_) {
            q.pop_front();
        }
        if (q.empty()) {
            it = requests_.erase(it);
        } else {
            ++it;
        }
    }
}

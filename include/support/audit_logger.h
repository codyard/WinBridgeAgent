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
#ifndef CLAWDESK_AUDIT_LOGGER_H
#define CLAWDESK_AUDIT_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <map>
#include <chrono>
#include <nlohmann/json.hpp>

namespace clawdesk {

// Risk level enum (shared with PolicyGuard)
enum class RiskLevel {
    Low,
    Medium,
    High,
    Critical
};

// Convert RiskLevel to string
std::string riskLevelToString(RiskLevel level);

// Audit log entry structure
struct AuditLogEntry {
    std::string time;           // UTC ISO 8601 timestamp
    std::string tool;           // Tool name
    RiskLevel risk;             // Risk level
    std::string result;         // "ok", "error", "denied"
    int duration_ms;            // Execution duration in milliseconds
    std::string error;          // Error message (optional, empty if no error)
    bool high_risk;             // v0.3.0: Mark high-risk operations (default false)
    nlohmann::json details;     // v0.3.0: Optional operation details (JSON format)
    
    // Constructor with default values
    AuditLogEntry()
        : duration_ms(0), high_risk(false), details(nlohmann::json::object()) {}
};

// AuditLogger class for thread-safe JSON Lines logging
class AuditLogger {
public:
    // Constructor: initialize logger with log file path
    explicit AuditLogger(const std::string& logPath);
    
    // Destructor: ensure file is closed
    ~AuditLogger();

    // Log a tool call
    void logToolCall(const AuditLogEntry& entry);

    // Clean up old logs based on retention days
    void cleanupOldLogs(int retentionDays);

    // Get statistics for the last N days
    std::map<std::string, int> getStats(int days);

    // Get current UTC timestamp in ISO 8601 format
    std::string getCurrentTimestamp();

private:

    // Write a single log line (JSON object)
    void writeLogLine(const std::string& line);

    // Ensure log directory exists
    void ensureLogDirectory();

    // Rotate log file if needed (e.g., daily rotation)
    void rotateLogIfNeeded();

    // Get log file path for a specific date
    std::string getLogFilePath(const std::string& date);

    // Parse ISO 8601 timestamp to time_point
    std::chrono::system_clock::time_point parseTimestamp(const std::string& timestamp);

    std::string logPath_;           // Base log file path
    std::mutex logMutex_;           // Mutex for thread-safe logging
    std::ofstream logFile_;         // Output file stream
    std::string currentDate_;       // Current date for rotation (YYYY-MM-DD)
};

} // namespace clawdesk

#endif // CLAWDESK_AUDIT_LOGGER_H

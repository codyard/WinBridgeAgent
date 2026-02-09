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
#include "support/audit_logger.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace clawdesk {

// Convert RiskLevel enum to string
std::string riskLevelToString(RiskLevel level) {
    switch (level) {
        case RiskLevel::Low:      return "low";
        case RiskLevel::Medium:   return "medium";
        case RiskLevel::High:     return "high";
        case RiskLevel::Critical: return "critical";
        default:                  return "unknown";
    }
}

// Constructor
AuditLogger::AuditLogger(const std::string& logPath)
    : logPath_(logPath), currentDate_("") {
    ensureLogDirectory();
    
    // Initialize with current date
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc;
    
#ifdef _WIN32
    gmtime_s(&tm_utc, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_utc);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%d");
    currentDate_ = oss.str();
    
    // Open log file in append mode
    std::string logFilePath = getLogFilePath(currentDate_);
    logFile_.open(logFilePath, std::ios::app);
    
    if (!logFile_.is_open()) {
        std::cerr << "Failed to open log file: " << logFilePath << std::endl;
    }
}

// Destructor
AuditLogger::~AuditLogger() {
    std::lock_guard<std::mutex> lock(logMutex_);
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

// Log a tool call
void AuditLogger::logToolCall(const AuditLogEntry& entry) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    // Check if we need to rotate the log file
    rotateLogIfNeeded();
    
    // Create JSON object
    json logJson;
    logJson["time"] = entry.time;
    logJson["tool"] = entry.tool;
    logJson["risk"] = riskLevelToString(entry.risk);
    logJson["result"] = entry.result;
    logJson["duration_ms"] = entry.duration_ms;
    
    // Only include error field if it's not empty
    if (!entry.error.empty()) {
        logJson["error"] = entry.error;
    }
    
    // v0.3.0: Add high_risk field if true
    if (entry.high_risk) {
        logJson["high_risk"] = true;
    }
    
    // v0.3.0: Add details field if not empty
    if (!entry.details.empty()) {
        logJson["details"] = entry.details;
    }
    
    // Write JSON line
    writeLogLine(logJson.dump());
}

// Clean up old logs based on retention days
void AuditLogger::cleanupOldLogs(int retentionDays) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    try {
        fs::path logDir = fs::path(logPath_).parent_path();
        
        if (!fs::exists(logDir) || !fs::is_directory(logDir)) {
            return;
        }
        
        // Calculate cutoff time
        auto now = std::chrono::system_clock::now();
        auto cutoff = now - std::chrono::hours(24 * retentionDays);
        
        // Iterate through log files
        for (const auto& entry : fs::directory_iterator(logDir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            
            std::string filename = entry.path().filename().string();
            
            // Check if it's a log file (audit-YYYY-MM-DD.log pattern)
            if (filename.find("audit-") == 0 && filename.find(".log") != std::string::npos) {
                // Get file modification time
                auto fileTime = fs::last_write_time(entry.path());
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    fileTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
                );
                
                // Delete if older than retention period
                if (sctp < cutoff) {
                    try {
                        fs::remove(entry.path());
                        std::cout << "Deleted old log file: " << filename << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to delete log file " << filename << ": " << e.what() << std::endl;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error during log cleanup: " << e.what() << std::endl;
    }
}

// Get statistics for the last N days
std::map<std::string, int> AuditLogger::getStats(int days) {
    std::map<std::string, int> stats;
    
    try {
        fs::path logDir = fs::path(logPath_).parent_path();
        
        if (!fs::exists(logDir) || !fs::is_directory(logDir)) {
            return stats;
        }
        
        // Calculate cutoff time
        auto now = std::chrono::system_clock::now();
        auto cutoff = now - std::chrono::hours(24 * days);
        
        // Iterate through log files
        for (const auto& entry : fs::directory_iterator(logDir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            
            std::string filename = entry.path().filename().string();
            
            // Check if it's a log file
            if (filename.find("audit-") == 0 && filename.find(".log") != std::string::npos) {
                // Get file modification time
                auto fileTime = fs::last_write_time(entry.path());
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    fileTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
                );
                
                // Process if within the time range
                if (sctp >= cutoff) {
                    std::ifstream logFileRead(entry.path());
                    std::string line;
                    
                    while (std::getline(logFileRead, line)) {
                        try {
                            json logEntry = json::parse(line);
                            std::string toolName = logEntry["tool"];
                            stats[toolName]++;
                        } catch (const std::exception& e) {
                            // Skip malformed lines
                            continue;
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting stats: " << e.what() << std::endl;
    }
    
    return stats;
}

// Get current UTC timestamp in ISO 8601 format
std::string AuditLogger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;
    
    std::tm tm_utc;
#ifdef _WIN32
    gmtime_s(&tm_utc, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_utc);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    
    return oss.str();
}

// Write a single log line (JSON object)
void AuditLogger::writeLogLine(const std::string& line) {
    if (logFile_.is_open()) {
        logFile_ << line << std::endl;
        logFile_.flush();  // Ensure immediate write
    }
}

// Ensure log directory exists
void AuditLogger::ensureLogDirectory() {
    try {
        fs::path logDir = fs::path(logPath_).parent_path();
        
        if (!fs::exists(logDir)) {
            fs::create_directories(logDir);
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to create log directory: " << e.what() << std::endl;
    }
}

// Rotate log file if needed (daily rotation)
void AuditLogger::rotateLogIfNeeded() {
    // Get current date
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc;
    
#ifdef _WIN32
    gmtime_s(&tm_utc, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_utc);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%d");
    std::string newDate = oss.str();
    
    // Check if date has changed
    if (newDate != currentDate_) {
        // Close current file
        if (logFile_.is_open()) {
            logFile_.close();
        }
        
        // Update current date
        currentDate_ = newDate;
        
        // Open new log file
        std::string logFilePath = getLogFilePath(currentDate_);
        logFile_.open(logFilePath, std::ios::app);
        
        if (!logFile_.is_open()) {
            std::cerr << "Failed to open rotated log file: " << logFilePath << std::endl;
        }
    }
}

// Get log file path for a specific date
std::string AuditLogger::getLogFilePath(const std::string& date) {
    fs::path logPath(logPath_);
    fs::path logDir = logPath.parent_path();
    std::string baseName = logPath.stem().string();
    std::string extension = logPath.extension().string();
    
    // Create filename: audit-YYYY-MM-DD.log
    std::string filename = baseName + "-" + date + extension;
    
    return (logDir / filename).string();
}

// Parse ISO 8601 timestamp to time_point
std::chrono::system_clock::time_point AuditLogger::parseTimestamp(const std::string& timestamp) {
    std::tm tm = {};
    std::istringstream ss(timestamp);
    
    // Parse ISO 8601 format: YYYY-MM-DDTHH:MM:SS.sssZ
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    
    return tp;
}

} // namespace clawdesk

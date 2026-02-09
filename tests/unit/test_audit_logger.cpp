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
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using namespace clawdesk;

// Test helper: read all lines from a file
std::vector<std::string> readAllLines(const std::string& filepath) {
    std::vector<std::string> lines;
    std::ifstream file(filepath);
    std::string line;
    
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    
    return lines;
}

// Test 1: Basic logging functionality
void testBasicLogging() {
    std::cout << "Test 1: Basic logging functionality..." << std::endl;
    
    // Create a temporary log file path
    std::string logPath = "test_logs/audit.log";
    
    // Clean up any existing test logs
    if (fs::exists("test_logs")) {
        fs::remove_all("test_logs");
    }
    
    // Create logger
    AuditLogger logger(logPath);
    
    // Log a tool call
    AuditLogEntry entry;
    entry.time = "2026-02-03T12:03:01.234Z";
    entry.tool = "screenshot_full";
    entry.risk = RiskLevel::High;
    entry.result = "ok";
    entry.duration_ms = 128;
    entry.error = "";
    
    logger.logToolCall(entry);
    
    // Verify log file was created
    std::string expectedLogFile = "test_logs/audit-2026-02-03.log";
    
    // Since we're using current date for rotation, let's check the actual file
    // Get current date
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc;
    
#ifdef _WIN32
    gmtime_s(&tm_utc, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_utc);
#endif
    
    char dateStr[11];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &tm_utc);
    std::string actualLogFile = std::string("test_logs/audit-") + dateStr + ".log";
    
    assert(fs::exists(actualLogFile));
    
    // Read and verify log content
    auto lines = readAllLines(actualLogFile);
    assert(lines.size() == 1);
    
    // Verify JSON format
    assert(lines[0].find("\"time\"") != std::string::npos);
    assert(lines[0].find("\"tool\":\"screenshot_full\"") != std::string::npos);
    assert(lines[0].find("\"risk\":\"high\"") != std::string::npos);
    assert(lines[0].find("\"result\":\"ok\"") != std::string::npos);
    assert(lines[0].find("\"duration_ms\":128") != std::string::npos);
    
    std::cout << "✓ Basic logging test passed" << std::endl;
    
    // Clean up
    fs::remove_all("test_logs");
}

// Test 2: Multiple log entries
void testMultipleEntries() {
    std::cout << "Test 2: Multiple log entries..." << std::endl;
    
    std::string logPath = "test_logs/audit.log";
    
    // Clean up
    if (fs::exists("test_logs")) {
        fs::remove_all("test_logs");
    }
    
    AuditLogger logger(logPath);
    
    // Log multiple entries
    for (int i = 0; i < 5; i++) {
        AuditLogEntry entry;
        entry.time = "2026-02-03T12:03:0" + std::to_string(i) + ".000Z";
        entry.tool = "test_tool_" + std::to_string(i);
        entry.risk = RiskLevel::Low;
        entry.result = "ok";
        entry.duration_ms = 10 + i;
        entry.error = "";
        
        logger.logToolCall(entry);
    }
    
    // Get current date for log file
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc;
    
#ifdef _WIN32
    gmtime_s(&tm_utc, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_utc);
#endif
    
    char dateStr[11];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &tm_utc);
    std::string actualLogFile = std::string("test_logs/audit-") + dateStr + ".log";
    
    // Verify 5 entries
    auto lines = readAllLines(actualLogFile);
    assert(lines.size() == 5);
    
    std::cout << "✓ Multiple entries test passed" << std::endl;
    
    // Clean up
    fs::remove_all("test_logs");
}

// Test 3: Thread safety
void testThreadSafety() {
    std::cout << "Test 3: Thread safety..." << std::endl;
    
    std::string logPath = "test_logs/audit.log";
    
    // Clean up
    if (fs::exists("test_logs")) {
        fs::remove_all("test_logs");
    }
    
    AuditLogger logger(logPath);
    
    // Create multiple threads that log concurrently
    const int numThreads = 10;
    const int entriesPerThread = 10;
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([&logger, t]() {
            for (int i = 0; i < entriesPerThread; i++) {
                AuditLogEntry entry;
                entry.time = "2026-02-03T12:03:01.000Z";
                entry.tool = "thread_" + std::to_string(t) + "_entry_" + std::to_string(i);
                entry.risk = RiskLevel::Medium;
                entry.result = "ok";
                entry.duration_ms = 5;
                entry.error = "";
                
                logger.logToolCall(entry);
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Get current date for log file
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc;
    
#ifdef _WIN32
    gmtime_s(&tm_utc, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_utc);
#endif
    
    char dateStr[11];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &tm_utc);
    std::string actualLogFile = std::string("test_logs/audit-") + dateStr + ".log";
    
    // Verify all entries were logged
    auto lines = readAllLines(actualLogFile);
    assert(lines.size() == numThreads * entriesPerThread);
    
    std::cout << "✓ Thread safety test passed" << std::endl;
    
    // Clean up
    fs::remove_all("test_logs");
}

// Test 4: Error logging
void testErrorLogging() {
    std::cout << "Test 4: Error logging..." << std::endl;
    
    std::string logPath = "test_logs/audit.log";
    
    // Clean up
    if (fs::exists("test_logs")) {
        fs::remove_all("test_logs");
    }
    
    AuditLogger logger(logPath);
    
    // Log an error entry
    AuditLogEntry entry;
    entry.time = "2026-02-03T12:03:01.234Z";
    entry.tool = "run_command_restricted";
    entry.risk = RiskLevel::Critical;
    entry.result = "error";
    entry.duration_ms = 5;
    entry.error = "Command not in whitelist";
    
    logger.logToolCall(entry);
    
    // Get current date for log file
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc;
    
#ifdef _WIN32
    gmtime_s(&tm_utc, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_utc);
#endif
    
    char dateStr[11];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &tm_utc);
    std::string actualLogFile = std::string("test_logs/audit-") + dateStr + ".log";
    
    // Verify error field is included
    auto lines = readAllLines(actualLogFile);
    assert(lines.size() == 1);
    assert(lines[0].find("\"error\":\"Command not in whitelist\"") != std::string::npos);
    assert(lines[0].find("\"result\":\"error\"") != std::string::npos);
    
    std::cout << "✓ Error logging test passed" << std::endl;
    
    // Clean up
    fs::remove_all("test_logs");
}

// Test 5: Risk level conversion
void testRiskLevelConversion() {
    std::cout << "Test 5: Risk level conversion..." << std::endl;
    
    assert(riskLevelToString(RiskLevel::Low) == "low");
    assert(riskLevelToString(RiskLevel::Medium) == "medium");
    assert(riskLevelToString(RiskLevel::High) == "high");
    assert(riskLevelToString(RiskLevel::Critical) == "critical");
    
    std::cout << "✓ Risk level conversion test passed" << std::endl;
}

// Test 6: Statistics
void testStatistics() {
    std::cout << "Test 6: Statistics..." << std::endl;
    
    std::string logPath = "test_logs/audit.log";
    
    // Clean up
    if (fs::exists("test_logs")) {
        fs::remove_all("test_logs");
    }
    
    AuditLogger logger(logPath);
    
    // Log multiple entries with different tools
    std::vector<std::string> tools = {"screenshot_full", "find_files", "clipboard_read", "screenshot_full", "find_files", "screenshot_full"};
    
    for (const auto& tool : tools) {
        AuditLogEntry entry;
        entry.time = "2026-02-03T12:03:01.000Z";
        entry.tool = tool;
        entry.risk = RiskLevel::Low;
        entry.result = "ok";
        entry.duration_ms = 10;
        entry.error = "";
        
        logger.logToolCall(entry);
    }
    
    // Get statistics
    auto stats = logger.getStats(1);
    
    // Verify counts
    assert(stats["screenshot_full"] == 3);
    assert(stats["find_files"] == 2);
    assert(stats["clipboard_read"] == 1);
    
    std::cout << "✓ Statistics test passed" << std::endl;
    
    // Clean up
    fs::remove_all("test_logs");
}

// Test 7: High-risk operation logging (v0.3.0)
void testHighRiskLogging() {
    std::cout << "Test 7: High-risk operation logging..." << std::endl;
    
    std::string logPath = "test_logs/audit.log";
    
    // Clean up
    if (fs::exists("test_logs")) {
        fs::remove_all("test_logs");
    }
    
    AuditLogger logger(logPath);
    
    // Test 1: Process termination
    {
        AuditLogEntry entry;
        entry.time = "2026-02-03T12:00:00.000Z";
        entry.tool = "kill_process";
        entry.risk = RiskLevel::High;
        entry.result = "ok";
        entry.duration_ms = 15;
        entry.high_risk = true;
        entry.details = nlohmann::json{
            {"pid", 1234},
            {"process_name", "notepad.exe"},
            {"forced", false}
        };
        
        logger.logToolCall(entry);
    }
    
    // Test 2: File deletion
    {
        AuditLogEntry entry;
        entry.time = "2026-02-03T12:05:00.000Z";
        entry.tool = "delete_file";
        entry.risk = RiskLevel::High;
        entry.result = "ok";
        entry.duration_ms = 230;
        entry.high_risk = true;
        entry.details = nlohmann::json{
            {"path", "C:/Users/test/temp/file.txt"},
            {"type", "file"},
            {"recursive", false}
        };
        
        logger.logToolCall(entry);
    }
    
    // Test 3: Power management
    {
        AuditLogEntry entry;
        entry.time = "2026-02-03T12:10:00.000Z";
        entry.tool = "shutdown_system";
        entry.risk = RiskLevel::Critical;
        entry.result = "ok";
        entry.duration_ms = 50;
        entry.high_risk = true;
        entry.details = nlohmann::json{
            {"action", "shutdown"},
            {"delay", 60},
            {"force", false},
            {"scheduled_time", "2026-02-03T12:11:00.000Z"}
        };
        
        logger.logToolCall(entry);
    }
    
    // Get current date for log file
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc;
    
#ifdef _WIN32
    gmtime_s(&tm_utc, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_utc);
#endif
    
    char dateStr[11];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &tm_utc);
    std::string actualLogFile = std::string("test_logs/audit-") + dateStr + ".log";
    
    // Verify log entries
    auto lines = readAllLines(actualLogFile);
    assert(lines.size() == 3);
    
    // Verify process termination log
    assert(lines[0].find("\"high_risk\":true") != std::string::npos);
    assert(lines[0].find("\"details\"") != std::string::npos);
    assert(lines[0].find("\"pid\":1234") != std::string::npos);
    assert(lines[0].find("\"process_name\":\"notepad.exe\"") != std::string::npos);
    assert(lines[0].find("\"forced\":false") != std::string::npos);
    
    // Verify file deletion log
    assert(lines[1].find("\"high_risk\":true") != std::string::npos);
    assert(lines[1].find("\"path\":\"C:/Users/test/temp/file.txt\"") != std::string::npos);
    assert(lines[1].find("\"type\":\"file\"") != std::string::npos);
    assert(lines[1].find("\"recursive\":false") != std::string::npos);
    
    // Verify power management log
    assert(lines[2].find("\"high_risk\":true") != std::string::npos);
    assert(lines[2].find("\"action\":\"shutdown\"") != std::string::npos);
    assert(lines[2].find("\"delay\":60") != std::string::npos);
    assert(lines[2].find("\"scheduled_time\":\"2026-02-03T12:11:00.000Z\"") != std::string::npos);
    
    std::cout << "✓ High-risk operation logging test passed" << std::endl;
    
    // Clean up
    fs::remove_all("test_logs");
}

// Test 8: Backward compatibility (v0.3.0)
void testBackwardCompatibility() {
    std::cout << "Test 8: Backward compatibility..." << std::endl;
    
    std::string logPath = "test_logs/audit.log";
    
    // Clean up
    if (fs::exists("test_logs")) {
        fs::remove_all("test_logs");
    }
    
    AuditLogger logger(logPath);
    
    // Log a regular (non-high-risk) entry without details
    AuditLogEntry entry;
    entry.time = "2026-02-03T12:03:01.234Z";
    entry.tool = "screenshot_full";
    entry.risk = RiskLevel::High;
    entry.result = "ok";
    entry.duration_ms = 128;
    entry.error = "";
    // high_risk defaults to false
    // details defaults to empty object
    
    logger.logToolCall(entry);
    
    // Get current date for log file
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc;
    
#ifdef _WIN32
    gmtime_s(&tm_utc, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_utc);
#endif
    
    char dateStr[11];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &tm_utc);
    std::string actualLogFile = std::string("test_logs/audit-") + dateStr + ".log";
    
    // Verify log entry does NOT include high_risk or details fields
    auto lines = readAllLines(actualLogFile);
    assert(lines.size() == 1);
    assert(lines[0].find("\"high_risk\"") == std::string::npos);
    assert(lines[0].find("\"details\"") == std::string::npos);
    
    // Verify standard fields are present
    assert(lines[0].find("\"tool\":\"screenshot_full\"") != std::string::npos);
    assert(lines[0].find("\"risk\":\"high\"") != std::string::npos);
    assert(lines[0].find("\"result\":\"ok\"") != std::string::npos);
    
    std::cout << "✓ Backward compatibility test passed" << std::endl;
    
    // Clean up
    fs::remove_all("test_logs");
}

int main() {
    std::cout << "Running AuditLogger tests..." << std::endl;
    std::cout << "================================" << std::endl;
    
    try {
        testBasicLogging();
        testMultipleEntries();
        testThreadSafety();
        testErrorLogging();
        testRiskLevelConversion();
        testStatistics();
        testHighRiskLogging();
        testBackwardCompatibility();
        
        std::cout << "================================" << std::endl;
        std::cout << "All tests passed! ✓" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}

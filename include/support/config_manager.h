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
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <mutex>

/**
 * ServerConfig - 服务器配置结构
 * 
 * 包含所有服务器运行所需的配置项：
 * - auth_token: Bearer Token 用于客户端认证
 * - server_port: 期望的监听端口（0 表示随机端口）
 * - auto_port: 端口被占用时是否自动选择随机端口
 * - allowed_dirs: 允许访问的目录白名单
 * - allowed_apps: 允许启动的应用程序白名单（名称 -> 路径）
 * - allowed_commands: 允许执行的命令白名单
 * - license_key: 许可证密钥（可选）
 */
struct ServerConfig {
    std::string auth_token;
    int server_port;                                    // 默认 35182，0 表示随机端口
    bool auto_port;                                     // 默认 true
    std::string listen_address;                         // 监听地址："0.0.0.0" 或 "127.0.0.1"
    std::vector<std::string> allowed_dirs;
    std::map<std::string, std::string> allowed_apps;
    std::vector<std::string> allowed_commands;
    std::string license_key;
    std::string language;                               // 语言设置
    bool auto_startup;                                  // 开机自启动
    std::string api_key;                                // API Key
    bool high_risk_confirmations;                       // 高风险操作确认
    bool dashboard_auto_show;                           // Dashboard 启动自动显示
    bool dashboard_always_on_top;                       // Dashboard 总在最前
    std::string tray_icon_style;                        // 托盘图标样式
    int log_retention_days;                             // 日志保留天数
    bool daemon_enabled;                                // 守护进程启用
    
    // 更新配置
    bool auto_update_enabled;                           // 启用自动更新
    int update_check_interval_hours;                    // 检查更新间隔（小时）
    std::string update_channel;                         // 更新通道："stable" 或 "beta"
    std::string github_repo;                            // GitHub 仓库（如 "owner/repo"）
    bool update_verify_signature;                       // 验证代码签名
    std::string last_update_check;                      // 上次检查时间（ISO 8601）
    std::string skipped_version;                        // 跳过的版本号
    int command_timeout_seconds;                         // 命令执行超时（秒），默认 30
};

/**
 * ConfigManager - 配置管理器
 * 
 * 负责配置文件的加载、保存和访问。
 * 
 * 功能：
 * - 从 config.json 加载配置
 * - 保存配置到 config.json
 * - 生成默认配置（包括自动生成 Auth Token）
 * - 线程安全的配置访问
 * 
 * 需求覆盖：
 * - 2.3: Auth Token 自动生成
 * - 2.4: Auth Token 丢失时重新生成
 * - 14.1: 从 config.json 加载配置
 * - 14.2: 支持配置 Auth Token、白名单等
 * - 14.3: 配置文件不存在时创建默认配置
 * - 14.4: 配置文件修改后重启加载
 * - 14.5: 验证配置文件格式和内容
 */
class ConfigManager {
public:
    /**
     * 构造函数
     * @param configPath 配置文件路径（默认为 config.json）
     */
    explicit ConfigManager(const std::string& configPath = "config.json");

    /**
     * 加载配置文件
     * 如果文件不存在，则生成默认配置并保存
     * 如果文件格式错误，抛出异常
     */
    void load();

    /**
     * 保存配置到文件
     */
    void save();

    // ===== 配置项访问器 =====

    /**
     * 获取 Auth Token
     * @return Auth Token 字符串
     */
    std::string getAuthToken() const;

    /**
     * 获取允许的目录列表
     * @return 目录路径列表
     */
    std::vector<std::string> getAllowedDirs() const;

    /**
     * 获取允许的应用程序映射
     * @return 应用名称 -> 路径的映射
     */
    std::map<std::string, std::string> getAllowedApps() const;

    /**
     * 获取允许的命令列表
     * @return 命令名称列表
     */
    std::vector<std::string> getAllowedCommands() const;

    /**
     * 获取许可证密钥
     * @return 许可证密钥（可能为空）
     */
    std::string getLicenseKey() const;

    /**
     * 获取服务器端口配置
     * @return 端口号（0 表示随机端口）
     */
    int getServerPort() const;

    /**
     * 检查是否启用自动端口选择
     * @return true 表示启用自动端口
     */
    bool isAutoPortEnabled() const;

    /**
     * 获取监听地址
     * @return 监听地址（"0.0.0.0" 或 "127.0.0.1"）
     */
    std::string getListenAddress() const;

    std::string getLanguage() const;
    bool isAutoStartupEnabled() const;
    std::string getApiKey() const;
    bool isHighRiskConfirmationEnabled() const;
    bool isDashboardAutoShowEnabled() const;
    bool isDashboardAlwaysOnTopEnabled() const;
    std::string getTrayIconStyle() const;
    int getLogRetentionDays() const;
    bool isDaemonEnabled() const;
    int getCommandTimeoutSeconds() const;

    // ===== 配置项修改器 =====

    /**
     * 设置许可证密钥
     * @param key 许可证密钥
     */
    void setLicenseKey(const std::string& key);

    /**
     * 设置实际使用的端口（保存到配置中）
     * @param port 实际端口号
     */
    void setActualPort(int port);
    void setAutoPort(bool enabled);

    /**
     * 设置监听地址
     * @param address 监听地址（"0.0.0.0" 或 "127.0.0.1"）
     */
    void setListenAddress(const std::string& address);
    void setAuthToken(const std::string& token);
    void setAllowedDirs(const std::vector<std::string>& dirs);
    void setAllowedApps(const std::map<std::string, std::string>& apps);
    void setAllowedCommands(const std::vector<std::string>& commands);

    void setLanguage(const std::string& language);
    void setAutoStartup(bool enabled);
    void setApiKey(const std::string& apiKey);
    void setHighRiskConfirmations(bool enabled);
    void setDashboardAutoShow(bool enabled);
    void setDashboardAlwaysOnTop(bool enabled);
    void setTrayIconStyle(const std::string& style);
    void setLogRetentionDays(int days);
    void setDaemonEnabled(bool enabled);
    
    // 更新配置访问器
    bool isAutoUpdateEnabled() const;
    int getUpdateCheckIntervalHours() const;
    std::string getUpdateChannel() const;
    std::string getGitHubRepo() const;
    bool isUpdateVerifySignatureEnabled() const;
    std::string getLastUpdateCheck() const;
    std::string getSkippedVersion() const;
    
    // 更新配置修改器
    void setAutoUpdateEnabled(bool enabled);
    void setUpdateCheckIntervalHours(int hours);
    void setUpdateChannel(const std::string& channel);
    void setGitHubRepo(const std::string& repo);
    void setUpdateVerifySignature(bool enabled);
    void setLastUpdateCheck(const std::string& timestamp);
    void setSkippedVersion(const std::string& version);

private:
    /**
     * 生成默认配置
     * @return 默认的 ServerConfig 对象
     */
    ServerConfig generateDefaultConfig();

    /**
     * 生成随机 Auth Token
     * @return 随机生成的 Token 字符串
     */
    std::string generateAuthToken();

    /**
     * 验证配置的有效性
     * @param config 要验证的配置
     * @return true 表示配置有效
     */
    bool validateConfig(const ServerConfig& config) const;

    ServerConfig config_;           // 当前配置
    std::string configPath_;        // 配置文件路径
    mutable std::mutex configMutex_; // 线程安全锁
};

#endif // CONFIG_MANAGER_H

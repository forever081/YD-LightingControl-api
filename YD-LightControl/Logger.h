#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <windows.h>

enum class LogLevel {
    DEBUG = 0,
    INFO,
    WARN,
    ERR
};

class Logger {
public:
    static Logger& Instance();

    void Init(const std::string& logDir,
        size_t maxFileSizeBytes,
        int retainDays,
        LogLevel level = LogLevel::DEBUG);

    void Log(LogLevel level,
        const std::string& module,
        const std::string& msg);

    void LogHex(LogLevel level,
        const std::string& module,
        const std::string& prefix,
        const std::vector<uint8_t>& data);

    std::string GetLastErrorString();

    ~Logger();

private:
    Logger();

    std::string GetDateString();
    std::string BuildLogFileName();
    void RotateIfNeeded();
    void CleanupOldLogs();
    void CreateDirIfNotExist(const std::string& path);

private:
    std::mutex m_mutex;
    std::ofstream m_ofs;

    std::string m_logDir;
    std::string m_currentDate;
    size_t m_maxSize = 0;
    int m_retainDays = 0;
    int m_fileIndex = 0;
    LogLevel m_level = LogLevel::DEBUG;
};

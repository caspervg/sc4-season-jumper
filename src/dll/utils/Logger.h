#pragma once
#include <memory>
#include <string>
#include <spdlog/spdlog.h>

class Logger
{
public:
    // Get the singleton instance
    static std::shared_ptr<spdlog::logger> Get();
    static void Initialize(const std::string& logName = "SC4SeasonJumper",
                           const std::string& userDir = "",
                           bool logToFile = true);

    // Set the log level (and flush level) at runtime
    static void SetLevel(spdlog::level::level_enum logLevel);

    // Shutdown the logger (called at exit)
    static void Shutdown();

private:
    static std::shared_ptr<spdlog::logger> s_logger;
    static std::string s_logName;
    static bool s_initialized;

    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};

// Convenience macros for logging
#define LOG_TRACE(...) Logger::Get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) Logger::Get()->debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::Get()->info(__VA_ARGS__)
#define LOG_WARN(...) Logger::Get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::Get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) Logger::Get()->critical(__VA_ARGS__)
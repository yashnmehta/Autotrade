#include "logger.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>

namespace nsefo {

// Static member initialization
LogLevel Logger::current_level = LogLevel::INFO;
std::ofstream Logger::file_stream;
bool Logger::log_to_console = true;
std::mutex Logger::log_mutex;

void Logger::init(LogLevel level, const std::string& logFile, bool console) {
    current_level = level;
    log_to_console = console;
    
    if (!logFile.empty()) {
        file_stream.open(logFile, std::ios::app);
        if (!file_stream.is_open()) {
            std::cerr << "Warning: Could not open log file: " << logFile << std::endl;
        }
    }
}

void Logger::setLevel(LogLevel level) {
    current_level = level;
}

void Logger::close() {
    if (file_stream.is_open()) {
        file_stream.close();
    }
}

std::string Logger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_now;
    localtime_r(&time_t_now, &tm_now);
    
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        default: return "?????";
    }
}

LogLevel Logger::stringToLevel(const std::string& str) {
    if (str == "DEBUG") return LogLevel::DEBUG;
    if (str == "INFO")  return LogLevel::INFO;
    if (str == "WARN")  return LogLevel::WARN;
    if (str == "ERROR") return LogLevel::ERROR;
    return LogLevel::INFO;  // Default
}

} // namespace nsefo

#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <sstream>
#include <iostream>

namespace nsecm {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
public:
    static void init(LogLevel level, const std::string& logFile = "", bool console = true);
    static void setLevel(LogLevel level);
    
    template<typename... Args>
    static void debug(Args&&... args) {
        log(LogLevel::DEBUG, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void info(Args&&... args) {
        log(LogLevel::INFO, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void warn(Args&&... args) {
        log(LogLevel::WARN, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void error(Args&&... args) {
        log(LogLevel::ERROR, std::forward<Args>(args)...);
    }
    
    static void close();
    
private:
    static LogLevel current_level;
    static std::ofstream file_stream;
    static bool log_to_console;
    static std::mutex log_mutex;
    
    template<typename... Args>
    static void log(LogLevel level, Args&&... args) {
        if (level < current_level) return;
        
        std::lock_guard<std::mutex> lock(log_mutex);
        
        std::ostringstream oss;
        oss << getTimestamp() << " [" << levelToString(level) << "] ";
        appendArgs(oss, std::forward<Args>(args)...);
        
        std::string message = oss.str();
        
        if (log_to_console) {
            if (level >= LogLevel::ERROR) {
                std::cerr << message << std::endl;
            } else {
                std::cout << message << std::endl;
            }
        }
        
        if (file_stream.is_open()) {
            file_stream << message << std::endl;
            file_stream.flush();
        }
    }
    
    template<typename T>
    static void appendArgs(std::ostringstream& oss, T&& arg) {
        oss << std::forward<T>(arg);
    }
    
    template<typename T, typename... Args>
    static void appendArgs(std::ostringstream& oss, T&& first, Args&&... rest) {
        oss << std::forward<T>(first);
        appendArgs(oss, std::forward<Args>(rest)...);
    }
    
    static std::string getTimestamp();
    static std::string levelToString(LogLevel level);
    static LogLevel stringToLevel(const std::string& str);
    
    friend class Config;
};

} // namespace nsecm

#endif // LOGGER_H

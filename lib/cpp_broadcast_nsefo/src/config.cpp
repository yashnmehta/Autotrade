#include "config.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>

namespace nsefo {

Config::Config() {
    setDefaults();
}

void Config::setDefaults() {
    // Network settings
    multicast_ip = "233.1.2.5";
    port = 34330;
    buffer_size = 2048;  // Must be at least 65535 for NSE packets
    socket_timeout_sec = 1;
    
    // Logging settings
    log_level = "INFO";
    log_file = "";  // Empty means no file logging
    log_to_console = true;
    
    // Statistics settings
    stats_interval_sec = 30;
    enable_stats = true;
    
    // Debug settings
    enable_hex_dump = false;
    hex_dump_size = 64;
}

std::string Config::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

bool Config::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    
    std::cout << "Loading configuration from file: " << filename << std::endl;
    
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open config file: " << filename << std::endl;
        return false;
    }
    
    std::string line;
    std::string current_section;
    
    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Section header
        if (line[0] == '[' && line[line.length()-1] == ']') {
            current_section = line.substr(1, line.length()-2);
            continue;
        }
        
        // Key=Value pair
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }
        
        std::string key = trim(line.substr(0, eq_pos));
        std::string value = trim(line.substr(eq_pos + 1));
        
        // Parse values
        if (key == "multicast_ip") {
            multicast_ip = value;
        } else if (key == "port") {
            port = std::stoi(value);
        } else if (key == "buffer_size") {
            buffer_size = std::stoul(value);
        } else if (key == "socket_timeout_sec") {
            socket_timeout_sec = std::stoi(value);
        } else if (key == "log_level") {
            log_level = value;
        } else if (key == "log_file") {
            log_file = value;
        } else if (key == "log_to_console") {
            log_to_console = (value == "true" || value == "1" || value == "yes");
        } else if (key == "stats_interval_sec") {
            stats_interval_sec = std::stoi(value);
        } else if (key == "enable_stats") {
            enable_stats = (value == "true" || value == "1" || value == "yes");
        } else if (key == "enable_hex_dump") {
            enable_hex_dump = (value == "true" || value == "1" || value == "yes");
        } else if (key == "hex_dump_size") {
            hex_dump_size = std::stoul(value);
        }
    }
    
    file.close();
    std::cout << "Configuration loaded from: " << filename << std::endl;
    return true;
}

void Config::loadFromEnv() {
    const char* env_val;
    
    if ((env_val = std::getenv("NSE_MULTICAST_IP"))) {
        multicast_ip = env_val;
    }
    if ((env_val = std::getenv("NSE_PORT"))) {
        port = std::stoi(env_val);
    }
    if ((env_val = std::getenv("NSE_BUFFER_SIZE"))) {
        buffer_size = std::stoul(env_val);
    }
    if ((env_val = std::getenv("NSE_LOG_LEVEL"))) {
        log_level = env_val;
    }
    if ((env_val = std::getenv("NSE_LOG_FILE"))) {
        log_file = env_val;
    }
    if ((env_val = std::getenv("NSE_STATS_INTERVAL"))) {
        stats_interval_sec = std::stoi(env_val);
    }
}

void Config::print() const {
    std::cout << "\n=== Configuration ===" << std::endl;
    std::cout << "[Network]" << std::endl;
    std::cout << "  multicast_ip = " << multicast_ip << std::endl;
    std::cout << "  port = " << port << std::endl;
    std::cout << "  buffer_size = " << buffer_size << std::endl;
    std::cout << "  socket_timeout_sec = " << socket_timeout_sec << std::endl;
    
    std::cout << "[Logging]" << std::endl;
    std::cout << "  log_level = " << log_level << std::endl;
    std::cout << "  log_file = " << (log_file.empty() ? "(none)" : log_file) << std::endl;
    std::cout << "  log_to_console = " << (log_to_console ? "true" : "false") << std::endl;
    
    std::cout << "[Statistics]" << std::endl;
    std::cout << "  stats_interval_sec = " << stats_interval_sec << std::endl;
    std::cout << "  enable_stats = " << (enable_stats ? "true" : "false") << std::endl;
    
    std::cout << "[Debug]" << std::endl;
    std::cout << "  enable_hex_dump = " << (enable_hex_dump ? "true" : "false") << std::endl;
    std::cout << "  hex_dump_size = " << hex_dump_size << std::endl;
    std::cout << std::endl;
}

} // namespace nsefo

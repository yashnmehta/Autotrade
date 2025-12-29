#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <iostream>

namespace nsefo {

class Config {
public:
    // Network settings
    std::string multicast_ip;
    int port;
    size_t buffer_size;
    int socket_timeout_sec;
    
    // Logging settings
    std::string log_level;
    std::string log_file;
    bool log_to_console;
    
    // Statistics settings
    int stats_interval_sec;
    bool enable_stats;
    
    // Debug settings
    bool enable_hex_dump;
    size_t hex_dump_size;
    
    // Constructor with defaults
    Config();
    
    // Load from file (INI format)
    bool loadFromFile(const std::string& filename);
    
    // Load from environment variables
    void loadFromEnv();
    
    // Print current configuration
    void print() const;
    
private:
    void setDefaults();
    std::string trim(const std::string& str);
};

} // namespace nsefo

#endif // CONFIG_H

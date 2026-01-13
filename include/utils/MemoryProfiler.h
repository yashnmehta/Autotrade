#ifndef MEMORY_PROFILER_H
#define MEMORY_PROFILER_H

#include <QString>
#include <cstdint>

/**
 * @brief High-precision memory profiling utility for Windows/Linux/macOS
 */
class MemoryProfiler {
public:
    struct MemoryStats {
        size_t physicalMemory; // Resident Set Size (RSS) - Actual RAM used
        size_t virtualMemory;  // Total address space used
        size_t peakPhysical;   // Highest RAM usage recorded
    };

    /**
     * @brief Get current memory usage of the process
     * @return MemoryStats structure with bytes
     */
    static MemoryStats getCurrentUsage();

    /**
     * @brief Convert bytes to a human-readable string (KB, MB, GB)
     */
    static QString formatBytes(size_t bytes);

    /**
     * @brief Log a memory snapshot to the console
     * @param label A label to identify where in the code this snapshot was taken
     */
    static void logSnapshot(const QString& label);
};

#endif // MEMORY_PROFILER_H

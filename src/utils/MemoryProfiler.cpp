#include "utils/MemoryProfiler.h"
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__) || defined(__MACH__)
#include <mach/mach.h>
#else
#include <unistd.h>
#include <ios>
#include <iostream>
#include <fstream>
#include <string>
#endif

MemoryProfiler::MemoryStats MemoryProfiler::getCurrentUsage() {
    MemoryStats stats = {0, 0, 0};

#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        stats.physicalMemory = pmc.WorkingSetSize;
        stats.virtualMemory = pmc.PrivateUsage;
        stats.peakPhysical = pmc.PeakWorkingSetSize;
    }
#elif defined(__APPLE__) || defined(__MACH__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS) {
        stats.physicalMemory = info.resident_size;
        stats.virtualMemory = info.virtual_size;
    }
#else
    // Linux RSS retrieval from /proc/self/statm
    long rss = 0, virt = 0;
    std::ifstream statm("/proc/self/statm");
    if (statm >> virt >> rss) {
        long pageSize = sysconf(_SC_PAGESIZE);
        stats.physicalMemory = rss * pageSize;
        stats.virtualMemory = virt * pageSize;
    }
#endif

    return stats;
}

QString MemoryProfiler::formatBytes(size_t bytes) {
    double size = static_cast<double>(bytes);
    QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIdx = 0;
    
    while (size >= 1024.0 && unitIdx < units.size() - 1) {
        size /= 1024.0;
        unitIdx++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(units[unitIdx]);
}

void MemoryProfiler::logSnapshot(const QString& label) {
    MemoryStats stats = getCurrentUsage();
    qDebug().noquote() << QString("[RAM] [%1] Physical: %2 | Virtual: %3 | Peak: %4")
                          .arg(label.leftJustified(20, ' '))
                          .arg(formatBytes(stats.physicalMemory).rightJustified(10, ' '))
                          .arg(formatBytes(stats.virtualMemory).rightJustified(10, ' '))
                          .arg(formatBytes(stats.peakPhysical).rightJustified(10, ' '));
}

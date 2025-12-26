#ifndef LATENCYTRACKER_H
#define LATENCYTRACKER_H

#include <chrono>
#include <cstdint>
#include <QString>
#include <QDebug>

/**
 * @brief Utility for tracking end-to-end latency in the trading terminal
 * 
 * Tracks data flow: UDP Receive → Parse → Queue → Dequeue → FeedHandler → Model → View
 * 
 * Usage:
 * ```cpp
 * // Stage 1: UDP receive
 * int64_t t1 = LatencyTracker::now();
 * 
 * // Stage 2: Parse
 * int64_t t2 = LatencyTracker::now();
 * 
 * // Calculate latency
 * int64_t parseLatency = t2 - t1;
 * ```
 */
class LatencyTracker {
public:
    /**
     * @brief Get current timestamp in microseconds
     * @return Microseconds since epoch
     */
    static inline int64_t now() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }
    
    /**
     * @brief Calculate latency in microseconds
     * @param start Start timestamp (µs)
     * @param end End timestamp (µs)
     * @return Latency in microseconds
     */
    static inline int64_t latency(int64_t start, int64_t end) {
        return end - start;
    }
    
    /**
     * @brief Print latency breakdown for a tick
     * @param refNo Reference number for tracking
     * @param token Exchange instrument ID
     * @param t_recv UDP receive timestamp
     * @param t_parse Parse timestamp
     * @param t_queue Queue enqueue timestamp
     * @param t_dequeue Queue dequeue timestamp
     * @param t_feedhandler FeedHandler process timestamp
     * @param t_model Model update timestamp
     * @param t_view View update timestamp
     */
    static void printLatencyBreakdown(
        uint64_t refNo,
        int64_t token,
        int64_t t_recv,
        int64_t t_parse,
        int64_t t_queue,
        int64_t t_dequeue,
        int64_t t_feedhandler,
        int64_t t_model,
        int64_t t_view
    ) {
        // Calculate stage latencies
        int64_t lat_parse = t_parse - t_recv;           // UDP → Parse
        int64_t lat_queue = t_queue - t_parse;          // Parse → Queue
        int64_t lat_wait = t_dequeue - t_queue;         // Queue wait time
        int64_t lat_feedhandler = t_feedhandler - t_dequeue;  // Dequeue → FeedHandler
        int64_t lat_model = t_model - t_feedhandler;    // FeedHandler → Model
        int64_t lat_view = t_view - t_model;            // Model → View
        int64_t lat_total = t_view - t_recv;            // Total end-to-end
        
        qDebug() << "╔═══════════════════════════════════════════════════════════════╗";
        qDebug() << "║          LATENCY BREAKDOWN - Ref:" << refNo << "Token:" << token;
        qDebug() << "╠═══════════════════════════════════════════════════════════════╣";
        qDebug().nospace() << "║ UDP → Parse:       " << QString("%1").arg(lat_parse, 6) << " µs (" << formatMicros(lat_parse) << ")";
        qDebug().nospace() << "║ Parse → Queue:     " << QString("%1").arg(lat_queue, 6) << " µs (" << formatMicros(lat_queue) << ")";
        qDebug().nospace() << "║ Queue Wait:        " << QString("%1").arg(lat_wait, 6) << " µs (" << formatMicros(lat_wait) << ") ⚠️";
        qDebug().nospace() << "║ Dequeue → Feed:    " << QString("%1").arg(lat_feedhandler, 6) << " µs (" << formatMicros(lat_feedhandler) << ")";
        qDebug().nospace() << "║ Feed → Model:      " << QString("%1").arg(lat_model, 6) << " µs (" << formatMicros(lat_model) << ")";
        qDebug().nospace() << "║ Model → View:      " << QString("%1").arg(lat_view, 6) << " µs (" << formatMicros(lat_view) << ")";
        qDebug() << "╠═══════════════════════════════════════════════════════════════╣";
        qDebug().nospace() << "║ TOTAL (UDP→Screen):" << QString("%1").arg(lat_total, 6) << " µs (" << formatMicros(lat_total) << ") " << getLatencyEmoji(lat_total);
        qDebug() << "╚═══════════════════════════════════════════════════════════════╝";
        
        // Highlight bottlenecks
        if (lat_wait > 1000) {
            qWarning() << "🔴 BOTTLENECK: Queue wait time =" << lat_wait << "µs (" << (lat_wait/1000.0) << "ms)";
        }
        if (lat_total > 5000) {
            qWarning() << "🔴 HIGH LATENCY: Total latency =" << lat_total << "µs (" << (lat_total/1000.0) << "ms)";
        }
    }
    
    /**
     * @brief Format microseconds as human-readable string
     */
    static QString formatMicros(int64_t micros) {
        if (micros < 1000) {
            return QString("%1µs").arg(micros);
        } else if (micros < 1000000) {
            return QString("%1ms").arg(micros / 1000.0, 0, 'f', 2);
        } else {
            return QString("%1s").arg(micros / 1000000.0, 0, 'f', 3);
        }
    }
    
    /**
     * @brief Get emoji for latency level
     */
    static QString getLatencyEmoji(int64_t micros) {
        if (micros < 1000) {
            return "✅ INSTANT";
        } else if (micros < 2000) {
            return "🟢 FAST";
        } else if (micros < 5000) {
            return "🟡 OK";
        } else if (micros < 16000) {
            return "🟠 NOTICEABLE";
        } else {
            return "🔴 SLOW";
        }
    }
    
    /**
     * @brief Track statistics for aggregate analysis
     */
    struct LatencyStats {
        int64_t count = 0;
        int64_t totalLatency = 0;
        int64_t minLatency = INT64_MAX;
        int64_t maxLatency = 0;
        
        void record(int64_t latency) {
            count++;
            totalLatency += latency;
            if (latency < minLatency) minLatency = latency;
            if (latency > maxLatency) maxLatency = latency;
        }
        
        double average() const {
            return count > 0 ? (double)totalLatency / count : 0.0;
        }
        
        void print(const QString& label) const {
            if (count == 0) {
                qDebug() << "[Stats]" << label << "- No data";
                return;
            }
            
            qDebug() << "╔═══════════════════════════════════════════════════════════╗";
            qDebug().nospace() << "║ " << label;
            qDebug() << "╠═══════════════════════════════════════════════════════════╣";
            qDebug().nospace() << "║ Samples:  " << count;
            qDebug().nospace() << "║ Average:  " << QString("%1").arg(average(), 0, 'f', 2) << " µs (" << LatencyTracker::formatMicros(average()) << ")";
            qDebug().nospace() << "║ Min:      " << minLatency << " µs (" << LatencyTracker::formatMicros(minLatency) << ")";
            qDebug().nospace() << "║ Max:      " << maxLatency << " µs (" << LatencyTracker::formatMicros(maxLatency) << ")";
            qDebug() << "╚═══════════════════════════════════════════════════════════╝";
        }
    };
    
    // Global statistics
    static LatencyStats s_parseStats;
    static LatencyStats s_queueStats;
    static LatencyStats s_waitStats;
    static LatencyStats s_feedHandlerStats;
    static LatencyStats s_modelStats;
    static LatencyStats s_viewStats;
    static LatencyStats s_totalStats;
    
    /**
     * @brief Record latency for a complete tick flow
     */
    static void recordLatency(
        int64_t t_recv,
        int64_t t_parse,
        int64_t t_queue,
        int64_t t_dequeue,
        int64_t t_feedhandler,
        int64_t t_model,
        int64_t t_view
    ) {
        s_parseStats.record(t_parse - t_recv);
        s_queueStats.record(t_queue - t_parse);
        s_waitStats.record(t_dequeue - t_queue);
        s_feedHandlerStats.record(t_feedhandler - t_dequeue);
        s_modelStats.record(t_model - t_feedhandler);
        s_viewStats.record(t_view - t_model);
        s_totalStats.record(t_view - t_recv);
    }
    
    /**
     * @brief Print aggregate statistics
     */
    static void printAggregateStats() {
        qDebug() << "";
        qDebug() << "╔═══════════════════════════════════════════════════════════════╗";
        qDebug() << "║           AGGREGATE LATENCY STATISTICS                       ║";
        qDebug() << "╚═══════════════════════════════════════════════════════════════╝";
        
        s_parseStats.print("UDP → Parse");
        s_queueStats.print("Parse → Queue");
        s_waitStats.print("Queue Wait Time");
        s_feedHandlerStats.print("FeedHandler Processing");
        s_modelStats.print("Model Update");
        s_viewStats.print("View Update");
        s_totalStats.print("🎯 TOTAL END-TO-END (UDP→Screen)");
        
        qDebug() << "";
    }
};

// Static member initialization
inline LatencyTracker::LatencyStats LatencyTracker::s_parseStats;
inline LatencyTracker::LatencyStats LatencyTracker::s_queueStats;
inline LatencyTracker::LatencyStats LatencyTracker::s_waitStats;
inline LatencyTracker::LatencyStats LatencyTracker::s_feedHandlerStats;
inline LatencyTracker::LatencyStats LatencyTracker::s_modelStats;
inline LatencyTracker::LatencyStats LatencyTracker::s_viewStats;
inline LatencyTracker::LatencyStats LatencyTracker::s_totalStats;

#endif // LATENCYTRACKER_H

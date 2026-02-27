#include "services/XTSFeedBridge.h"
#include "api/xts/XTSMarketDataClient.h"
#include <QDebug>
#include <QDateTime>
#include <algorithm>

// ═════════════════════════════════════════════════════════════════════════
// SegmentState helpers
// ═════════════════════════════════════════════════════════════════════════

void XTSFeedBridge::SegmentState::touchToken(uint32_t token) {
    // Move to back of LRU (most-recently-used)
    lruOrder.removeAll(token);
    lruOrder.append(token);
}

uint32_t XTSFeedBridge::SegmentState::evictLRU() {
    if (lruOrder.isEmpty()) return 0;
    uint32_t oldest = lruOrder.first();
    lruOrder.removeFirst();
    activeSet.remove(oldest);
    return oldest;
}

void XTSFeedBridge::SegmentState::addToken(uint32_t token) {
    activeSet.insert(token);
    lruOrder.removeAll(token);  // Avoid duplicates
    lruOrder.append(token);
}

void XTSFeedBridge::SegmentState::removeToken(uint32_t token) {
    activeSet.remove(token);
    lruOrder.removeAll(token);
    pendingSet.remove(token);
}

// ═════════════════════════════════════════════════════════════════════════
// Singleton
// ═════════════════════════════════════════════════════════════════════════

XTSFeedBridge& XTSFeedBridge::instance() {
    static XTSFeedBridge s_instance;
    return s_instance;
}

XTSFeedBridge::XTSFeedBridge()
    : QObject(nullptr)
{
    m_rateLimitTimer.start();  // Start elapsed timer

    // Batch timer — fires periodically to drain the pending queue
    m_batchTimer = new QTimer(this);
    m_batchTimer->setInterval(m_config.batchIntervalMs);
    connect(m_batchTimer, &QTimer::timeout, this, &XTSFeedBridge::processPendingQueue);
    // Timer is NOT started until feedMode = XTS_ONLY

    // Cooldown timer — one-shot, resumes queue processing after rate limit
    m_cooldownTimer = new QTimer(this);
    m_cooldownTimer->setSingleShot(true);
    connect(m_cooldownTimer, &QTimer::timeout, this, [this]() {
        m_inCooldown = false;
        qDebug() << "[XTSFeedBridge] Cooldown expired, resuming queue processing";
        if (!m_pendingQueue.empty()) {
            m_batchTimer->start();
        }
    });

    qDebug() << "[XTSFeedBridge] Initialized (mode: HYBRID — bridge dormant)";
}

XTSFeedBridge::~XTSFeedBridge() {
    if (m_batchTimer) m_batchTimer->stop();
    if (m_cooldownTimer) m_cooldownTimer->stop();
}

// ═════════════════════════════════════════════════════════════════════════
// Configuration
// ═════════════════════════════════════════════════════════════════════════

void XTSFeedBridge::setFeedMode(FeedMode mode) {
    FeedMode old = m_feedMode.exchange(mode);
    if (old == mode) return;

    qDebug() << "[XTSFeedBridge] Feed mode changed:"
             << (mode == FeedMode::XTS_ONLY ? "XTS_ONLY" : "HYBRID");

    if (mode == FeedMode::XTS_ONLY) {
        // Start the batch timer so pending subscriptions are processed
        if (!m_pendingQueue.empty()) {
            m_batchTimer->start();
        }
    } else {
        // HYBRID — stop the timer, UDP handles everything
        m_batchTimer->stop();
    }

    emit feedModeChanged(mode);
}

void XTSFeedBridge::setMarketDataClient(XTSMarketDataClient* client) {
    QMutexLocker lock(&m_mutex);
    m_mdClient = client;
    if (client) {
        qDebug() << "[XTSFeedBridge] MarketDataClient set"
                 << (client->isLoggedIn() ? "(logged in)" : "(not logged in)");
    }
}

void XTSFeedBridge::setConfig(const Config& config) {
    QMutexLocker lock(&m_mutex);
    m_config = config;
    m_batchTimer->setInterval(config.batchIntervalMs);
    qDebug() << "[XTSFeedBridge] Config updated — globalCap:"
             << config.maxTotalSubscriptions
             << "batchSize:" << config.batchSize
             << "rateLimit:" << config.maxRestCallsPerSec << "/sec"
             << "defaultMsgCode:" << config.defaultMessageCode;
}

XTSFeedBridge::Config XTSFeedBridge::config() const {
    QMutexLocker lock(&m_mutex);
    return m_config;
}

// ═════════════════════════════════════════════════════════════════════════
// Subscription API
// ═════════════════════════════════════════════════════════════════════════

void XTSFeedBridge::requestSubscribe(uint32_t token, int exchangeSegment,
                                      int xtsMessageCode) {
    // In HYBRID mode, do nothing — UDP handles subscriptions
    if (m_feedMode.load() == FeedMode::HYBRID) {
        return;
    }

    QMutexLocker lock(&m_mutex);

    // Check if already subscribed or pending
    auto& seg = m_segments[exchangeSegment];
    if (seg.activeSet.contains(token) || seg.pendingSet.contains(token)) {
        // Already active — just touch LRU to prevent eviction
        if (seg.activeSet.contains(token)) {
            seg.touchToken(token);
        }
        return;
    }

    // Mark as pending
    seg.pendingSet.insert(token);

    // Enqueue
    PendingSubscription ps;
    ps.token = token;
    ps.exchangeSegment = exchangeSegment;
    ps.xtsMessageCode = xtsMessageCode;
    ps.retryCount = 0;
    ps.queuedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_pendingQueue.push(ps);

    // Ensure batch timer is running
    if (!m_batchTimer->isActive() && !m_inCooldown) {
        m_batchTimer->start();
    }

    emitStatsUpdate();
}

void XTSFeedBridge::requestUnsubscribe(uint32_t token, int exchangeSegment,
                                        int xtsMessageCode) {
    if (m_feedMode.load() == FeedMode::HYBRID) return;

    QMutexLocker lock(&m_mutex);

    auto it = m_segments.find(exchangeSegment);
    if (it == m_segments.end()) return;

    auto& seg = it.value();
    if (!seg.activeSet.contains(token)) {
        seg.pendingSet.remove(token);
        return;
    }

    // Remove from active tracking
    seg.removeToken(token);

    // Queue REST unsubscribe
    lock.unlock();
    sendBatchUnsubscribe(exchangeSegment, {token}, xtsMessageCode);

    emitStatsUpdate();
}

// ═════════════════════════════════════════════════════════════════════════
// Batch Queue Processing
// ═════════════════════════════════════════════════════════════════════════

void XTSFeedBridge::processPendingQueue() {
    if (m_inCooldown) return;

    QMutexLocker lock(&m_mutex);

    if (m_pendingQueue.empty()) {
        m_batchTimer->stop();
        return;
    }

    if (!m_mdClient || !m_mdClient->isLoggedIn()) {
        qWarning() << "[XTSFeedBridge] Cannot process queue: MD client not ready";
        return;
    }

    // Rate limit check: reset counter every second
    qint64 now = m_rateLimitTimer.elapsed();
    if (now - m_lastCallTimestamp >= 1000) {
        m_callsThisSecond = 0;
        m_lastCallTimestamp = now;
    }

    if (m_callsThisSecond >= m_config.maxRestCallsPerSec) {
        // Already at rate limit — wait for next second
        return;
    }

    // Check GLOBAL capacity
    int globalActive = totalActiveCount();
    int globalRemaining = m_config.maxTotalSubscriptions - globalActive;

    if (globalRemaining <= 0) {
        // At global cap — need to evict before subscribing
        int needed = qMin(m_config.batchSize, (int)m_pendingQueue.size());
        lock.unlock();
        int evicted = evictTokensIfNeeded(needed);
        lock.relock();
        globalRemaining = m_config.maxTotalSubscriptions - totalActiveCount();
        if (globalRemaining <= 0) {
            qWarning() << "[XTSFeedBridge] Cannot evict enough tokens — queue stalled";
            return;
        }
    }

    // Dequeue up to min(batchSize, globalRemaining) tokens, grouped by (segment, messageCode)
    // Key: (segment << 16) | messageCode
    QMap<int64_t, QVector<uint32_t>> batches;
    QMap<int64_t, int> batchMsgCodes;
    QMap<int64_t, int> batchSegments;
    int dequeued = 0;
    int maxDequeue = qMin(m_config.batchSize, globalRemaining);

    while (!m_pendingQueue.empty() && dequeued < maxDequeue) {
        PendingSubscription ps = m_pendingQueue.front();
        m_pendingQueue.pop();

        auto& seg = m_segments[ps.exchangeSegment];

        // Skip if already subscribed (could happen if queued twice)
        if (seg.activeSet.contains(ps.token)) {
            seg.pendingSet.remove(ps.token);
            continue;
        }

        int64_t batchKey = (static_cast<int64_t>(ps.exchangeSegment) << 16) | ps.xtsMessageCode;
        batches[batchKey].append(ps.token);
        batchMsgCodes[batchKey] = ps.xtsMessageCode;
        batchSegments[batchKey] = ps.exchangeSegment;
        dequeued++;
    }

    // Release lock before making REST calls
    lock.unlock();

    // Fire batched REST subscribe calls per (segment, messageCode)
    for (auto it = batches.begin(); it != batches.end(); ++it) {
        int64_t batchKey = it.key();
        const QVector<uint32_t>& tokens = it.value();
        int segment = batchSegments[batchKey];
        int msgCode = batchMsgCodes[batchKey];
        if (!tokens.isEmpty()) {
            sendBatchSubscribe(segment, tokens, msgCode);
        }
    }

    // Stop timer if queue is drained
    lock.relock();
    if (m_pendingQueue.empty()) {
        m_batchTimer->stop();
    }
}

// ═════════════════════════════════════════════════════════════════════════
// REST API Calls
// ═════════════════════════════════════════════════════════════════════════

void XTSFeedBridge::sendBatchSubscribe(int exchangeSegment, const QVector<uint32_t>& tokens,
                                        int xtsMessageCode) {
    if (!m_mdClient) return;

    // Convert to int64_t vector for XTSMarketDataClient API
    QVector<int64_t> instrumentIDs;
    instrumentIDs.reserve(tokens.size());
    for (uint32_t t : tokens) {
        instrumentIDs.append(static_cast<int64_t>(t));
    }

    qDebug() << "[XTSFeedBridge] REST subscribe — segment:" << exchangeSegment
             << "tokens:" << tokens.size()
             << "msgCode:" << xtsMessageCode
             << "first:" << (tokens.isEmpty() ? 0 : tokens.first());

    // Track rate limit
    {
        QMutexLocker lock(&m_mutex);
        m_callsThisSecond++;
        m_restCallsMade++;
    }

    // Connect to subscription result (one-shot)
    // We use a shared connection to handle the response
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(m_mdClient, &XTSMarketDataClient::subscriptionCompleted,
                    this, [this, tokens, exchangeSegment, xtsMessageCode, conn](bool success, const QString& error) {
        // Disconnect this one-shot handler
        disconnect(*conn);

        QMutexLocker lock(&m_mutex);

        if (success) {
            auto& seg = m_segments[exchangeSegment];
            for (uint32_t token : tokens) {
                seg.addToken(token);
                seg.pendingSet.remove(token);
            }
            qDebug() << "[XTSFeedBridge] ✅ Subscribed" << tokens.size()
                     << "tokens on segment" << exchangeSegment
                     << "(global active:" << totalActiveCount()
                     << "/" << m_config.maxTotalSubscriptions << ")";
        } else {
            m_restCallsFailed++;

            // Check for rate limit (429)
            if (error.contains("429") || error.contains("rate", Qt::CaseInsensitive)
                || error.contains("throttl", Qt::CaseInsensitive)) {
                m_rateLimitHits++;
                qWarning() << "[XTSFeedBridge] ⚠ Rate limit hit! Entering cooldown"
                           << m_config.cooldownOnRateLimitMs << "ms";
                lock.unlock();
                enterCooldown(m_config.cooldownOnRateLimitMs);
                
                // Re-queue the tokens for retry
                QMutexLocker relock(&m_mutex);
                for (uint32_t token : tokens) {
                    PendingSubscription ps;
                    ps.token = token;
                    ps.exchangeSegment = exchangeSegment;
                    ps.xtsMessageCode = xtsMessageCode;
                    ps.retryCount = 1;
                    ps.queuedAtMs = QDateTime::currentMSecsSinceEpoch();
                    m_pendingQueue.push(ps);
                    // Keep in pendingSet
                }
                return;
            }

            // Check for "already subscribed" (not a real error)
            if (error.contains("already subscribed", Qt::CaseInsensitive) ||
                error.contains("e-session-0002")) {
                auto& seg = m_segments[exchangeSegment];
                for (uint32_t token : tokens) {
                    seg.addToken(token);
                    seg.pendingSet.remove(token);
                }
                qDebug() << "[XTSFeedBridge] ℹ Tokens already subscribed on segment"
                         << exchangeSegment;
            } else {
                // Generic failure — retry up to maxRetries
                qWarning() << "[XTSFeedBridge] ❌ Subscribe failed:" << error;
                auto& seg = m_segments[exchangeSegment];
                for (uint32_t token : tokens) {
                    seg.pendingSet.remove(token);
                    // Could re-queue with retry count check here
                }
            }
        }

        lock.unlock();
        emitStatsUpdate();
    });

    // Fire the REST call
    m_mdClient->subscribe(instrumentIDs, exchangeSegment, xtsMessageCode);
}

void XTSFeedBridge::sendBatchUnsubscribe(int exchangeSegment, const QVector<uint32_t>& tokens,
                                          int xtsMessageCode) {
    if (!m_mdClient) return;

    QVector<int64_t> instrumentIDs;
    instrumentIDs.reserve(tokens.size());
    for (uint32_t t : tokens) {
        instrumentIDs.append(static_cast<int64_t>(t));
    }

    qDebug() << "[XTSFeedBridge] REST unsubscribe — segment:" << exchangeSegment
             << "tokens:" << tokens.size()
             << "msgCode:" << xtsMessageCode;

    m_mdClient->unsubscribe(instrumentIDs, exchangeSegment, xtsMessageCode);
}

// ═════════════════════════════════════════════════════════════════════════
// Rate Limit & Cooldown
// ═════════════════════════════════════════════════════════════════════════

void XTSFeedBridge::enterCooldown(int cooldownMs) {
    m_inCooldown = true;
    m_batchTimer->stop();
    m_cooldownTimer->start(cooldownMs);
    emit rateLimitHit(cooldownMs);
}

// ═════════════════════════════════════════════════════════════════════════
// LRU Eviction
// ═════════════════════════════════════════════════════════════════════════

int XTSFeedBridge::evictTokensIfNeeded(int needed) {
    QMutexLocker lock(&m_mutex);

    int evicted = 0;
    QMap<int, QVector<uint32_t>> evictedBySegment;

    // Round-robin across segments: pick the globally-oldest token each iteration
    while (evicted < needed) {
        bool anyEvicted = false;
        for (auto it = m_segments.begin(); it != m_segments.end(); ++it) {
            if (evicted >= needed) break;
            auto& seg = it.value();
            if (!seg.lruOrder.isEmpty()) {
                uint32_t victim = seg.evictLRU();
                if (victim > 0) {
                    evictedBySegment[it.key()].append(victim);
                    evicted++;
                    m_totalEvicted++;
                    anyEvicted = true;
                }
            }
        }
        if (!anyEvicted) break;
    }

    if (evicted > 0) {
        qDebug() << "[XTSFeedBridge] Evicted" << evicted << "LRU tokens globally";
    }

    // Fire unsubscribe REST calls per segment (outside lock)
    lock.unlock();
    for (auto it = evictedBySegment.begin(); it != evictedBySegment.end(); ++it) {
        if (!it.value().isEmpty()) {
            emit tokensEvicted(it.value().size(), it.key());
            sendBatchUnsubscribe(it.key(), it.value(), m_config.defaultMessageCode);
        }
    }

    return evicted;
}

// ═════════════════════════════════════════════════════════════════════════
// Unsubscribe All Except Candles (for XTS→UDP migration)
// ═════════════════════════════════════════════════════════════════════════

void XTSFeedBridge::unsubscribeAllExceptCandles() {
    QMutexLocker lock(&m_mutex);

    // Collect all (segment, tokens) pairs to unsubscribe,
    // EXCLUDING 1505 (candle) subscriptions.
    // We track subscriptions by segment in m_segments, but we don't
    // separately track per-messageCode. So we unsubscribe ALL active tokens
    // from the non-candle message codes (1501, 1502, 1512).
    // 1505 candle subscriptions are managed separately by chart windows
    // and are never added to m_segments tracking.

    QMap<int, QVector<uint32_t>> toUnsubscribe;  // segment -> tokens
    int totalTokens = 0;

    for (auto it = m_segments.begin(); it != m_segments.end(); ++it) {
        int segment = it.key();
        SegmentState& state = it.value();

        // Collect all active tokens for this segment
        QVector<uint32_t> tokens;
        tokens.reserve(state.activeSet.size());
        for (uint32_t token : state.activeSet) {
            tokens.append(token);
        }

        if (!tokens.isEmpty()) {
            toUnsubscribe[segment] = tokens;
            totalTokens += tokens.size();
        }

        // Clear the segment state
        state.activeSet.clear();
        state.lruOrder.clear();
        state.pendingSet.clear();
    }

    // Also drain the pending queue
    while (!m_pendingQueue.empty()) {
        m_pendingQueue.pop();
    }
    m_batchTimer->stop();

    qDebug() << "[XTSFeedBridge] unsubscribeAllExceptCandles:"
             << totalTokens << "tokens across"
             << toUnsubscribe.size() << "segments";

    // Release lock before REST calls
    lock.unlock();

    // Fire unsubscribe REST calls per segment
    for (auto it = toUnsubscribe.begin(); it != toUnsubscribe.end(); ++it) {
        sendBatchUnsubscribe(it.key(), it.value(), m_config.defaultMessageCode);
    }

    emitStatsUpdate();
}

// ═════════════════════════════════════════════════════════════════════════
// Bulk Subscribe All (for UDP→XTS migration — single API call per segment)
// ═════════════════════════════════════════════════════════════════════════

int XTSFeedBridge::bulkSubscribeAll(const std::vector<std::pair<int, uint32_t>>& tokens) {
    if (tokens.empty()) return 0;

    if (!m_mdClient || !m_mdClient->isLoggedIn()) {
        qWarning() << "[XTSFeedBridge] Cannot bulk subscribe: MD client not ready";
        return 0;
    }

    const int limit = m_config.maxTotalSubscriptions;
    
    // Group tokens by segment, respecting the global limit
    QMap<int, QVector<uint32_t>> bySegment;
    int totalQueued = 0;

    for (const auto& [segment, token] : tokens) {
        if (totalQueued >= limit) {
            qWarning() << "[XTSFeedBridge] bulkSubscribeAll: Hit limit of"
                       << limit << "— remaining tokens will not receive live XTS data";
            break;
        }
        bySegment[segment].append(token);
        totalQueued++;
    }

    qDebug() << "[XTSFeedBridge] bulkSubscribeAll:" << totalQueued
             << "tokens across" << bySegment.size() << "segments"
             << "(limit:" << limit << ")";

    // Clear pending queue and existing state (we're replacing everything)
    {
        QMutexLocker lock(&m_mutex);
        while (!m_pendingQueue.empty()) {
            m_pendingQueue.pop();
        }
        for (auto& seg : m_segments) {
            seg.activeSet.clear();
            seg.pendingSet.clear();
            seg.lruOrder.clear();
        }
    }

    // Fire one REST call per segment
    for (auto it = bySegment.begin(); it != bySegment.end(); ++it) {
        int segment = it.key();
        const QVector<uint32_t>& segTokens = it.value();

        // Mark as pending
        {
            QMutexLocker lock(&m_mutex);
            auto& seg = m_segments[segment];
            for (uint32_t token : segTokens) {
                seg.pendingSet.insert(token);
            }
        }

        // Send the single bulk REST call for this segment
        sendBatchSubscribe(segment, segTokens, m_config.defaultMessageCode);
    }

    emitStatsUpdate();
    return totalQueued;
}

// ═════════════════════════════════════════════════════════════════════════
// Statistics
// ═════════════════════════════════════════════════════════════════════════

XTSFeedBridge::Stats XTSFeedBridge::getStats() const {
    QMutexLocker lock(&m_mutex);
    Stats stats;
    stats.totalEvicted    = m_totalEvicted;
    stats.restCallsMade   = m_restCallsMade;
    stats.restCallsFailed = m_restCallsFailed;
    stats.rateLimitHits   = m_rateLimitHits;
    stats.totalPending    = static_cast<int>(m_pendingQueue.size());
    stats.totalCapacity   = m_config.maxTotalSubscriptions;

    for (auto it = m_segments.begin(); it != m_segments.end(); ++it) {
        int seg = it.key();
        const SegmentState& state = it.value();
        stats.perSegmentCount[seg] = state.activeSet.size();
        stats.totalSubscribed += state.activeSet.size();
    }
    stats.totalCapacity = m_config.maxTotalSubscriptions;

    return stats;
}

void XTSFeedBridge::dumpStats() const {
    Stats s = getStats();
    qDebug() << "╔═══════════════════════════════════════════════════════╗";
    qDebug() << "║           XTSFeedBridge Statistics                    ║";
    qDebug() << "╠═══════════════════════════════════════════════════════╣";
    qDebug() << "║ Mode:" << (m_feedMode.load() == FeedMode::XTS_ONLY ? "XTS_ONLY" : "HYBRID");
    qDebug() << "║ Subscribed:" << s.totalSubscribed << "/" << s.totalCapacity
             << " Pending:" << s.totalPending;
    qDebug() << "║ REST calls:" << s.restCallsMade << " Failed:" << s.restCallsFailed;
    qDebug() << "║ Rate limit hits:" << s.rateLimitHits << " Evictions:" << s.totalEvicted;
    for (auto it = s.perSegmentCount.begin(); it != s.perSegmentCount.end(); ++it) {
        qDebug() << "║   Segment" << it.key() << ":" << it.value();
    }
    qDebug() << "╚═══════════════════════════════════════════════════════╝";
}

int XTSFeedBridge::totalActiveCount() const {
    int total = 0;
    for (auto it = m_segments.constBegin(); it != m_segments.constEnd(); ++it) {
        total += it.value().activeSet.size();
    }
    return total;
}

void XTSFeedBridge::emitStatsUpdate() {
    Stats s = getStats();
    emit subscriptionStatsChanged(s.totalSubscribed, s.totalPending, s.totalCapacity);
}

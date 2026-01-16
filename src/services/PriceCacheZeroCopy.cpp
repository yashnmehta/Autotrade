#include "services/PriceCacheZeroCopy.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <chrono>
#include <cstring>
#include <new>  // for std::align_val_t

namespace PriceCacheTypes {

// ============================================================================
// Singleton Instance
// ============================================================================

PriceCacheZeroCopy& PriceCacheZeroCopy::getInstance() {
    static PriceCacheZeroCopy instance;
    return instance;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

PriceCacheZeroCopy::PriceCacheZeroCopy()
    : m_initialized(false)
    , m_nseCmArray(nullptr)
    , m_nseFoArray(nullptr)
    , m_bseCmArray(nullptr)
    , m_bseFoArray(nullptr)
    , m_nseCmCount(0)
    , m_nseFoCount(0)
    , m_bseCmCount(0)
    , m_bsFoCount(0)
    , m_totalSubscriptions(0)
{
    qDebug() << "[PriceCacheZeroCopy] Constructor called";
}

PriceCacheZeroCopy::~PriceCacheZeroCopy() {
    qDebug() << "[PriceCacheZeroCopy] Destructor called - cleaning up memory arrays";
    
    // Free all arrays
    if (m_nseCmArray) freeSegmentArray(m_nseCmArray);
    if (m_nseFoArray) freeSegmentArray(m_nseFoArray);
    if (m_bseCmArray) freeSegmentArray(m_bseCmArray);
    if (m_bseFoArray) freeSegmentArray(m_bseFoArray);
}

// ============================================================================
// Initialization
// ============================================================================

bool PriceCacheZeroCopy::initialize(
    const std::unordered_map<uint32_t, uint32_t>& nseCmTokens,
    const std::unordered_map<uint32_t, uint32_t>& nseFoTokens,
    const std::unordered_map<uint32_t, uint32_t>& bseCmTokens,
    const std::unordered_map<uint32_t, uint32_t>& bseFoTokens)
{
    if (m_initialized.load()) {
        qWarning() << "[PriceCacheZeroCopy] Already initialized!";
        return false;
    }
    
    qDebug() << "[PriceCacheZeroCopy] ========================================";
    qDebug() << "[PriceCacheZeroCopy] Initializing Zero-Copy Price Cache";
    qDebug() << "[PriceCacheZeroCopy] ========================================";
    
    // Store token counts
    m_nseCmCount = nseCmTokens.size();
    m_nseFoCount = nseFoTokens.size();
    m_bseCmCount = bseCmTokens.size();
    m_bsFoCount = bseFoTokens.size();
    
    qDebug() << "[PriceCacheZeroCopy] Token counts:";
    qDebug() << "[PriceCacheZeroCopy]   NSE CM:" << m_nseCmCount << "tokens";
    qDebug() << "[PriceCacheZeroCopy]   NSE FO:" << m_nseFoCount << "tokens";
    qDebug() << "[PriceCacheZeroCopy]   BSE CM:" << m_bseCmCount << "tokens";
    qDebug() << "[PriceCacheZeroCopy]   BSE FO:" << m_bsFoCount << "tokens";
    
    // Calculate memory requirements
    size_t nseCmMemory = m_nseCmCount * sizeof(ConsolidatedMarketData);
    size_t nseFoMemory = m_nseFoCount * sizeof(ConsolidatedMarketData);
    size_t bseCmMemory = m_bseCmCount * sizeof(ConsolidatedMarketData);
    size_t bseFoMemory = m_bsFoCount * sizeof(ConsolidatedMarketData);
    size_t totalMemory = nseCmMemory + nseFoMemory + bseCmMemory + bseFoMemory;
    
    qDebug() << "[PriceCacheZeroCopy] Memory allocation:";
    qDebug() << "[PriceCacheZeroCopy]   NSE CM:" << (nseCmMemory / 1024 / 1024) << "MB";
    qDebug() << "[PriceCacheZeroCopy]   NSE FO:" << (nseFoMemory / 1024 / 1024) << "MB";
    qDebug() << "[PriceCacheZeroCopy]   BSE CM:" << (bseCmMemory / 1024 / 1024) << "MB";
    qDebug() << "[PriceCacheZeroCopy]   BSE FO:" << (bseFoMemory / 1024 / 1024) << "MB";
    qDebug() << "[PriceCacheZeroCopy]   TOTAL:" << (totalMemory / 1024 / 1024) << "MB";
    
    // Allocate arrays
    try {
        if (m_nseCmCount > 0) {
            m_nseCmArray = allocateSegmentArray(m_nseCmCount);
            m_nseCmTokenMap = nseCmTokens;
            qDebug() << "[PriceCacheZeroCopy] ✓ NSE CM array allocated";
        }
        
        if (m_nseFoCount > 0) {
            m_nseFoArray = allocateSegmentArray(m_nseFoCount);
            m_nseFoTokenMap = nseFoTokens;
            qDebug() << "[PriceCacheZeroCopy] ✓ NSE FO array allocated";
        }
        
        if (m_bseCmCount > 0) {
            m_bseCmArray = allocateSegmentArray(m_bseCmCount);
            m_bseCmTokenMap = bseCmTokens;
            qDebug() << "[PriceCacheZeroCopy] ✓ BSE CM array allocated";
        }
        
        if (m_bsFoCount > 0) {
            m_bseFoArray = allocateSegmentArray(m_bsFoCount);
            m_bseFoTokenMap = bseFoTokens;
            qDebug() << "[PriceCacheZeroCopy] ✓ BSE FO array allocated";
        }
        
    } catch (const std::bad_alloc& e) {
        qCritical() << "[PriceCacheZeroCopy] ❌ Memory allocation failed:" << e.what();
        
        // Cleanup on failure
        if (m_nseCmArray) freeSegmentArray(m_nseCmArray);
        if (m_nseFoArray) freeSegmentArray(m_nseFoArray);
        if (m_bseCmArray) freeSegmentArray(m_bseCmArray);
        if (m_bseFoArray) freeSegmentArray(m_bseFoArray);
        
        return false;
    }
    
    // Mark as initialized
    m_initialized.store(true);
    
    qDebug() << "[PriceCacheZeroCopy] ========================================";
    qDebug() << "[PriceCacheZeroCopy] ✓ Zero-Copy Price Cache Initialized";
    qDebug() << "[PriceCacheZeroCopy] ========================================";
    qDebug() << "[PriceCacheZeroCopy] Architecture Features:";
    qDebug() << "[PriceCacheZeroCopy]   ⚡ Zero-copy memory arrays";
    qDebug() << "[PriceCacheZeroCopy]   ⚡ Cache-aligned structures (512 bytes)";
    qDebug() << "[PriceCacheZeroCopy]   ⚡ Direct UDP writes";
    qDebug() << "[PriceCacheZeroCopy]   ⚡ Pointer-based subscriptions";
    qDebug() << "[PriceCacheZeroCopy]   ⚡ Lock-free reads";
    qDebug() << "[PriceCacheZeroCopy] ========================================";
    
    return true;
}

// ============================================================================
// Memory Management
// ============================================================================

ConsolidatedMarketData* PriceCacheZeroCopy::allocateSegmentArray(uint32_t tokenCount) {
    if (tokenCount == 0) {
        return nullptr;
    }
    
    // Allocate cache-aligned memory (64-byte alignment)
    // Each structure is 512 bytes (8 cache lines)
    size_t totalSize = tokenCount * sizeof(ConsolidatedMarketData);
    
    // Use aligned allocation for better CPU cache performance
    // Note: MinGW doesn't support std::aligned_alloc, use _aligned_malloc instead
    #ifdef _WIN32
    void* memory = _aligned_malloc(totalSize, 64);
    #else
    void* memory = std::aligned_alloc(64, totalSize);
    #endif
    
    if (!memory) {
        throw std::bad_alloc();
    }
    
    // Zero-initialize the memory
    std::memset(memory, 0, totalSize);
    
    // Placement new to construct objects
    ConsolidatedMarketData* array = static_cast<ConsolidatedMarketData*>(memory);
    
    // Initialize each element (call default constructor)
    for (uint32_t i = 0; i < tokenCount; ++i) {
        new (&array[i]) ConsolidatedMarketData();
    }
    
    return array;
}

void PriceCacheZeroCopy::freeSegmentArray(ConsolidatedMarketData* array) {
    if (array) {
        // No need to call destructors (trivial destructor)
        #ifdef _WIN32
        _aligned_free(array);
        #else
        std::free(array);
        #endif
    }
}

// ============================================================================
// Subscription (REST-like)
// ============================================================================

SubscriptionResult PriceCacheZeroCopy::subscribe(uint32_t token, MarketSegment segment) {
    SubscriptionResult result;
    
    if (!m_initialized.load()) {
        result.success = false;
        result.errorMessage = "PriceCache not initialized";
        qWarning() << "[PriceCacheZeroCopy] Subscribe failed - not initialized";
        return result;
    }
    
    // Get token index and pointer
    uint32_t tokenIndex = 0;
    if (!getTokenIndex(token, segment, tokenIndex)) {
        result.success = false;
        result.errorMessage = QString("Token %1 not found in segment %2")
                                .arg(token)
                                .arg(static_cast<int>(segment));
        return result;
    }
    
    // Calculate pointer
    ConsolidatedMarketData* dataPtr = calculatePointer(tokenIndex, segment);
    
    if (!dataPtr) {
        result.success = false;
        result.errorMessage = "Failed to calculate data pointer";
        qWarning() << "[PriceCacheZeroCopy] Subscribe failed - null pointer";
        return result;
    }
    
    // Read current snapshot (atomic read)
    // Note: For true thread-safety with concurrent writes, UDP receiver
    // should use atomic operations or lock-free queues
    ConsolidatedMarketData snapshot = *dataPtr;
    
    // Mark as subscribed
    dataPtr->isSubscribed = true;
    
    // Fill result
    result.dataPointer = dataPtr;
    result.tokenIndex = tokenIndex;
    result.snapshot = snapshot;
    result.success = true;
    
    // Update statistics
    m_totalSubscriptions.fetch_add(1);
    
    qDebug() << "[PriceCacheZeroCopy] ✓ Subscribed to token" << token 
             << "segment" << static_cast<int>(segment)
             << "index" << tokenIndex
             << "pointer" << static_cast<void*>(dataPtr);
    
    return result;
}

void PriceCacheZeroCopy::subscribeAsync(uint32_t token, MarketSegment segment, const QString& requesterId) {
    // Perform synchronous subscription
    SubscriptionResult result = subscribe(token, segment);
    
    // Emit signal with result (Qt will handle thread-safe delivery)
    // This signal will be delivered to the main UI thread via Qt's event loop
    qDebug() << "[PriceCacheZeroCopy] Emitting subscriptionReady signal for token" << token 
             << "requesterId" << requesterId
             << "success" << result.success;
    
    emit subscriptionReady(
        requesterId,
        token,
        segment,
        result.dataPointer,
        result.snapshot,
        result.success,
        result.errorMessage
    );
}

void PriceCacheZeroCopy::unsubscribe(uint32_t token, MarketSegment segment) {
    if (!m_initialized.load()) {
        return;
    }
    
    uint32_t tokenIndex = 0;
    if (getTokenIndex(token, segment, tokenIndex)) {
        ConsolidatedMarketData* dataPtr = calculatePointer(tokenIndex, segment);
        if (dataPtr) {
            dataPtr->isSubscribed = false;
            qDebug() << "[PriceCacheZeroCopy] Unsubscribed from token" << token;
        }
    }
}

// ============================================================================
// On-Demand Query
// ============================================================================

ConsolidatedMarketData PriceCacheZeroCopy::getLatestData(uint32_t token, MarketSegment segment) {
    ConsolidatedMarketData emptyData;
    
    if (!m_initialized.load()) {
        qWarning() << "[PriceCacheZeroCopy] getLatestData failed - not initialized";
        return emptyData;
    }
    
    uint32_t tokenIndex = 0;
    if (!getTokenIndex(token, segment, tokenIndex)) {
        return emptyData;
    }
    
    ConsolidatedMarketData* dataPtr = calculatePointer(tokenIndex, segment);
    if (!dataPtr) {
        return emptyData;
    }
    
    // Return snapshot
    return *dataPtr;
}

// ============================================================================
// Direct Access (for UDP Receivers)
// ============================================================================

ConsolidatedMarketData* PriceCacheZeroCopy::getSegmentBaseAddress(MarketSegment segment) {
    if (!m_initialized.load()) {
        return nullptr;
    }
    
    switch (segment) {
        case MarketSegment::NSE_CM:
            return m_nseCmArray;
        case MarketSegment::NSE_FO:
            return m_nseFoArray;
        case MarketSegment::BSE_CM:
            return m_bseCmArray;
        case MarketSegment::BSE_FO:
            return m_bseFoArray;
        default:
            return nullptr;
    }
}

bool PriceCacheZeroCopy::getTokenIndex(uint32_t token, MarketSegment segment, uint32_t& outIndex) {
    const std::unordered_map<uint32_t, uint32_t>* tokenMap = nullptr;
    
    switch (segment) {
        case MarketSegment::NSE_CM:
            tokenMap = &m_nseCmTokenMap;
            break;
        case MarketSegment::NSE_FO:
            tokenMap = &m_nseFoTokenMap;
            break;
        case MarketSegment::BSE_CM:
            tokenMap = &m_bseCmTokenMap;
            break;
        case MarketSegment::BSE_FO:
            tokenMap = &m_bseFoTokenMap;
            break;
        default:
            return false;
    }
    
    auto it = tokenMap->find(token);
    if (it == tokenMap->end()) {
        return false;
    }
    
    outIndex = it->second;
    return true;
}

// ============================================================================
// Helper Methods
// ============================================================================

ConsolidatedMarketData* PriceCacheZeroCopy::calculatePointer(uint32_t tokenIndex, MarketSegment segment) {
    ConsolidatedMarketData* baseAddress = getSegmentBaseAddress(segment);
    
    if (!baseAddress) {
        return nullptr;
    }
    
    // Simple pointer arithmetic: baseAddress + (tokenIndex * sizeof(ConsolidatedMarketData))
    // Since array is already typed, just use array indexing
    return &baseAddress[tokenIndex];
}

// ============================================================================
// Statistics
// ============================================================================

PriceCacheZeroCopy::CacheStats PriceCacheZeroCopy::getStats() const {
    CacheStats stats;
    
    stats.nseCmTokenCount = m_nseCmCount;
    stats.nseFoTokenCount = m_nseFoCount;
    stats.bseCmTokenCount = m_bseCmCount;
    stats.bseFoTokenCount = m_bsFoCount;
    
    stats.totalMemoryBytes = 
        (m_nseCmCount + m_nseFoCount + m_bseCmCount + m_bsFoCount) * 
        sizeof(ConsolidatedMarketData);
    
    stats.totalSubscriptions = m_totalSubscriptions.load();
    
    return stats;
}

void PriceCacheZeroCopy::update(const XTS::Tick& tick)
{
    if (!m_initialized.load()) return;

    MarketSegment segment = static_cast<MarketSegment>(tick.exchangeSegment);
    uint32_t tokenIndex = 0;
    if (!getTokenIndex(tick.exchangeInstrumentID, segment, tokenIndex)) {
        return; // Token not in our cache maps
    }

    ConsolidatedMarketData* data = calculatePointer(tokenIndex, segment);
    if (!data) return;

    // Update core fields (convert rupees to paise)
    if (tick.lastTradedPrice > 0) {
        data->lastTradedPrice = static_cast<int32_t>(tick.lastTradedPrice * 100.0 + 0.5);
    }
    
    data->lastTradeQuantity = static_cast<int32_t>(tick.lastTradedQuantity);
    data->volumeTradedToday = tick.volume;

    // Update OHLC
    if (tick.open > 0) data->openPrice = static_cast<int32_t>(tick.open * 100.0 + 0.5);
    if (tick.high > 0) data->highPrice = static_cast<int32_t>(tick.high * 100.0 + 0.5);
    if (tick.low > 0) data->lowPrice = static_cast<int32_t>(tick.low * 100.0 + 0.5);
    if (tick.close > 0) data->closePrice = static_cast<int32_t>(tick.close * 100.0 + 0.5);

    // Update Bid/Ask (Best Level)
    if (tick.bidPrice > 0) {
        data->bidPrice[0] = static_cast<int32_t>(tick.bidPrice * 100.0 + 0.5);
        data->bidQuantity[0] = static_cast<int64_t>(tick.bidQuantity);
    }
    if (tick.askPrice > 0) {
        data->askPrice[0] = static_cast<int32_t>(tick.askPrice * 100.0 + 0.5);
        data->askQuantity[0] = static_cast<int64_t>(tick.askQuantity);
    }

    // Update Depth if available
    for (int i = 0; i < 5; ++i) {
        if (tick.bidDepth[i].price > 0) {
            data->bidPrice[i] = static_cast<int32_t>(tick.bidDepth[i].price * 100.0 + 0.5);
            data->bidQuantity[i] = static_cast<int64_t>(tick.bidDepth[i].quantity);
            data->bidOrders[i] = static_cast<int16_t>(tick.bidDepth[i].orders);
        }
        if (tick.askDepth[i].price > 0) {
            data->askPrice[i] = static_cast<int32_t>(tick.askDepth[i].price * 100.0 + 0.5);
            data->askQuantity[i] = static_cast<int64_t>(tick.askDepth[i].quantity);
            data->askOrders[i] = static_cast<int16_t>(tick.askDepth[i].orders);
        }
    }

    data->totalBuyQuantity = static_cast<int64_t>(tick.totalBuyQuantity);
    data->totalSellQuantity = static_cast<int64_t>(tick.totalSellQuantity);
    
    if (tick.averagePrice > 0) {
        data->averageTradePrice = static_cast<int32_t>(tick.averagePrice * 100.0 + 0.5);
    }
}

void PriceCacheZeroCopy::update(const UDP::MarketTick& tick)
{
    if (!m_initialized.load()) {
        static int warningCount = 0;
        if (warningCount++ < 5) {
            qWarning() << "[PriceCacheZeroCopy] update() called but cache NOT INITIALIZED! Segment:" 
                       << tick.exchangeSegment << "Token:" << tick.token;
        }
        return;
    }

    MarketSegment segment = static_cast<MarketSegment>(tick.exchangeSegment);
    uint32_t tokenIndex = 0;
    if (!getTokenIndex(tick.token, segment, tokenIndex)) {
        static int notFoundCount = 0;
        if (notFoundCount++ < 10) {
            qWarning() << "[PriceCacheZeroCopy] Token not found in map! Segment:" 
                       << tick.exchangeSegment << "Token:" << tick.token;
        }
        return;
    }

    ConsolidatedMarketData* data = calculatePointer(tokenIndex, segment);
    if (!data) {
        qWarning() << "[PriceCacheZeroCopy] calculatePointer returned nullptr! Segment:" 
                   << tick.exchangeSegment << "Token:" << tick.token << "Index:" << tokenIndex;
        return;
    }

    // Debug first few updates
    static int updateCount = 0;
    if (updateCount++ < 20) {
        qDebug() << "[PriceCacheZeroCopy] UPDATE #" << updateCount << "- Segment:" << tick.exchangeSegment 
                 << "Token:" << tick.token << "LTP:" << tick.ltp << "Vol:" << tick.volume;
    }

    // ========== MESSAGE-CODE-AWARE MERGE LOGIC ==========
    
    // Update LTP and LTQ (Message Code 2101 - Trade)
    if (tick.ltp > 0) {
        data->lastTradedPrice = static_cast<int32_t>(tick.ltp * 100.0 + 0.5);
        data->lastTradeQuantity = static_cast<int32_t>(tick.ltq);
    }
    
    // Volume is CUMULATIVE - only increase, never decrease
    if (tick.volume > data->volumeTradedToday) {
        data->volumeTradedToday = tick.volume;
    }

    // OHLC PROTECTION: high never decreases, low never increases
    if (tick.open > 0) {
        data->openPrice = static_cast<int32_t>(tick.open * 100.0 + 0.5);
    }
    if (tick.high > 0) {
        int32_t newHigh = static_cast<int32_t>(tick.high * 100.0 + 0.5);
        if (newHigh > data->highPrice) {  // High only increases
            data->highPrice = newHigh;
        }
    }
    if (tick.low > 0) {
        int32_t newLow = static_cast<int32_t>(tick.low * 100.0 + 0.5);
        if (data->lowPrice == 0 || newLow < data->lowPrice) {  // Low only decreases
            data->lowPrice = newLow;
        }
    }
    if (tick.prevClose > 0) {
        data->closePrice = static_cast<int32_t>(tick.prevClose * 100.0 + 0.5);
    }

    // Depth Merge (Message Code 2001/7207 - Quote/Depth)
    // Only update if new data present, preserve existing otherwise
    for (int i = 0; i < 5; ++i) {
        if (tick.bids[i].price > 0) {
            data->bidPrice[i] = static_cast<int32_t>(tick.bids[i].price * 100.0 + 0.5);
            data->bidQuantity[i] = static_cast<int64_t>(tick.bids[i].quantity);
            data->bidOrders[i] = static_cast<int16_t>(tick.bids[i].orders);
        }
        // Don't clear bid if not in packet - preserve existing
        
        if (tick.asks[i].price > 0) {
            data->askPrice[i] = static_cast<int32_t>(tick.asks[i].price * 100.0 + 0.5);
            data->askQuantity[i] = static_cast<int64_t>(tick.asks[i].quantity);
            data->askOrders[i] = static_cast<int16_t>(tick.asks[i].orders);
        }
        // Don't clear ask if not in packet - preserve existing
    }

    // Update totals if present
    if (tick.totalBidQty > 0) {
        data->totalBuyQuantity = static_cast<int64_t>(tick.totalBidQty);
    }
    if (tick.totalAskQty > 0) {
        data->totalSellQuantity = static_cast<int64_t>(tick.totalAskQty);
    }
    
    // Update ATP if present
    if (tick.atp > 0) {
        data->averageTradePrice = static_cast<int32_t>(tick.atp * 100.0 + 0.5);
    }
    
    // Update timestamp for staleness tracking
    data->lastUpdateTime = tick.timestampEmitted;
}

// ============================================================================
// Debug / Export Functions
// ============================================================================

void PriceCacheZeroCopy::exportCacheToFile(const QString& filePath, uint32_t maxTokens)
{
    if (!m_initialized.load()) {
        qWarning() << "[PriceCacheZeroCopy] Cannot export - cache not initialized";
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[PriceCacheZeroCopy] Failed to open file for export:" << filePath;
        return;
    }

    QTextStream out(&file);
    out << "========================================\n";
    out << "PriceCacheZeroCopy Export\n";
    out << "========================================\n\n";
    
    auto exportSegment = [&](const QString& segmentName, MarketSegment segment, 
                             ConsolidatedMarketData* array, uint32_t count,
                             const std::unordered_map<uint32_t, uint32_t>& tokenMap) {
        out << "\n" << segmentName << " (Total: " << count << " tokens)\n";
        out << "-----------------------------------\n";
        
        uint32_t exported = 0;
        uint32_t activeCount = 0;
        
        for (const auto& pair : tokenMap) {
            uint32_t token = pair.first;
            uint32_t index = pair.second;
            
            if (maxTokens > 0 && exported >= maxTokens) break;
            
            ConsolidatedMarketData* data = &array[index];
            if (data->lastTradedPrice > 0) {
                activeCount++;
                out << QString("Token %1 (Index %2): LTP=₹%3 Vol=%4 O=%5 H=%6 L=%7 C=%8 Time=%9\n")
                    .arg(token)
                    .arg(index)
                    .arg(data->lastTradedPrice / 100.0, 0, 'f', 2)
                    .arg(data->volumeTradedToday)
                    .arg(data->openPrice / 100.0, 0, 'f', 2)
                    .arg(data->highPrice / 100.0, 0, 'f', 2)
                    .arg(data->lowPrice / 100.0, 0, 'f', 2)
                    .arg(data->closePrice / 100.0, 0, 'f', 2)
                    .arg(data->lastUpdateTime);
                exported++;
            }
        }
        
        out << QString("\nActive tokens in %1: %2 / %3 (%4%)\n")
            .arg(segmentName)
            .arg(activeCount)
            .arg(count)
            .arg(count > 0 ? (activeCount * 100.0 / count) : 0.0, 0, 'f', 1);
    };
    
    // Export each segment
    if (m_nseCmCount > 0) {
        exportSegment("NSE CM", MarketSegment::NSE_CM, m_nseCmArray, m_nseCmCount, m_nseCmTokenMap);
    }
    if (m_nseFoCount > 0) {
        exportSegment("NSE FO", MarketSegment::NSE_FO, m_nseFoArray, m_nseFoCount, m_nseFoTokenMap);
    }
    if (m_bseCmCount > 0) {
        exportSegment("BSE CM", MarketSegment::BSE_CM, m_bseCmArray, m_bseCmCount, m_bseCmTokenMap);
    }
    if (m_bsFoCount > 0) {
        exportSegment("BSE FO", MarketSegment::BSE_FO, m_bseFoArray, m_bsFoCount, m_bseFoTokenMap);
    }
    
    out << "\n========================================\n";
    out << "Export Complete\n";
    out << "========================================\n";
    
    file.close();
    qDebug() << "[PriceCacheZeroCopy] Cache exported to:" << filePath;
}

uint32_t PriceCacheZeroCopy::getActiveTokenCount(MarketSegment segment) const
{
    if (!m_initialized.load()) return 0;

    uint32_t activeCount = 0;
    ConsolidatedMarketData* array = nullptr;
    uint32_t count = 0;

    switch (segment) {
        case MarketSegment::NSE_CM:
            array = m_nseCmArray;
            count = m_nseCmCount;
            break;
        case MarketSegment::NSE_FO:
            array = m_nseFoArray;
            count = m_nseFoCount;
            break;
        case MarketSegment::BSE_CM:
            array = m_bseCmArray;
            count = m_bseCmCount;
            break;
        case MarketSegment::BSE_FO:
            array = m_bseFoArray;
            count = m_bsFoCount;
            break;
        default:
            return 0;
    }

    if (!array) return 0;

    for (uint32_t i = 0; i < count; ++i) {
        if (array[i].lastTradedPrice > 0) {
            activeCount++;
        }
    }

    return activeCount;
}

} // namespace PriceCacheTypes



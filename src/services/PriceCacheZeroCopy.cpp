#include "services/PriceCacheZeroCopy.h"
#include <QDebug>
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

} // namespace PriceCacheTypes



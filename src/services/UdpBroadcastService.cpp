#include "services/UdpBroadcastService.h"
#include "nsefo_callback.h"
#include "nsecm_callback.h"
#include "nsefo_price_store.h"
#include "nsecm_price_store.h"
#include "bse_price_store.h"
#include "services/FeedHandler.h"
#include "services/GreeksCalculationService.h"
#include "utils/LatencyTracker.h"
#include "data/PriceStoreGateway.h"
#include <QMetaObject>
#include <QDebug>
#include <thread>
#include <iostream>
#include <cstring>


// ========== CONVERSION HELPERS: Native Protocol → UDP::MarketTick ==========

namespace {

// Convert NSE FO Unified State to UDP::MarketTick
UDP::MarketTick convertNseFoUnified(const nsefo::UnifiedTokenState& data) {
    UDP::MarketTick tick(UDP::ExchangeSegment::NSEFO, data.token);
    tick.ltp = data.ltp;
    tick.ltq = data.lastTradeQty;
    tick.volume = data.volume;
    tick.open = data.open;
    tick.high = data.high;
    tick.low = data.low;
    tick.prevClose = data.close;
    tick.atp = data.avgPrice;
    
    // Depth
    for (int i = 0; i < 5; i++) {
        tick.bids[i] = UDP::DepthLevel(data.bids[i].price, data.bids[i].quantity, data.bids[i].orders);
        tick.asks[i] = UDP::DepthLevel(data.asks[i].price, data.asks[i].quantity, data.asks[i].orders);
    }
    
    tick.totalBidQty = data.totalBuyQty;
    tick.totalAskQty = data.totalSellQty;
    
    tick.openInterest = data.openInterest;
    
    tick.refNo = 0; 
    tick.timestampUdpRecv = data.lastPacketTimestamp;
    tick.timestampParsed = data.lastPacketTimestamp;
    tick.timestampEmitted = LatencyTracker::now();
    tick.messageType = 7200; 
    return tick;
}

// Convert NSE FO Market Depth to UDP::MarketTick
UDP::MarketTick convertNseFoDepth(const nsefo::MarketDepthData& data) {
    UDP::MarketTick tick(UDP::ExchangeSegment::NSEFO, data.token);
    
    // 5-level depth
    for (int i = 0; i < 5; i++) {
        tick.bids[i] = UDP::DepthLevel(data.bids[i].price, data.bids[i].quantity, data.bids[i].orders);
        tick.asks[i] = UDP::DepthLevel(data.asks[i].price, data.asks[i].quantity, data.asks[i].orders);
    }
    
    tick.totalBidQty = data.totalBuyQty;
    tick.totalAskQty = data.totalSellQty;
    tick.refNo = data.refNo;
    tick.timestampUdpRecv = data.timestampRecv;
    tick.timestampParsed = data.timestampParsed;
    tick.timestampEmitted = LatencyTracker::now();
    tick.messageType = 7200;
    return tick;
}

// Convert NSE CM Unified State to UDP::MarketTick
UDP::MarketTick convertNseCmUnified(const nsecm::UnifiedTokenState& data) {
    UDP::MarketTick tick(UDP::ExchangeSegment::NSECM, data.token);
    tick.ltp = data.ltp;
    tick.ltq = data.lastTradeQty;
    tick.volume = data.volume;
    tick.open = data.open;
    tick.high = data.high;
    tick.low = data.low;
    tick.prevClose = data.close;
    tick.atp = data.avgPrice;
    
    // Depth
    for (int i = 0; i < 5; i++) {
        tick.bids[i] = UDP::DepthLevel(data.bids[i].price, data.bids[i].quantity, data.bids[i].orders);
        tick.asks[i] = UDP::DepthLevel(data.asks[i].price, data.asks[i].quantity, data.asks[i].orders);
    }
    
    tick.totalBidQty = data.totalBuyQty;
    tick.totalAskQty = data.totalSellQty;
    
    tick.refNo = 0;
    tick.timestampUdpRecv = data.lastPacketTimestamp;
    tick.timestampParsed = data.lastPacketTimestamp;
    tick.timestampEmitted = LatencyTracker::now();
    tick.messageType = 7200;
    return tick;
}

// Convert NSE CM Touchline to UDP::MarketTick
UDP::MarketTick convertNseCmTouchline(const nsecm::TouchlineData& data) {
    UDP::MarketTick tick(UDP::ExchangeSegment::NSECM, data.token);
    tick.ltp = data.ltp;
    tick.ltq = data.lastTradeQty;
    tick.volume = data.volume;
    tick.open = data.open;
    tick.high = data.high;
    tick.low = data.low;
    tick.prevClose = data.close;
    tick.atp = data.avgPrice;
    tick.refNo = data.refNo;
    tick.timestampUdpRecv = data.timestampRecv;
    tick.timestampParsed = data.timestampParsed;
    tick.timestampEmitted = LatencyTracker::now();
    tick.messageType = 7200;
    return tick;
}

// Convert NSE CM Market Depth to UDP::MarketTick
UDP::MarketTick convertNseCmDepth(const nsecm::MarketDepthData& data) {
    UDP::MarketTick tick(UDP::ExchangeSegment::NSECM, data.token);
    
    // 5-level depth
    for (int i = 0; i < 5; i++) {
        tick.bids[i] = UDP::DepthLevel(data.bids[i].price, data.bids[i].quantity, data.bids[i].orders);
        tick.asks[i] = UDP::DepthLevel(data.asks[i].price, data.asks[i].quantity, data.asks[i].orders);
    }
    
    tick.totalBidQty = data.totalBuyQty;
    tick.totalAskQty = data.totalSellQty;
    tick.refNo = data.refNo;
    tick.timestampUdpRecv = data.timestampRecv;
    tick.timestampParsed = data.timestampParsed;
    tick.timestampEmitted = LatencyTracker::now();
    tick.messageType = 7200;
    return tick;
}

// Convert BSE Unified State to UDP::MarketTick
UDP::MarketTick convertBseUnified(const bse::UnifiedTokenState& data, UDP::ExchangeSegment segment) {
    UDP::MarketTick tick(segment, data.token);
    tick.ltp = data.ltp;
    tick.ltq = data.lastTradeQty;
    tick.volume = data.volume;
    tick.open = data.open;
    tick.high = data.high;
    tick.low = data.low;
    tick.prevClose = data.close;
    tick.atp = data.avgPrice;
    
    // Depth
    for (int i = 0; i < 5; i++) {
        tick.bids[i] = UDP::DepthLevel(data.bids[i].price, data.bids[i].quantity, data.bids[i].orders);
        tick.asks[i] = UDP::DepthLevel(data.asks[i].price, data.asks[i].quantity, data.asks[i].orders);
    }
    
    tick.totalBidQty = data.totalBuyQty;
    tick.totalAskQty = data.totalSellQty;
    
    tick.openInterest = data.openInterest;
    tick.oiChange = data.openInterestChange;

    // tick.upperLimit/lowerLimit not in MarketTick
    
    tick.refNo = 0;
    tick.timestampUdpRecv = data.lastPacketTimestamp;
    tick.timestampParsed = data.lastPacketTimestamp;
    tick.timestampEmitted = LatencyTracker::now();
    tick.messageType = 2020; // Generic Market Picture logic
    return tick;
}

// Convert BSE Session State to UDP::SessionStateTick
UDP::SessionStateTick convertBseSessionState(const bse::DecodedSessionState& state, UDP::ExchangeSegment segment) {
    UDP::SessionStateTick sessTick;
    sessTick.exchangeSegment = segment;
    sessTick.sessionNumber = state.sessionNumber;
    sessTick.marketSegmentId = state.marketSegmentId;
    
    // Map BSE market type to UDP::SessionState
    switch (state.marketType) {
        case 0: sessTick.state = UDP::SessionState::PRE_OPEN; break;
        case 1: sessTick.state = UDP::SessionState::CONTINUOUS; break;
        case 2: sessTick.state = UDP::SessionState::AUCTION; break;
        default: sessTick.state = UDP::SessionState::UNKNOWN; break;
    }
    
    sessTick.isStart = (state.startEndFlag == 0);
    sessTick.timestamp = state.timestamp;
    sessTick.timestampUdpRecv = state.packetTimestamp;
    return sessTick;
}

// Convert BSE Circuit Limit to UDP::CircuitLimitTick
UDP::CircuitLimitTick convertBseCircuitLimit(uint32_t token, int32_t upperCircuit, int32_t lowerCircuit, 
                                              UDP::ExchangeSegment segment, uint64_t timestamp) {
    UDP::CircuitLimitTick tick;
    tick.exchangeSegment = segment;
    tick.token = token;
    tick.upperLimit = upperCircuit / 100.0;  // Convert paise to rupees
    tick.lowerLimit = lowerCircuit / 100.0;
    tick.upperExecutionBand = tick.upperLimit;  // BSE uses same for execution band
    tick.lowerExecutionBand = tick.lowerLimit;
    tick.timestampUdpRecv = timestamp;
    
    return tick;
}

// Convert NSE FO Index Data to UDP::IndexTick
UDP::IndexTick convertNseFoIndex(const nsefo::IndexData& data) {
    UDP::IndexTick tick;
    tick.exchangeSegment = UDP::ExchangeSegment::NSEFO;
    tick.token = 0;  // Index doesn't have a token in 7207
    
    // Copy index name (ensure null-termination)
    size_t copySize = sizeof(tick.name) - 1;
    if (sizeof(data.name) < copySize) copySize = sizeof(data.name);
    std::memcpy(tick.name, data.name, copySize);
    tick.name[sizeof(tick.name) - 1] = '\0';
    
    tick.value = data.value;
    tick.open = data.open;
    tick.high = data.high;
    tick.low = data.low;
    tick.prevClose = data.close;
    tick.changePercent = data.percentChange;
    tick.change = tick.value - tick.prevClose;
    tick.marketCap = static_cast<uint64_t>(data.marketCap);
    tick.numAdvances = data.noOfUpmoves;
    tick.numDeclines = data.noOfDownmoves;
    tick.numUnchanged = 0;  // Not provided in message 7207
    tick.timestampUdpRecv = data.timestampRecv;
    
    return tick;
}

// Convert NSE FO Industry Index Data to UDP::IndexTick
UDP::IndexTick convertNseFoIndustryIndex(const nsefo::IndustryIndexData& data) {
    UDP::IndexTick tick;
    tick.exchangeSegment = UDP::ExchangeSegment::NSEFO;
    tick.token = 0;  // Industry index doesn't have a token
    
    // Copy industry name (ensure null-termination)
    size_t copySize2 = sizeof(tick.name) - 1;
    if (sizeof(data.name) < copySize2) copySize2 = sizeof(data.name);
    std::memcpy(tick.name, data.name, copySize2);
    tick.name[sizeof(tick.name) - 1] = '\0';
    
    tick.value = data.value;
    tick.timestampUdpRecv = data.timestampRecv;
    
    return tick;
}



// Convert BSE Index (DecodedRecord) to UDP::IndexTick
UDP::IndexTick convertBseIndex(const bse::DecodedRecord& record, UDP::ExchangeSegment segment) {
    UDP::IndexTick tick;
    tick.exchangeSegment = segment;
    tick.token = record.token;
    
    // Name is not in the record, will rely on UI to map token to name
    tick.name[0] = '\0';
    
    tick.value = record.ltp / 100.0;
    tick.open = record.open / 100.0;
    tick.high = record.high / 100.0;
    tick.low = record.low / 100.0;
    tick.prevClose = record.close / 100.0;
    tick.change = tick.value - tick.prevClose;
    tick.changePercent = (tick.prevClose > 0) ? (tick.change / tick.prevClose) * 100.0 : 0.0;
    
    // Not provided in BSE 2012 message
    tick.marketCap = 0;
    tick.numAdvances = 0;
    tick.numDeclines = 0;
    tick.numUnchanged = 0;
    
    tick.timestampUdpRecv = record.packetTimestamp;
    
    return tick;
}

// Convert NSE FO Circuit Limit Data to UDP::CircuitLimitTick
UDP::CircuitLimitTick convertNseFoCircuitLimit(const nsefo::CircuitLimitData& data) {
    UDP::CircuitLimitTick tick;
    tick.exchangeSegment = UDP::ExchangeSegment::NSEFO;
    tick.token = data.token;
    tick.upperLimit = data.upperLimit;
    tick.lowerLimit = data.lowerLimit;
    tick.upperExecutionBand = data.upperLimit;  // NSE uses same for execution band
    tick.lowerExecutionBand = data.lowerLimit;
    tick.timestampUdpRecv = data.timestampRecv;
    
    return tick;
}


} // anonymous namespace


// ========== SERVICE IMPLEMENTATION ==========

UdpBroadcastService& UdpBroadcastService::instance() {
    static UdpBroadcastService inst;
    return inst;
}

UdpBroadcastService::UdpBroadcastService(QObject* parent) : QObject(parent) {}

UdpBroadcastService::~UdpBroadcastService() {
    stop();
}

// ========== SUBSCRIPTION MANAGEMENT ==========

void UdpBroadcastService::subscribeToken(uint32_t token, int exchangeSegment) {
    std::unique_lock lock(m_subscriptionMutex);
    m_subscribedTokens.insert(token);
    
    // Sync with Distributed Gateway filter
    MarketData::PriceStoreGateway::instance().setTokenEnabled(exchangeSegment, token, true);

    // qDebug() << "[UdpBroadcast] Subscribed to token:" << token 
    //          << "Segment:" << exchangeSegment
    //          << "Total subscriptions:" << m_subscribedTokens.size();
}

void UdpBroadcastService::unsubscribeToken(uint32_t token, int exchangeSegment) {
    std::unique_lock lock(m_subscriptionMutex);
    m_subscribedTokens.erase(token);
    
    // Sync with Distributed Gateway filter
    MarketData::PriceStoreGateway::instance().setTokenEnabled(exchangeSegment, token, false);

    qDebug() << "[UdpBroadcast] Unsubscribed from token:" << token
             << "Segment:" << exchangeSegment
             << "Remaining subscriptions:" << m_subscribedTokens.size();
}

void UdpBroadcastService::clearSubscriptions() {
    std::unique_lock lock(m_subscriptionMutex);
    m_subscribedTokens.clear();
    qDebug() << "[UdpBroadcast] Cleared all subscriptions";
}

void UdpBroadcastService::setSubscriptionFilterEnabled(bool enabled) {
    m_filteringEnabled = enabled;
    qDebug() << "[UdpBroadcast] Subscription filtering:" << (enabled ? "ENABLED" : "DISABLED");
    if (!enabled) {
        qWarning() << "[UdpBroadcast] Warning: Filtering disabled - will emit signals for ALL ticks (high CPU usage)";
    }
}

// ========== CALLBACK SETUP ==========

void UdpBroadcastService::setupNseFoCallbacks() {
    // 0. Set Notification Filter
    nsefo::MarketDataCallbackRegistry::instance().setTokenFilter([](int32_t token) {
        return MarketData::PriceStoreGateway::instance().isTokenEnabled(2, token);
    });

    // Touchline + Depth (7200/7208) - Unified Callback
    auto unifiedCallback = [this](int32_t token) {
        // 1. Fetch from Global Store (Zero-Copy Read)
        const auto* data = nsefo::g_nseFoPriceStore.getUnifiedState(token);
        if (!data) return;

        // 2. Convert to UDP::MarketTick
        UDP::MarketTick udpTick = convertNseFoUnified(*data);
        m_totalTicks++;

        // 3. FeedHandler Distribution
        FeedHandler::instance().onUdpTickReceived(udpTick);
        
        // 4. Greeks Calculation (for option contracts only)
        auto& greeksService = GreeksCalculationService::instance();
        if (greeksService.isEnabled() && data->ltp > 0) {
            // Trigger async Greeks calculation - service handles throttling internally
            greeksService.onPriceUpdate(token, data->ltp, 2 /*NSEFO*/);
            
            // Also trigger updates for options where THIS token is the underlying (Futures)
            greeksService.onUnderlyingPriceUpdate(token, data->ltp, 2 /*NSEFO*/);
        }

        // 5. UI Signals (Throttled)
        if (shouldEmitSignal(token)) {
            emit udpTickReceived(udpTick);
        }
    };

    nsefo::MarketDataCallbackRegistry::instance().registerTouchlineCallback(unifiedCallback);
    nsefo::MarketDataCallbackRegistry::instance().registerMarketDepthCallback(unifiedCallback);
    
    // Ticker (7202) -> Using same unified callback as Ticker updates UnifiedState too
    nsefo::MarketDataCallbackRegistry::instance().registerTickerCallback(unifiedCallback);
    
    // Circuit Limit (7220) -> Also updates unified state? 
    // Wait, UnifiedState doesn't have circuit limits yet (except LPP which I added). 
    // If parsers update store, we can use the same callback.
    nsefo::MarketDataCallbackRegistry::instance().registerCircuitLimitCallback(unifiedCallback);
    
    // Index callback for message 7207 (BCAST_INDICES)
    nsefo::MarketDataCallbackRegistry::instance().registerIndexCallback([this](const nsefo::IndexData& data) {
        // Emit new UDP::IndexTick
        UDP::IndexTick indexTick = convertNseFoIndex(data);
        emit udpIndexReceived(indexTick);
    });
    
    // Industry index callback for message 7203 (BCAST_INDUSTRY_INDEX_UPDATE)
    nsefo::MarketDataCallbackRegistry::instance().registerIndustryIndexCallback([this](const nsefo::IndustryIndexData& data) {
        // Emit new UDP::IndexTick
        UDP::IndexTick indexTick = convertNseFoIndustryIndex(data);
        emit udpIndexReceived(indexTick);
    });
    

}

void UdpBroadcastService::setupNseCmCallbacks() {
    // 0. Set Notification Filter
    nsecm::MarketDataCallbackRegistry::instance().setTokenFilter([](int32_t token) {
        return MarketData::PriceStoreGateway::instance().isTokenEnabled(1, token);
    });

    // Unified CM Callback
    auto unifiedCallback = [this](int32_t token) {
        // 1. Fetch from Global CM Store
        const auto* data = nsecm::g_nseCmPriceStore.getUnifiedState(token);
        if (!data) return;

        // 2. Convert to UDP::MarketTick
        UDP::MarketTick udpTick = convertNseCmUnified(*data);
        m_totalTicks++;
        
        // 4. FeedHandler
        FeedHandler::instance().onUdpTickReceived(udpTick);
        
        // 5. Greeks Calculation (Update for underlyings in Cash Market)
        auto& greeksService = GreeksCalculationService::instance();
        if (greeksService.isEnabled() && data->ltp > 0) {
            greeksService.onUnderlyingPriceUpdate(token, data->ltp, 1 /*NSECM*/);
        }

        // 5. Signals
        if (shouldEmitSignal(token)) {
            emit udpTickReceived(udpTick);
        }
    };

    // Touchline updates (7200, 7208)
    nsecm::MarketDataCallbackRegistry::instance().registerTouchlineCallback(unifiedCallback);

    // Market depth updates (7200, 7208)
    nsecm::MarketDataCallbackRegistry::instance().registerMarketDepthCallback(unifiedCallback);

    // Ticker updates (18703)
    nsecm::MarketDataCallbackRegistry::instance().registerTickerCallback(unifiedCallback);

    // Market watch updates (7201)
    nsecm::MarketDataCallbackRegistry::instance().registerMarketWatchCallback([this](const nsecm::MarketWatchData& data) {
        // Log market watch data (can be removed in production)
        // qDebug() << "NSE CM Market Watch: Token=" << data.token;
    });

    // 7207 Index Callback for CM
    nsecm::MarketDataCallbackRegistry::instance().registerIndexCallback([this](const nsecm::IndicesUpdate& update) {
        // Log Stage 1: Reception
        // qDebug() << "[UDP] Received NSECM Index Update (7207), Records:" << update.numRecords;
        
        // Iterate through all indices in the update
        for (int i = 0; i < update.numRecords; i++) {
            const auto& data = update.indices[i];
            
            // Log Stage 2: Data Content
            // qDebug() << "  -> Index:" << data.name << "Value:" << data.value << "Change:" << (data.value - data.close);

            // Update unified store for ATMs and Watchlist
            auto itName = nsecm::g_indexNameToToken.find(data.name);
            if (itName != nsecm::g_indexNameToToken.end()) {
                uint32_t token = itName->second;
                nsecm::g_nseCmPriceStore.updateTouchline(token, data.value, data.open, data.high, data.low, data.close, 0, 0, 0, data.value, 0, 0, 0, 0);
            }

            UDP::IndexTick tick;
            tick.exchangeSegment = UDP::ExchangeSegment::NSECM;
            tick.token = 0; // No token in 7207
            
            // Name
            size_t copySize = sizeof(tick.name) - 1;
            if (sizeof(data.name) < copySize) copySize = sizeof(data.name);
            std::memcpy(tick.name, data.name, copySize);
            tick.name[sizeof(tick.name) - 1] = '\0';
            
            tick.value = data.value;
            tick.open = data.open;
            tick.high = data.high;
            tick.low = data.low;
            tick.prevClose = data.close;
            tick.changePercent = data.percentChange;
            tick.change = tick.value - tick.prevClose;
            tick.marketCap = static_cast<uint64_t>(data.marketCap);
            tick.numAdvances = data.upMoves;
            tick.numDeclines = data.downMoves;
            
            emit udpIndexReceived(tick);

            // FUNNEL TO FEEDHANDLER: Convert IndexTick to MarketTick for subscribers (like ATM Watch)
            if (itName != nsecm::g_indexNameToToken.end()) {
                UDP::MarketTick mTick;
                mTick.exchangeSegment = UDP::ExchangeSegment::NSECM;
                mTick.token = itName->second;
                mTick.ltp = tick.value;
                mTick.open = tick.open;
                mTick.high = tick.high;
                mTick.low = tick.low;
                mTick.prevClose = tick.prevClose;
                mTick.volume = 0; // Indices don't have volume in 7207
                
                FeedHandler::instance().onUdpTickReceived(mTick);
            }
        }
    });

    // Admin messages (6501, 6541)
    nsecm::MarketDataCallbackRegistry::instance().registerAdminCallback([this](const nsecm::AdminMessage& data) {
        // Log admin messages (can be removed in production)
        // qDebug() << "NSE CM Admin: Action=" << QString::fromStdString(data.actionCode) 
        //          << " Msg=" << QString::fromStdString(data.message);
    });

    // System information (7206)
    nsecm::MarketDataCallbackRegistry::instance().registerSystemInformationCallback([this](const nsecm::SystemInformationData& data) {
        // Log system information (can be removed in production)
        // qDebug() << "NSE CM System Info: MarketIndex=" << data.marketIndex 
        //          << " NormalStatus=" << data.normalMarketStatus;
    });

    // Call auction order cancellation (7210)
    nsecm::MarketDataCallbackRegistry::instance().registerCallAuctionOrderCxlCallback([this](const nsecm::CallAuctionOrderCxlData& data) {
        // Log call auction cancellations (can be removed in production)
        // qDebug() << "NSE CM Call Auction Cxl: Records=" << data.noOfRecords;
    });

    // Market open/close/pre-open messages (6511, 6521, 6522, 6531, 6571)
    nsecm::MarketDataCallbackRegistry::instance().registerMarketOpenCallback([this](const nsecm::MarketOpenMessage& data) {
        // Create session state tick
        UDP::SessionStateTick stateTick;
        stateTick.exchangeSegment = UDP::ExchangeSegment::NSECM;
        stateTick.timestamp = data.timestamp;
        stateTick.timestampUdpRecv = LatencyTracker::now();
        
        // Set state based on transaction code
        switch(data.txCode) {
            case 6511: // BC_OPEN_MSG
                stateTick.state = UDP::SessionState::PRE_OPEN;
                stateTick.isStart = true;
                break;
            case 6521: // BC_CLOSE_MSG
                stateTick.state = UDP::SessionState::CLOSED;
                stateTick.isStart = false;
                break;
            case 6522: // BC_POSTCLOSE_MSG
                stateTick.state = UDP::SessionState::POST_CLOSE;
                stateTick.isStart = true;
                break;
            case 6531: // BC_PRE_OR_POST_DAY_MSG
                stateTick.state = UDP::SessionState::PRE_OPEN;
                stateTick.isStart = true;
                break;
            case 6571: // BC_NORMAL_MKT_PREOPEN_ENDED
                stateTick.state = UDP::SessionState::CONTINUOUS;
                stateTick.isStart = true;
                break;
            default:
                stateTick.state = UDP::SessionState::UNKNOWN;
                break;
        }
        
        emit udpSessionStateReceived(stateTick);
        
        // Log for debugging (can be removed in production)
        // qDebug() << "NSE CM Session State: TxCode=" << data.txCode 
        //          << " Symbol=" << QString::fromStdString(data.symbol)
        //          << " State=" << static_cast<int>(stateTick.state);
    });
}

void UdpBroadcastService::setupBseFoCallbacks() {
    if (!m_bseFoReceiver) return;
    
    auto unifiedCallback = [this](uint32_t token) {
        const auto* data = bse::g_bseFoPriceStore.getUnifiedState(token);
        if (!data) return;

        UDP::MarketTick udpTick = convertBseUnified(*data, UDP::ExchangeSegment::BSEFO);
        m_totalTicks++;
        
        FeedHandler::instance().onUdpTickReceived(udpTick);
        
        // Greeks Calculation (for option contracts only)
        auto& greeksService = GreeksCalculationService::instance();
        if (greeksService.isEnabled() && data->ltp > 0) {
            greeksService.onPriceUpdate(token, data->ltp, 12 /*BSEFO*/);
            // Also trigger updates for options where THIS token is the underlying
            greeksService.onUnderlyingPriceUpdate(token, data->ltp, 12 /*BSEFO*/);
        }

        if (shouldEmitSignal(token)) {
             emit udpTickReceived(udpTick);
             
             // Check Circuit Limits from Unified Data
             if (data->upperCircuit > 0 || data->lowerCircuit > 0) {
                 UDP::CircuitLimitTick limitTick;
                 limitTick.token = token;
                 limitTick.upperLimit = data->upperCircuit;
                 limitTick.lowerLimit = data->lowerCircuit;
                 limitTick.exchangeSegment = UDP::ExchangeSegment::BSEFO;
                 emit udpCircuitLimitReceived(limitTick);
             }
        }
    };

    m_bseFoReceiver->setRecordCallback(unifiedCallback);
    m_bseFoReceiver->setOpenInterestCallback(unifiedCallback);
    m_bseFoReceiver->setClosePriceCallback(unifiedCallback);
    m_bseFoReceiver->setImpliedVolatilityCallback(unifiedCallback);
    
    m_bseFoReceiver->setSessionStateCallback([this](const bse::DecodedSessionState& state) {
        UDP::SessionStateTick sessTick = convertBseSessionState(state, UDP::ExchangeSegment::BSEFO);
        emit udpSessionStateReceived(sessTick);
    });
}

void UdpBroadcastService::setupBseCmCallbacks() {
    if (!m_bseCmReceiver) return;
    
    auto unifiedCallback = [this](uint32_t token) {
        const auto* data = bse::g_bseCmPriceStore.getUnifiedState(token);
        if (!data) return;

        UDP::MarketTick udpTick = convertBseUnified(*data, UDP::ExchangeSegment::BSECM);
        m_totalTicks++;

        FeedHandler::instance().onUdpTickReceived(udpTick);
        
        // Greeks Calculation (Update for underlyings in Cash Market)
        auto& greeksService = GreeksCalculationService::instance();
        if (greeksService.isEnabled() && data->ltp > 0) {
            greeksService.onUnderlyingPriceUpdate(token, data->ltp, 11 /*BSECM*/);
        }

        if (shouldEmitSignal(token)) {
             emit udpTickReceived(udpTick);
             
             if (data->upperCircuit > 0 || data->lowerCircuit > 0) {
                 UDP::CircuitLimitTick limitTick;
                 limitTick.token = token;
                 limitTick.upperLimit = data->upperCircuit;
                 limitTick.lowerLimit = data->lowerCircuit;
                 limitTick.exchangeSegment = UDP::ExchangeSegment::BSECM;
                 emit udpCircuitLimitReceived(limitTick);
             }
        }
    };

    m_bseCmReceiver->setRecordCallback(unifiedCallback);
    m_bseCmReceiver->setClosePriceCallback(unifiedCallback);
    
    m_bseCmReceiver->setSessionStateCallback([this](const bse::DecodedSessionState& state) {
        UDP::SessionStateTick sessTick = convertBseSessionState(state, UDP::ExchangeSegment::BSECM);
        emit udpSessionStateReceived(sessTick);
    });
    
    m_bseCmReceiver->setIndexCallback([this](uint32_t token) {
        const auto* record = bse::g_bseCmIndexStore.getIndex(token);
        if(!record) return;
        UDP::IndexTick indexTick = convertBseIndex(*record, UDP::ExchangeSegment::BSECM);
        emit udpIndexReceived(indexTick);
    });
}

void UdpBroadcastService::start(const Config& config) {
    if (m_active) return;

    m_lastConfig = config;
    qDebug() << "[UdpBroadcastService] Initializing broadcast segments...";
    
    try {
        bool anyEnabled = false;

        // CRITICAL FIX: Stagger receiver startup to prevent thread storm
        // Starting all 4 receivers simultaneously causes:
        // 1. CPU thread scheduler thrashing
        // 2. Distributed PriceStore concurrent write contention
        // 3. GUI event loop starvation
        // Solution: Start receivers with 100ms delays

        // 1. NSE FO Setup (highest priority - derivatives)
        if (config.enableNSEFO && !config.nseFoIp.empty() && config.nseFoPort > 0) {
            if (startReceiver(ExchangeReceiver::NSEFO, config.nseFoIp, config.nseFoPort)) {
                anyEnabled = true;
            }
            if (anyEnabled) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 2. NSE CM Setup (second priority - cash market)
        if (config.enableNSECM && !config.nseCmIp.empty() && config.nseCmPort > 0) {
            if (startReceiver(ExchangeReceiver::NSECM, config.nseCmIp, config.nseCmPort)) {
                anyEnabled = true;
            }
            if (anyEnabled) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 3. BSE FO Setup
        if (config.enableBSEFO && !config.bseFoIp.empty() && config.bseFoPort > 0) {
            if (startReceiver(ExchangeReceiver::BSEFO, config.bseFoIp, config.bseFoPort)) {
                anyEnabled = true;
            }
            if (anyEnabled) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 4. BSE CM Setup
        if (config.enableBSECM && !config.bseCmIp.empty() && config.bseCmPort > 0) {
            if (startReceiver(ExchangeReceiver::BSECM, config.bseCmIp, config.bseCmPort)) {
                anyEnabled = true;
            }
        }

        m_active = anyEnabled;
        emit statusChanged(m_active);
    } catch (const std::exception& e) {
        qCritical() << "[UdpBroadcastService] Initialization failure:" << e.what();
        emit statusChanged(false);
    }
}

bool UdpBroadcastService::startReceiver(ExchangeReceiver receiver, const std::string& ip, int port) {
    try {
        switch (receiver) {
            case ExchangeReceiver::NSEFO: {
                if (m_nseFoActive) {
                    qWarning() << "[UdpBroadcastService] NSE FO already running";
                    return true;
                }
                m_nseFoReceiver = std::make_unique<nsefo::MulticastReceiver>(ip, port);
                if (m_nseFoReceiver->isValid()) {
                    setupNseFoCallbacks();
                    m_nseFoThread = std::thread([this]() {
                        try { 
                            if (m_nseFoReceiver) m_nseFoReceiver->start(); 
                        } catch (...) { 
                            qCritical() << "[UdpBroadcastService] NSE FO Thread crashed"; 
                        }
                        m_nseFoActive = false;
                        emit receiverStatusChanged(ExchangeReceiver::NSEFO, false);
                    });
                    m_nseFoActive = true;
                    emit receiverStatusChanged(ExchangeReceiver::NSEFO, true);
                    qDebug() << "  -> NSE FO: STARTED on" << QString::fromStdString(ip) << ":" << port;
                    return true;
                }
                break;
            }
            
            case ExchangeReceiver::NSECM: {
                if (m_nseCmActive) {
                    qWarning() << "[UdpBroadcastService] NSE CM already running";
                    return true;
                }
                m_nseCmReceiver = std::make_unique<nsecm::MulticastReceiver>(ip, port);
                if (m_nseCmReceiver->isValid()) {
                    setupNseCmCallbacks();
                    m_nseCmThread = std::thread([this]() {
                        try { 
                            if (m_nseCmReceiver) m_nseCmReceiver->start(); 
                        } catch (...) { 
                            qCritical() << "[UdpBroadcastService] NSE CM Thread crashed"; 
                        }
                        m_nseCmActive = false;
                        emit receiverStatusChanged(ExchangeReceiver::NSECM, false);
                    });
                    m_nseCmActive = true;
                    emit receiverStatusChanged(ExchangeReceiver::NSECM, true);
                    qDebug() << "  -> NSE CM: STARTED on" << QString::fromStdString(ip) << ":" << port;
                    return true;
                }
                break;
            }
            
            case ExchangeReceiver::BSEFO: {
                if (m_bseFoActive) {
                    qWarning() << "[UdpBroadcastService] BSE FO already running";
                    return true;
                }
                m_bseFoReceiver = std::make_unique<bse::BSEReceiver>(ip, port, "BSEFO");
                setupBseFoCallbacks();
                m_bseFoThread = std::thread([this]() {
                    try { 
                        if (m_bseFoReceiver) m_bseFoReceiver->start(); 
                    } catch (...) { 
                        qCritical() << "[UdpBroadcastService] BSE FO Thread crashed"; 
                    }
                    m_bseFoActive = false;
                    emit receiverStatusChanged(ExchangeReceiver::BSEFO, false);
                });
                m_bseFoActive = true;
                emit receiverStatusChanged(ExchangeReceiver::BSEFO, true);
                qDebug() << "  -> BSE FO: STARTED on" << QString::fromStdString(ip) << ":" << port;
                return true;
            }
            
            case ExchangeReceiver::BSECM: {
                if (m_bseCmActive) {
                    qWarning() << "[UdpBroadcastService] BSE CM already running";
                    return true;
                }
                m_bseCmReceiver = std::make_unique<bse::BSEReceiver>(ip, port, "BSECM");
                setupBseCmCallbacks();
                m_bseCmThread = std::thread([this]() {
                    try { 
                        if (m_bseCmReceiver) m_bseCmReceiver->start(); 
                    } catch (...) { 
                        qCritical() << "[UdpBroadcastService] BSE CM Thread crashed"; 
                    }
                    m_bseCmActive = false;
                    emit receiverStatusChanged(ExchangeReceiver::BSECM, false);
                });
                m_bseCmActive = true;
                emit receiverStatusChanged(ExchangeReceiver::BSECM, true);
                qDebug() << "  -> BSE CM: STARTED on" << QString::fromStdString(ip) << ":" << port;
                return true;
            }
        }
    } catch (const std::exception& e) {
        qCritical() << "[UdpBroadcastService] Failed to start receiver:" << e.what();
    }
    return false;
}

void UdpBroadcastService::stopReceiver(ExchangeReceiver receiver) {
    switch (receiver) {
        case ExchangeReceiver::NSEFO:
            if (m_nseFoReceiver) {
                m_nseFoReceiver->stop();
                if (m_nseFoThread.joinable()) {
                    m_nseFoThread.join();
                }
                m_nseFoReceiver.reset();
                m_nseFoActive = false;
                emit receiverStatusChanged(ExchangeReceiver::NSEFO, false);
                qDebug() << "[UdpBroadcastService] NSE FO: STOPPED";
            }
            break;
            
        case ExchangeReceiver::NSECM:
            if (m_nseCmReceiver) {
                m_nseCmReceiver->stop();
                if (m_nseCmThread.joinable()) {
                    m_nseCmThread.join();
                }
                m_nseCmReceiver.reset();
                m_nseCmActive = false;
                emit receiverStatusChanged(ExchangeReceiver::NSECM, false);
                qDebug() << "[UdpBroadcastService] NSE CM: STOPPED";
            }
            break;
            
        case ExchangeReceiver::BSEFO:
            if (m_bseFoReceiver) {
                m_bseFoReceiver->stop();
                if (m_bseFoThread.joinable()) {
                    m_bseFoThread.join();
                }
                m_bseFoReceiver.reset();
                m_bseFoActive = false;
                emit receiverStatusChanged(ExchangeReceiver::BSEFO, false);
                qDebug() << "[UdpBroadcastService] BSE FO: STOPPED";
            }
            break;
            
        case ExchangeReceiver::BSECM:
            if (m_bseCmReceiver) {
                m_bseCmReceiver->stop();
                if (m_bseCmThread.joinable()) {
                    m_bseCmThread.join();
                }
                m_bseCmReceiver.reset();
                m_bseCmActive = false;
                emit receiverStatusChanged(ExchangeReceiver::BSECM, false);
                qDebug() << "[UdpBroadcastService] BSE CM: STOPPED";
            }
            break;
    }
    
    // Update overall active status
    m_active = m_nseFoActive || m_nseCmActive || m_bseFoActive || m_bseCmActive;
    emit statusChanged(m_active);
}

bool UdpBroadcastService::isReceiverActive(ExchangeReceiver receiver) const {
    switch (receiver) {
        case ExchangeReceiver::NSEFO: return m_nseFoActive;
        case ExchangeReceiver::NSECM: return m_nseCmActive;
        case ExchangeReceiver::BSEFO: return m_bseFoActive;
        case ExchangeReceiver::BSECM: return m_bseCmActive;
    }
    return false;
}

bool UdpBroadcastService::restartReceiver(ExchangeReceiver receiver, const std::string& ip, int port) {
    stopReceiver(receiver);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return startReceiver(receiver, ip, port);
}

void UdpBroadcastService::stop() {
    if (!m_active && !m_nseFoActive && !m_nseCmActive && !m_bseFoActive && !m_bseCmActive) return;
    
    qDebug() << "[UdpBroadcastService] Stopping all receivers...";
    
    // Stop all receivers
    if (m_nseFoReceiver) m_nseFoReceiver->stop();
    if (m_nseCmReceiver) m_nseCmReceiver->stop();
    if (m_bseFoReceiver) m_bseFoReceiver->stop();
    if (m_bseCmReceiver) m_bseCmReceiver->stop();

    // Wait for threads to finish
    if (m_nseFoThread.joinable()) m_nseFoThread.join();
    if (m_nseCmThread.joinable()) m_nseCmThread.join();
    if (m_bseFoThread.joinable()) m_bseFoThread.join();
    if (m_bseCmThread.joinable()) m_bseCmThread.join();

    // Reset receivers
    m_nseFoReceiver.reset();
    m_nseCmReceiver.reset();
    m_bseFoReceiver.reset();
    m_bseCmReceiver.reset();

    // Update status
    m_nseFoActive = false;
    m_nseCmActive = false;
    m_bseFoActive = false;
    m_bseCmActive = false;
    m_active = false;
    
    emit statusChanged(false);
    qDebug() << "[UdpBroadcastService] All receivers stopped";
}

bool UdpBroadcastService::isActive() const {
    return m_active;
}

UdpBroadcastService::Stats UdpBroadcastService::getStats() const {
    Stats s;
    s.nseFoPackets = m_nseFoReceiver ? m_nseFoReceiver->getStats().totalPackets : 0;
    s.nseCmPackets = m_nseCmReceiver ? m_nseCmReceiver->getStats().totalPackets : 0;
    s.bseFoPackets = m_bseFoReceiver ? m_bseFoReceiver->getStats().packetsReceived : 0;
    s.bseCmPackets = m_bseCmReceiver ? m_bseCmReceiver->getStats().packetsReceived : 0;
    s.totalTicks = m_totalTicks.load();
    s.nseFoActive = m_nseFoActive;
    s.nseCmActive = m_nseCmActive;
    s.bseFoActive = m_bseFoActive;
    s.bseCmActive = m_bseCmActive;
    return s;
}

#include "services/UdpBroadcastService.h"
#include "nsefo_callback.h"
#include "nsecm_callback.h"
#include "services/FeedHandler.h"
#include "utils/LatencyTracker.h"
#include <QMetaObject>
#include <QDebug>
#include <thread>
#include <iostream>
#include <cstring>

// ========== CONVERSION HELPERS: Native Protocol → UDP::MarketTick ==========

namespace {

// Convert NSE FO Touchline to UDP::MarketTick
UDP::MarketTick convertNseFoTouchline(const nsefo::TouchlineData& data) {
    UDP::MarketTick tick(UDP::ExchangeSegment::NSEFO, data.token);
    tick.ltp = data.ltp;
    tick.ltq = data.lastTradeQty;
    tick.volume = data.volume;
    tick.open = data.open;
    tick.high = data.high;
    tick.low = data.low;
    tick.prevClose = data.close;  // NSE: "close" means previous close
    tick.refNo = data.refNo;
    tick.timestampUdpRecv = data.timestampRecv;
    tick.timestampParsed = data.timestampParsed;
    tick.timestampEmitted = LatencyTracker::now();
    tick.messageType = 7200;  // BCAST_MBO_MBP_UPDATE
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

// Convert BSE Record to UDP::MarketTick
UDP::MarketTick convertBseRecord(const bse::DecodedRecord& record, UDP::ExchangeSegment segment) {
    UDP::MarketTick tick(segment, record.token);
    tick.ltp = record.ltp / 100.0;
    tick.ltq = record.ltq;
    tick.volume = record.volume;
    tick.open = record.open / 100.0;
    tick.high = record.high / 100.0;
    tick.low = record.low / 100.0;
    tick.prevClose = record.close / 100.0;  // BSE: close means previous close
    tick.atp = record.weightedAvgPrice / 100.0;
    
    // 5-level depth
    for (size_t i = 0; i < 5 && i < record.bids.size(); i++) {
        tick.bids[i].price = record.bids[i].price / 100.0;
        tick.bids[i].quantity = record.bids[i].quantity;
        tick.bids[i].orders = record.bids[i].numOrders;
    }
    for (size_t i = 0; i < 5 && i < record.asks.size(); i++) {
        tick.asks[i].price = record.asks[i].price / 100.0;
        tick.asks[i].quantity = record.asks[i].quantity;
        tick.asks[i].orders = record.asks[i].numOrders;
    }
    
    tick.marketSeqNumber = 0;  // BSE doesn't provide this in 2020/2021
    tick.timestampUdpRecv = record.packetTimestamp;
    tick.timestampParsed = record.packetTimestamp;
    tick.timestampEmitted = LatencyTracker::now();
    tick.messageType = 2020;  // MARKET_PICTURE
    return tick;
}

// Convert BSE Open Interest to UDP::MarketTick
UDP::MarketTick convertBseOI(const bse::DecodedOpenInterest& oiData, UDP::ExchangeSegment segment) {
    UDP::MarketTick tick(segment, oiData.token);
    tick.openInterest = oiData.openInterest;
    tick.oiChange = oiData.openInterestChange;
    tick.timestampUdpRecv = oiData.packetTimestamp;
    tick.timestampParsed = oiData.packetTimestamp;
    tick.timestampEmitted = LatencyTracker::now();
    tick.messageType = 2015;  // OPEN_INTEREST
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

// Backward compatibility: Convert UDP::MarketTick to legacy XTS::Tick
XTS::Tick convertToLegacy(const UDP::MarketTick& udp) {
    XTS::Tick tick;
    tick.exchangeSegment = static_cast<int>(udp.exchangeSegment);
    tick.exchangeInstrumentID = udp.token;
    tick.lastTradedPrice = udp.ltp;
    tick.lastTradedQuantity = (int)udp.ltq;
    tick.volume = udp.volume;
    tick.open = udp.open;
    tick.high = udp.high;
    tick.low = udp.low;
    tick.close = udp.prevClose;  // Map prevClose → close (legacy field name)
    tick.averagePrice = udp.atp;
    tick.openInterest = udp.openInterest;
    
    // 5-level depth
    for (int i = 0; i < 5; i++) {
        tick.bidDepth[i].price = udp.bids[i].price;
        tick.bidDepth[i].quantity = udp.bids[i].quantity;
        tick.bidDepth[i].orders = (int)udp.bids[i].orders;
        tick.askDepth[i].price = udp.asks[i].price;
        tick.askDepth[i].quantity = udp.asks[i].quantity;
        tick.askDepth[i].orders = (int)udp.asks[i].orders;
    }
    
    // Best bid/ask
    tick.bidPrice = udp.bestBid();
    tick.bidQuantity = (int)udp.bids[0].quantity;
    tick.askPrice = udp.bestAsk();
    tick.askQuantity = (int)udp.asks[0].quantity;
    tick.totalBuyQuantity = (int)udp.totalBidQty;
    tick.totalSellQuantity = (int)udp.totalAskQty;
    
    // Latency tracking
    tick.refNo = udp.refNo;
    tick.timestampUdpRecv = udp.timestampUdpRecv;
    tick.timestampParsed = udp.timestampParsed;
    tick.timestampQueued = udp.timestampEmitted;
    
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

void UdpBroadcastService::setupNseFoCallbacks() {
    nsefo::MarketDataCallbackRegistry::instance().registerTouchlineCallback([this](const nsefo::TouchlineData& data) {
        // Emit new UDP::MarketTick
        UDP::MarketTick udpTick = convertNseFoTouchline(data);
        emit udpTickReceived(udpTick);
        
        // Emit legacy XTS::Tick for backward compatibility
        XTS::Tick legacyTick = convertToLegacy(udpTick);
        m_totalTicks++;
        emit tickReceived(legacyTick);
    });

    nsefo::MarketDataCallbackRegistry::instance().registerMarketDepthCallback([this](const nsefo::MarketDepthData& data) {
        // Emit new UDP::MarketTick
        UDP::MarketTick udpTick = convertNseFoDepth(data);
        emit udpTickReceived(udpTick);
        
        // Emit legacy XTS::Tick for backward compatibility
        XTS::Tick legacyTick = convertToLegacy(udpTick);
        m_totalTicks++;
        emit tickReceived(legacyTick);
    });

    nsefo::MarketDataCallbackRegistry::instance().registerTickerCallback([this](const nsefo::TickerData& data) {
        if (data.fillVolume > 0) {
            // Create UDP::MarketTick for ticker (partial update)
            UDP::MarketTick udpTick(UDP::ExchangeSegment::NSEFO, data.token);
            udpTick.volume = data.fillVolume;
            udpTick.refNo = data.refNo;
            udpTick.timestampUdpRecv = data.timestampRecv;
            udpTick.timestampParsed = data.timestampParsed;
            udpTick.timestampEmitted = LatencyTracker::now();
            udpTick.messageType = 7202;  // BCAST_TICKER_AND_MKT_INDEX
            emit udpTickReceived(udpTick);
            
            // Legacy XTS::Tick
            XTS::Tick legacyTick = convertToLegacy(udpTick);
            m_totalTicks++;
            emit tickReceived(legacyTick);
        }
    });
    
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
    
    // Circuit limit callback for message 7220 (BCAST_LIMIT_PRICE_PROTECTION_RANGE)
    nsefo::MarketDataCallbackRegistry::instance().registerCircuitLimitCallback([this](const nsefo::CircuitLimitData& data) {
        // Emit new UDP::CircuitLimitTick
        UDP::CircuitLimitTick limitTick = convertNseFoCircuitLimit(data);
        emit udpCircuitLimitReceived(limitTick);
    });
}

void UdpBroadcastService::setupNseCmCallbacks() {
    nsecm::MarketDataCallbackRegistry::instance().registerTouchlineCallback([this](const nsecm::TouchlineData& data) {
        // Emit new UDP::MarketTick
        UDP::MarketTick udpTick = convertNseCmTouchline(data);
        emit udpTickReceived(udpTick);
        
        // Emit legacy XTS::Tick for backward compatibility
        XTS::Tick legacyTick = convertToLegacy(udpTick);
        m_totalTicks++;
        emit tickReceived(legacyTick);
    });

    nsecm::MarketDataCallbackRegistry::instance().registerMarketDepthCallback([this](const nsecm::MarketDepthData& data) {
        // Emit new UDP::MarketTick
        UDP::MarketTick udpTick = convertNseCmDepth(data);
        emit udpTickReceived(udpTick);
        
        // Emit legacy XTS::Tick for backward compatibility
        XTS::Tick legacyTick = convertToLegacy(udpTick);
        m_totalTicks++;
        emit tickReceived(legacyTick);
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
            
            // Log Stage 3: Emission
            qDebug() << "[UDP] Emitting NSECM Index For:" << tick.name << "Val:" << tick.value;
            emit udpIndexReceived(tick);
        }
    });
}

void UdpBroadcastService::setupBseFoCallbacks() {
    if (!m_bseFoReceiver) return;
    
    m_bseFoReceiver->setRecordCallback([this](const bse::DecodedRecord& record) {
        // Emit new UDP::MarketTick
        UDP::MarketTick udpTick = convertBseRecord(record, UDP::ExchangeSegment::BSEFO);
        emit udpTickReceived(udpTick);
        
        // Emit legacy XTS::Tick for backward compatibility
        XTS::Tick legacyTick = convertToLegacy(udpTick);
        m_totalTicks++;
        emit tickReceived(legacyTick);
        
        // Emit circuit limits if present
        if (record.upperCircuit > 0 || record.lowerCircuit > 0) {
            UDP::CircuitLimitTick limitTick = convertBseCircuitLimit(
                record.token, record.upperCircuit, record.lowerCircuit, 
                UDP::ExchangeSegment::BSEFO, record.packetTimestamp);
            emit udpCircuitLimitReceived(limitTick);
        }
    });
    
    // Open Interest callback for BSE FO derivatives
    m_bseFoReceiver->setOpenInterestCallback([this](const bse::DecodedOpenInterest& oiData) {
        // Emit new UDP::MarketTick
        UDP::MarketTick udpTick = convertBseOI(oiData, UDP::ExchangeSegment::BSEFO);
        emit udpTickReceived(udpTick);
        
        // Emit legacy XTS::Tick for backward compatibility
        XTS::Tick legacyTick = convertToLegacy(udpTick);
        m_totalTicks++;
        emit tickReceived(legacyTick);
    });
    
    // Session State callback for BSE FO
    m_bseFoReceiver->setSessionStateCallback([this](const bse::DecodedSessionState& state) {
        UDP::SessionStateTick sessTick = convertBseSessionState(state, UDP::ExchangeSegment::BSEFO);
        emit udpSessionStateReceived(sessTick);
    });
    
    // Close Price callback for BSE FO
    m_bseFoReceiver->setClosePriceCallback([this](const bse::DecodedClosePrice& cp) {
        // Update prevClose field for the token
        // Create a partial MarketTick update with just the close price
        UDP::MarketTick udpTick(UDP::ExchangeSegment::BSEFO, cp.token);
        udpTick.prevClose = cp.closePrice / 100.0;  // Convert paise to rupees
        udpTick.timestampUdpRecv = cp.packetTimestamp;
        udpTick.timestampEmitted = LatencyTracker::now();
        udpTick.messageType = 2014;  // CLOSE_PRICE
        emit udpTickReceived(udpTick);
    });
    
    // Index Callback (2012)
    m_bseFoReceiver->setIndexCallback([this](const bse::DecodedRecord& record) {
        UDP::IndexTick indexTick = convertBseIndex(record, UDP::ExchangeSegment::BSEFO);
        emit udpIndexReceived(indexTick);
    });
}

void UdpBroadcastService::setupBseCmCallbacks() {
    if (!m_bseCmReceiver) return;
    
    m_bseCmReceiver->setRecordCallback([this](const bse::DecodedRecord& record) {
        // Emit new UDP::MarketTick
        UDP::MarketTick udpTick = convertBseRecord(record, UDP::ExchangeSegment::BSECM);
        emit udpTickReceived(udpTick);
        
        // Emit legacy XTS::Tick for backward compatibility
        XTS::Tick legacyTick = convertToLegacy(udpTick);
        m_totalTicks++;
        emit tickReceived(legacyTick);
        
        // Emit circuit limits if present
        if (record.upperCircuit > 0 || record.lowerCircuit > 0) {
            UDP::CircuitLimitTick limitTick = convertBseCircuitLimit(
                record.token, record.upperCircuit, record.lowerCircuit, 
                UDP::ExchangeSegment::BSECM, record.packetTimestamp);
            emit udpCircuitLimitReceived(limitTick);
        }
    });
    
    // Session State callback for BSE CM
    m_bseCmReceiver->setSessionStateCallback([this](const bse::DecodedSessionState& state) {
        UDP::SessionStateTick sessTick = convertBseSessionState(state, UDP::ExchangeSegment::BSECM);
        emit udpSessionStateReceived(sessTick);
    });
    
    // Close Price callback for BSE CM
    m_bseCmReceiver->setClosePriceCallback([this](const bse::DecodedClosePrice& cp) {
        // Update prevClose field for the token
        UDP::MarketTick udpTick(UDP::ExchangeSegment::BSECM, cp.token);
        udpTick.prevClose = cp.closePrice / 100.0;  // Convert paise to rupees
        udpTick.timestampUdpRecv = cp.packetTimestamp;
        udpTick.timestampEmitted = LatencyTracker::now();
        udpTick.messageType = 2014;  // CLOSE_PRICE
        emit udpTickReceived(udpTick);
    });
    
    // Index Callback (2012)
    m_bseCmReceiver->setIndexCallback([this](const bse::DecodedRecord& record) {
        UDP::IndexTick indexTick = convertBseIndex(record, UDP::ExchangeSegment::BSECM);
        emit udpIndexReceived(indexTick);
    });
}

void UdpBroadcastService::start(const Config& config) {
    if (m_active) return;

    m_lastConfig = config;
    qDebug() << "[UdpBroadcastService] Initializing broadcast segments...";
    
    try {
        bool anyEnabled = false;

        // 1. NSE FO Setup
        if (config.enableNSEFO && !config.nseFoIp.empty() && config.nseFoPort > 0) {
            if (startReceiver(ExchangeReceiver::NSEFO, config.nseFoIp, config.nseFoPort)) {
                anyEnabled = true;
            }
        }

        // 2. NSE CM Setup
        if (config.enableNSECM && !config.nseCmIp.empty() && config.nseCmPort > 0) {
            if (startReceiver(ExchangeReceiver::NSECM, config.nseCmIp, config.nseCmPort)) {
                anyEnabled = true;
            }
        }

        // 3. BSE FO Setup
        if (config.enableBSEFO && !config.bseFoIp.empty() && config.bseFoPort > 0) {
            if (startReceiver(ExchangeReceiver::BSEFO, config.bseFoIp, config.bseFoPort)) {
                anyEnabled = true;
            }
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

#include "services/UdpBroadcastService.h"
#include "nsefo_callback.h"
#include "nsecm_callback.h"
#include "services/FeedHandler.h"
#include "utils/LatencyTracker.h"
#include <QMetaObject>
#include <QDebug>
#include <thread>
#include <iostream>

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
    });
    
    // Session State callback for BSE CM
    m_bseCmReceiver->setSessionStateCallback([this](const bse::DecodedSessionState& state) {
        UDP::SessionStateTick sessTick = convertBseSessionState(state, UDP::ExchangeSegment::BSECM);
        emit udpSessionStateReceived(sessTick);
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

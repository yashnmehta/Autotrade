#include "services/UdpBroadcastService.h"
#include "nsefo_callback.h"
#include "nsecm_callback.h"
#include "services/FeedHandler.h"
#include "utils/LatencyTracker.h"
#include <QMetaObject>
#include <QDebug>
#include <thread>
#include <iostream>

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
        XTS::Tick tick;
        tick.exchangeSegment = 2; // NSEFO
        tick.exchangeInstrumentID = data.token;
        tick.lastTradedPrice = data.ltp;
        tick.lastTradedQuantity = data.lastTradeQty;
        tick.volume = data.volume;
        tick.open = data.open;
        tick.high = data.high;
        tick.low = data.low;
        tick.close = data.close;
        tick.lastUpdateTime = data.lastTradeTime;
        tick.refNo = data.refNo;
        tick.timestampUdpRecv = data.timestampRecv;
        tick.timestampParsed = data.timestampParsed;
        tick.timestampQueued = LatencyTracker::now();
        
        m_totalTicks++;
        emit tickReceived(tick);
    });

    nsefo::MarketDataCallbackRegistry::instance().registerMarketDepthCallback([this](const nsefo::MarketDepthData& data) {
        // Debug: Log raw bid/ask data from NSEFO parser
        static int nsefoDepthLogCount = 0;
        if (nsefoDepthLogCount++ < 50) {
            qDebug() << "[UDP-NSEFO-DEPTH] Token:" << data.token
                     << "Bid:" << data.bids[0].price << "/" << data.bids[0].quantity
                     << "Ask:" << data.asks[0].price << "/" << data.asks[0].quantity;
        }
        
        XTS::Tick tick;
        tick.exchangeSegment = 2;
        tick.exchangeInstrumentID = data.token;
        if (data.bids[0].quantity > 0) {
            tick.bidPrice = data.bids[0].price;
            tick.bidQuantity = data.bids[0].quantity;
        }
        if (data.asks[0].quantity > 0) {
            tick.askPrice = data.asks[0].price;
            tick.askQuantity = data.asks[0].quantity;
        }
        tick.totalBuyQuantity = (int)data.totalBuyQty;
        tick.totalSellQuantity = (int)data.totalSellQty;
        tick.refNo = data.refNo;
        tick.timestampUdpRecv = data.timestampRecv;
        tick.timestampParsed = data.timestampParsed;
        
        m_totalTicks++;
        emit tickReceived(tick);
    });

    nsefo::MarketDataCallbackRegistry::instance().registerTickerCallback([this](const nsefo::TickerData& data) {
        if (data.fillVolume > 0) {
            XTS::Tick tick;
            tick.exchangeSegment = 2;
            tick.exchangeInstrumentID = data.token;
            tick.volume = data.fillVolume;
            tick.refNo = data.refNo;
            tick.timestampUdpRecv = data.timestampRecv;
            tick.timestampParsed = data.timestampParsed;
            
            m_totalTicks++;
            emit tickReceived(tick);
        }
    });
}

void UdpBroadcastService::setupNseCmCallbacks() {
    nsecm::MarketDataCallbackRegistry::instance().registerTouchlineCallback([this](const nsecm::TouchlineData& data) {
        XTS::Tick tick;
        tick.exchangeSegment = 1; // NSECM
        tick.exchangeInstrumentID = data.token;
        tick.lastTradedPrice = data.ltp;
        tick.lastTradedQuantity = data.lastTradeQty;
        tick.volume = (uint32_t)data.volume;
        tick.open = data.open;
        tick.high = data.high;
        tick.low = data.low;
        tick.close = data.close;
        tick.lastUpdateTime = data.lastTradeTime;
        tick.averagePrice = data.avgPrice;
        tick.refNo = data.refNo;
        tick.timestampUdpRecv = data.timestampRecv;
        tick.timestampParsed = data.timestampParsed;
        
        m_totalTicks++;
        emit tickReceived(tick);
    });

    nsecm::MarketDataCallbackRegistry::instance().registerMarketDepthCallback([this](const nsecm::MarketDepthData& data) {
        // Debug: Log raw bid/ask data from NSECM parser
        static int nsecmDepthLogCount = 0;
        if (nsecmDepthLogCount++ < 50) {
            qDebug() << "[UDP-NSECM-DEPTH] Token:" << data.token
                     << "Bid:" << data.bids[0].price << "/" << data.bids[0].quantity
                     << "Ask:" << data.asks[0].price << "/" << data.asks[0].quantity;
        }
        
        XTS::Tick tick;
        tick.exchangeSegment = 1;
        tick.exchangeInstrumentID = data.token;
        if (data.bids[0].quantity > 0) {
            tick.bidPrice = data.bids[0].price;
            tick.bidQuantity = (int)data.bids[0].quantity;
        }
        if (data.asks[0].quantity > 0) {
            tick.askPrice = data.asks[0].price;
            tick.askQuantity = (int)data.asks[0].quantity;
        }
        tick.totalBuyQuantity = (int)data.totalBuyQty;
        tick.totalSellQuantity = (int)data.totalSellQty;
        tick.refNo = data.refNo;
        tick.timestampUdpRecv = data.timestampRecv;
        tick.timestampParsed = data.timestampParsed;
        
        m_totalTicks++;
        emit tickReceived(tick);
    });
}

void UdpBroadcastService::setupBseFoCallbacks() {
    if (!m_bseFoReceiver) return;
    
    m_bseFoReceiver->setRecordCallback([this](const bse::DecodedRecord& record) {
        XTS::Tick tick;
        tick.exchangeSegment = 12; // BSEFO
        tick.exchangeInstrumentID = record.token;
        tick.lastTradedPrice = record.ltp / 100.0;
        tick.lastTradedQuantity = (int)record.ltq;
        tick.volume = (uint32_t)record.volume;
        tick.open = record.open / 100.0;
        tick.high = record.high / 100.0;
        tick.low = record.low / 100.0;
        tick.close = record.close / 100.0;
        tick.averagePrice = record.weightedAvgPrice / 100.0;
        
        if (!record.bids.empty()) {
            tick.bidPrice = record.bids[0].price / 100.0;
            tick.bidQuantity = (int)record.bids[0].quantity;
        }
        if (!record.asks.empty()) {
            tick.askPrice = record.asks[0].price / 100.0;
            tick.askQuantity = (int)record.asks[0].quantity;
        }
        tick.timestampUdpRecv = record.packetTimestamp;
        
        // Debug logging for BSE FO
        static int bseFoLog = 0;
        if (bseFoLog++ < 10) {
            qDebug() << "[UDP] BSE FO Tick - Token:" << record.token << "LTP:" << tick.lastTradedPrice;
        }
        
        m_totalTicks++;
        emit tickReceived(tick);
    });
}

void UdpBroadcastService::setupBseCmCallbacks() {
    if (!m_bseCmReceiver) return;
    
    m_bseCmReceiver->setRecordCallback([this](const bse::DecodedRecord& record) {
        XTS::Tick tick;
        tick.exchangeSegment = 11; // BSECM
        tick.exchangeInstrumentID = record.token;
        tick.lastTradedPrice = record.ltp / 100.0;
        tick.lastTradedQuantity = (int)record.ltq;
        tick.volume = (uint32_t)record.volume;
        tick.open = record.open / 100.0;
        tick.high = record.high / 100.0;
        tick.low = record.low / 100.0;
        tick.close = record.close / 100.0;
        tick.averagePrice = record.weightedAvgPrice / 100.0;
        
        if (!record.bids.empty()) {
            tick.bidPrice = record.bids[0].price / 100.0;
            tick.bidQuantity = (int)record.bids[0].quantity;
        }
        if (!record.asks.empty()) {
            tick.askPrice = record.asks[0].price / 100.0;
            tick.askQuantity = (int)record.asks[0].quantity;
        }
        tick.timestampUdpRecv = record.packetTimestamp;
        
        // Debug logging for BSE CM
        static int bseCmLog = 0;
        if (bseCmLog++ < 5) {
            qDebug() << "[UDP] BSE CM Tick - Token:" << record.token << "LTP:" << tick.lastTradedPrice;
        }
        
        m_totalTicks++;
        emit tickReceived(tick);
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

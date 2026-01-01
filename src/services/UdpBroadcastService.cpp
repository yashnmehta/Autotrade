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

void UdpBroadcastService::start(const Config& config) {
    if (m_active) return;

    qDebug() << "[UdpBroadcastService] Initializing broadcast segments...";
    
    try {
        bool anyEnabled = false;

        // 1. NSE FO Setup
        if (config.enableNSEFO) {
            m_nseFoReceiver = std::make_unique<nsefo::MulticastReceiver>(config.nseFoIp, config.nseFoPort);
            if (m_nseFoReceiver->isValid()) {
                anyEnabled = true;
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

                std::thread([this]() {
                    try { if (m_nseFoReceiver) m_nseFoReceiver->start(); }
                    catch (...) { qCritical() << "[UdpBroadcastService] NSE FO Thread crashed"; }
                }).detach();
                qDebug() << "  -> NSE FO: STARTED on" << QString::fromStdString(config.nseFoIp) << ":" << config.nseFoPort;
            }
        }

        // 2. NSE CM Setup
        if (config.enableNSECM) {
            m_nseCmReceiver = std::make_unique<nsecm::MulticastReceiver>(config.nseCmIp, config.nseCmPort);
            if (m_nseCmReceiver->isValid()) {
                anyEnabled = true;
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

                std::thread([this]() {
                    try { if (m_nseCmReceiver) m_nseCmReceiver->start(); }
                    catch (...) { qCritical() << "[UdpBroadcastService] NSE CM Thread crashed"; }
                }).detach();
                qDebug() << "  -> NSE CM: STARTED on" << QString::fromStdString(config.nseCmIp) << ":" << config.nseCmPort;
            }
        }

        // 3. BSE FO Setup
        if (config.enableBSEFO) {
            m_bseFoReceiver = std::make_unique<bse::BSEReceiver>(config.bseFoIp, config.bseFoPort, "BSEFO");
            // BSEReceiver socket init is in constructor, check sockfd_ is -1 in header or something? 
            // In bse_receiver.cpp, it prints errors if bind/etc fails. 
            // We'll assume if we could create it, we start it.
            anyEnabled = true;
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
                
                if (record.token == 832146 || record.token == 832178 || 
                    (record.token >= 500000 && record.token < 600000) ||
                    (record.token >= 800000 && record.token < 900000)) {
                    qDebug() << "[UDP] BSE FO Tick received - Token:" << record.token << "LTP:" << (record.ltp / 100.0);
                }
                
                m_totalTicks++;
                emit tickReceived(tick);
            });

            std::thread([this]() {
                try { if (m_bseFoReceiver) m_bseFoReceiver->start(); }
                catch (...) { qCritical() << "[UdpBroadcastService] BSE FO Thread crashed"; }
            }).detach();
            qDebug() << "  -> BSE FO: STARTED on" << QString::fromStdString(config.bseFoIp) << ":" << config.bseFoPort;
        }

        // 4. BSE CM Setup
        if (config.enableBSECM && !config.bseCmIp.empty()) {
            m_bseCmReceiver = std::make_unique<bse::BSEReceiver>(config.bseCmIp, config.bseCmPort, "BSECM");
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
                
                if (record.token >= 500000 && record.token < 600000) {
                    static int bseCmLog = 0;
                    if (bseCmLog++ < 1) {
                        qDebug() << "[UDP] BSE CM Tick received - Token:" << record.token << "LTP:" << tick.lastTradedPrice;
                    }
                }
                
                m_totalTicks++;
                emit tickReceived(tick);
            });

            std::thread([this]() {
                try { if (m_bseCmReceiver) m_bseCmReceiver->start(); }
                catch (...) { qCritical() << "[UdpBroadcastService] BSE CM Thread crashed"; }
            }).detach();
            qDebug() << "  -> BSE CM: STARTED on" << QString::fromStdString(config.bseCmIp) << ":" << config.bseCmPort;
        }

        m_active = anyEnabled;
        emit statusChanged(m_active);
    } catch (const std::exception& e) {
        qCritical() << "[UdpBroadcastService] Initialization failure:" << e.what();
        emit statusChanged(false);
    }
}

void UdpBroadcastService::stop() {
    if (!m_active) return;
    
    if (m_nseFoReceiver) m_nseFoReceiver->stop();
    if (m_nseCmReceiver) m_nseCmReceiver->stop();
    if (m_bseFoReceiver) m_bseFoReceiver->stop();

    // Small delay for threads to wind down
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    m_nseFoReceiver.reset();
    m_nseCmReceiver.reset();
    m_bseFoReceiver.reset();
    m_bseCmReceiver.reset();

    m_active = false;
    emit statusChanged(false);
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
    return s;
}

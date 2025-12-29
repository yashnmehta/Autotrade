#include "services/UdpBroadcastService.h"
#include "market_data_callback.h"
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

void UdpBroadcastService::start(const std::string& ip, int port) {
    if (m_active) return;

    qDebug() << "[UdpBroadcastService] Starting receiver on" << QString::fromStdString(ip) << ":" << port;
    m_receiver = std::make_unique<MulticastReceiver>(ip, port);
    
    if (!m_receiver->isValid()) {
        emit statusChanged(false);
        return;
    }

    m_active = true;

    // Register callbacks
    MarketDataCallbackRegistry::instance().registerTouchlineCallback([this](const TouchlineData& data) {
        m_msg7200Count++;
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
        tick.timestampDequeued = LatencyTracker::now();

        if (data.token == 49543) {
             std::cout << "[UDP-TOUCHLINE] Token: 49543 | RefNo: " << tick.refNo
                       << " | LTP: " << tick.lastTradedPrice << std::endl;
        }

        emit tickReceived(tick);
    });

    MarketDataCallbackRegistry::instance().registerMarketDepthCallback([this](const MarketDepthData& data) {
        m_depthCount++;
        XTS::Tick tick;
        tick.exchangeSegment = 2;
        tick.exchangeInstrumentID = data.token;
        if (!data.bids.empty()) {
            tick.bidPrice = data.bids[0].price;
            tick.bidQuantity = data.bids[0].quantity;
        }
        if (!data.asks.empty()) {
            tick.askPrice = data.asks[0].price;
            tick.askQuantity = data.asks[0].quantity;
        }
        tick.totalBuyQuantity = (int)data.totalBuyQty;
        tick.totalSellQuantity = (int)data.totalSellQty;
        tick.refNo = data.refNo;
        tick.timestampUdpRecv = data.timestampRecv;
        tick.timestampParsed = data.timestampParsed;
        tick.timestampQueued = LatencyTracker::now();
        tick.timestampDequeued = LatencyTracker::now();

        emit tickReceived(tick);
    });

    MarketDataCallbackRegistry::instance().registerTickerCallback([this](const TickerData& data) {
        m_msg7202Count++;
        if (data.fillVolume > 0) {
            XTS::Tick tick;
            tick.exchangeSegment = 2;
            tick.exchangeInstrumentID = data.token;
            tick.volume = data.fillVolume;
            tick.refNo = data.refNo;
            tick.timestampUdpRecv = data.timestampRecv;
            tick.timestampParsed = data.timestampParsed;
            tick.timestampQueued = LatencyTracker::now();
            tick.timestampDequeued = LatencyTracker::now();
            emit tickReceived(tick);
        }
    });

    std::thread([this]() {
        m_receiver->start();
    }).detach();

    emit statusChanged(true);
}

void UdpBroadcastService::stop() {
    if (!m_active) return;
    
    if (m_receiver) {
        m_receiver->stop();
        // Allow time for thread to exit
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m_receiver.reset();
    }
    m_active = false;
    emit statusChanged(false);
}

bool UdpBroadcastService::isActive() const {
    return m_active;
}

UdpBroadcastService::Stats UdpBroadcastService::getStats() const {
    Stats s;
    s.msg7200Count = m_msg7200Count.load();
    s.msg7201Count = m_msg7201Count.load();
    s.msg7202Count = m_msg7202Count.load();
    s.depthCount = m_depthCount.load();
    if (m_receiver) s.udpStats = m_receiver->getStats();
    return s;
}

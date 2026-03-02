/**
 * @file test_trading_data_service.cpp
 * @brief Unit tests for TradingDataService — thread-safe concurrent access
 *
 * Tests:
 *  - Basic CRUD for positions, orders, trades
 *  - Thread-safe concurrent getters/setters
 *  - Event-driven updates (onOrderEvent, onTradeEvent, onPositionEvent)
 *  - Deduplication & upsert logic
 *  - Signal emission on data changes
 *  - clearAll behavior
 *
 * Build: Requires Qt5::Core, Qt5::Test
 *        Compile with TradingDataService.cpp
 */

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QThread>
#include <QVector>
#include <QMutex>
#include <atomic>
#include "services/TradingDataService.h"

// ─── Helpers ─────────────────────────────────────────────

namespace {

XTS::Order makeOrder(int64_t appOrderID, const QString& status = "New",
                     int qty = 100, int filledQty = 0) {
    XTS::Order o;
    o.appOrderID = appOrderID;
    o.exchangeOrderID = QString("EX%1").arg(appOrderID);
    o.clientID = "TEST_CLIENT";
    o.loginID = "TEST_LOGIN";
    o.exchangeSegment = "NSEFO";
    o.exchangeInstrumentID = 49508;
    o.tradingSymbol = "NIFTY 50";
    o.orderSide = "BUY";
    o.orderType = "LIMIT";
    o.orderPrice = 20000.0;
    o.orderStopPrice = 0.0;
    o.orderQuantity = qty;
    o.cumulativeQuantity = filledQty;
    o.leavesQuantity = qty - filledQty;
    o.orderStatus = status;
    o.orderAverageTradedPrice = filledQty > 0 ? 20005.0 : 0.0;
    o.productType = "MIS";
    o.timeInForce = "DAY";
    o.orderGeneratedDateTime = "2026-02-28T10:00:00";
    return o;
}

XTS::Trade makeTrade(const QString& execID, int64_t appOrderID,
                     double price = 20005.0, int qty = 50) {
    XTS::Trade t;
    t.executionID = execID;
    t.appOrderID = appOrderID;
    t.exchangeOrderID = QString("EX%1").arg(appOrderID);
    t.clientID = "TEST_CLIENT";
    t.loginID = "TEST_LOGIN";
    t.exchangeSegment = "NSEFO";
    t.exchangeInstrumentID = 49508;
    t.tradingSymbol = "NIFTY 50";
    t.orderSide = "BUY";
    t.orderType = "LIMIT";
    t.lastTradedPrice = price;
    t.lastTradedQuantity = qty;
    t.lastExecutionTransactTime = "2026-02-28T10:05:00";
    t.orderAverageTradedPrice = price;
    t.cumulativeQuantity = qty;
    t.leavesQuantity = 50;
    t.orderStatus = "PartiallyFilled";
    t.productType = "MIS";
    t.orderPrice = 20000.0;
    t.orderQuantity = 100;
    return t;
}

XTS::Position makePosition(int64_t instrumentID, const QString& segment = "NSEFO",
                           const QString& product = "MIS", int qty = 50) {
    XTS::Position p;
    p.accountID = "TEST_ACCOUNT";
    p.exchangeInstrumentID = instrumentID;
    p.exchangeSegment = segment;
    p.productType = product;
    p.quantity = qty;
    p.buyAmount = 1000000.0;
    p.buyAveragePrice = 20000.0;
    p.sellAmount = 0.0;
    p.sellAveragePrice = 0.0;
    p.mtm = 250.0;
    p.realizedMTM = 0.0;
    p.unrealizedMTM = 250.0;
    p.bep = 20000.0;
    p.openBuyQuantity = qty;
    p.openSellQuantity = 0;
    p.netAmount = -1000000.0;
    p.marketLot = 50;
    p.multiplier = 1.0;
    p.tradingSymbol = "NIFTY 50";
    p.loginID = "TEST_LOGIN";
    return p;
}

} // anonymous namespace

// ─── Test Class ──────────────────────────────────────────

class TestTradingDataService : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    // Basic CRUD
    void testSetAndGetPositions();
    void testSetAndGetOrders();
    void testSetAndGetTrades();
    void testClearAll();

    // Signal emission
    void testPositionsSignal();
    void testOrdersSignal();
    void testTradesSignal();
    void testClearAllEmitsSignals();

    // Event-driven updates
    void testOnOrderEvent_newOrder();
    void testOnOrderEvent_updateExisting();
    void testOnTradeEvent_appends();
    void testOnPositionEvent_newPosition();
    void testOnPositionEvent_updateExisting();

    // Edge cases
    void testEmptyGetters();
    void testOverwriteWithEmpty();
    void testLargeDataSet();

    // Thread safety
    void testConcurrentReadsAndWrites();
    void testConcurrentOrderEvents();

private:
    TradingDataService* m_service = nullptr;
};

// ─── Setup / Teardown ────────────────────────────────────

void TestTradingDataService::initTestCase() {
    // Nothing global needed
}

void TestTradingDataService::init() {
    m_service = new TradingDataService(nullptr);
}

void TestTradingDataService::cleanup() {
    delete m_service;
    m_service = nullptr;
}

// ─── Basic CRUD ──────────────────────────────────────────

void TestTradingDataService::testSetAndGetPositions() {
    QVector<XTS::Position> positions;
    positions.append(makePosition(49508));
    positions.append(makePosition(49509, "NSEFO", "NRML", 100));

    m_service->setPositions(positions);
    auto result = m_service->getPositions();

    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].exchangeInstrumentID, (int64_t)49508);
    QCOMPARE(result[1].exchangeInstrumentID, (int64_t)49509);
    QCOMPARE(result[1].quantity, 100);
}

void TestTradingDataService::testSetAndGetOrders() {
    QVector<XTS::Order> orders;
    orders.append(makeOrder(1001));
    orders.append(makeOrder(1002, "Filled", 100, 100));
    orders.append(makeOrder(1003, "Cancelled"));

    m_service->setOrders(orders);
    auto result = m_service->getOrders();

    QCOMPARE(result.size(), 3);
    QCOMPARE(result[0].appOrderID, (int64_t)1001);
    QCOMPARE(result[1].orderStatus, QString("Filled"));
    QCOMPARE(result[2].orderStatus, QString("Cancelled"));
}

void TestTradingDataService::testSetAndGetTrades() {
    QVector<XTS::Trade> trades;
    trades.append(makeTrade("EXEC001", 1001, 20005.0, 50));
    trades.append(makeTrade("EXEC002", 1001, 20010.0, 50));

    m_service->setTrades(trades);
    auto result = m_service->getTrades();

    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].executionID, QString("EXEC001"));
    QCOMPARE(result[1].lastTradedPrice, 20010.0);
}

void TestTradingDataService::testClearAll() {
    m_service->setPositions({ makePosition(49508) });
    m_service->setOrders({ makeOrder(1001) });
    m_service->setTrades({ makeTrade("EXEC001", 1001) });

    // Verify data exists
    QCOMPARE(m_service->getPositions().size(), 1);
    QCOMPARE(m_service->getOrders().size(), 1);
    QCOMPARE(m_service->getTrades().size(), 1);

    m_service->clearAll();

    QCOMPARE(m_service->getPositions().size(), 0);
    QCOMPARE(m_service->getOrders().size(), 0);
    QCOMPARE(m_service->getTrades().size(), 0);
}

// ─── Signal Emission ─────────────────────────────────────

void TestTradingDataService::testPositionsSignal() {
    qRegisterMetaType<QVector<XTS::Position>>("QVector<XTS::Position>");
    QSignalSpy spy(m_service, &TradingDataService::positionsUpdated);
    QVERIFY(spy.isValid());

    QVector<XTS::Position> positions = { makePosition(49508) };
    m_service->setPositions(positions);

    QCOMPARE(spy.count(), 1);
    auto args = spy.takeFirst();
    auto emitted = args.at(0).value<QVector<XTS::Position>>();
    QCOMPARE(emitted.size(), 1);
    QCOMPARE(emitted[0].exchangeInstrumentID, (int64_t)49508);
}

void TestTradingDataService::testOrdersSignal() {
    qRegisterMetaType<QVector<XTS::Order>>("QVector<XTS::Order>");
    QSignalSpy spy(m_service, &TradingDataService::ordersUpdated);
    QVERIFY(spy.isValid());

    m_service->setOrders({ makeOrder(2001), makeOrder(2002) });

    QCOMPARE(spy.count(), 1);
    auto emitted = spy.takeFirst().at(0).value<QVector<XTS::Order>>();
    QCOMPARE(emitted.size(), 2);
}

void TestTradingDataService::testTradesSignal() {
    qRegisterMetaType<QVector<XTS::Trade>>("QVector<XTS::Trade>");
    QSignalSpy spy(m_service, &TradingDataService::tradesUpdated);
    QVERIFY(spy.isValid());

    m_service->setTrades({ makeTrade("E1", 100) });

    QCOMPARE(spy.count(), 1);
    auto emitted = spy.takeFirst().at(0).value<QVector<XTS::Trade>>();
    QCOMPARE(emitted.size(), 1);
}

void TestTradingDataService::testClearAllEmitsSignals() {
    qRegisterMetaType<QVector<XTS::Position>>("QVector<XTS::Position>");
    qRegisterMetaType<QVector<XTS::Order>>("QVector<XTS::Order>");
    qRegisterMetaType<QVector<XTS::Trade>>("QVector<XTS::Trade>");

    m_service->setPositions({ makePosition(49508) });
    m_service->setOrders({ makeOrder(1001) });
    m_service->setTrades({ makeTrade("E1", 1001) });

    QSignalSpy posSpy(m_service, &TradingDataService::positionsUpdated);
    QSignalSpy ordSpy(m_service, &TradingDataService::ordersUpdated);
    QSignalSpy trdSpy(m_service, &TradingDataService::tradesUpdated);

    m_service->clearAll();

    QCOMPARE(posSpy.count(), 1);
    QCOMPARE(ordSpy.count(), 1);
    QCOMPARE(trdSpy.count(), 1);

    // The emitted data should be empty
    QCOMPARE(posSpy.takeFirst().at(0).value<QVector<XTS::Position>>().size(), 0);
    QCOMPARE(ordSpy.takeFirst().at(0).value<QVector<XTS::Order>>().size(), 0);
    QCOMPARE(trdSpy.takeFirst().at(0).value<QVector<XTS::Trade>>().size(), 0);
}

// ─── Event-Driven Updates ────────────────────────────────

void TestTradingDataService::testOnOrderEvent_newOrder() {
    qRegisterMetaType<QVector<XTS::Order>>("QVector<XTS::Order>");
    QSignalSpy spy(m_service, &TradingDataService::ordersUpdated);

    m_service->onOrderEvent(makeOrder(5001));

    QCOMPARE(m_service->getOrders().size(), 1);
    QCOMPARE(m_service->getOrders()[0].appOrderID, (int64_t)5001);
    QCOMPARE(spy.count(), 1);
}

void TestTradingDataService::testOnOrderEvent_updateExisting() {
    qRegisterMetaType<QVector<XTS::Order>>("QVector<XTS::Order>");

    // Add initial order
    m_service->onOrderEvent(makeOrder(5001, "New", 100, 0));
    QCOMPARE(m_service->getOrders().size(), 1);
    QCOMPARE(m_service->getOrders()[0].orderStatus, QString("New"));

    // Update same order (same appOrderID) — should upsert, not duplicate
    m_service->onOrderEvent(makeOrder(5001, "PartiallyFilled", 100, 50));
    auto orders = m_service->getOrders();
    QCOMPARE(orders.size(), 1);  // Still 1, not 2
    QCOMPARE(orders[0].orderStatus, QString("PartiallyFilled"));
    QCOMPARE(orders[0].cumulativeQuantity, 50);

    // Update to Filled
    m_service->onOrderEvent(makeOrder(5001, "Filled", 100, 100));
    orders = m_service->getOrders();
    QCOMPARE(orders.size(), 1);
    QCOMPARE(orders[0].orderStatus, QString("Filled"));

    // Add a different order
    m_service->onOrderEvent(makeOrder(5002, "New"));
    QCOMPARE(m_service->getOrders().size(), 2);
}

void TestTradingDataService::testOnTradeEvent_appends() {
    qRegisterMetaType<QVector<XTS::Trade>>("QVector<XTS::Trade>");

    // Trades are always appended (they're a log)
    m_service->onTradeEvent(makeTrade("EXEC1", 5001, 20005.0, 50));
    QCOMPARE(m_service->getTrades().size(), 1);

    m_service->onTradeEvent(makeTrade("EXEC2", 5001, 20010.0, 50));
    QCOMPARE(m_service->getTrades().size(), 2);

    // Even same execution ID is appended (no dedup for trades)
    m_service->onTradeEvent(makeTrade("EXEC1", 5001, 20005.0, 50));
    QCOMPARE(m_service->getTrades().size(), 3);
}

void TestTradingDataService::testOnPositionEvent_newPosition() {
    qRegisterMetaType<QVector<XTS::Position>>("QVector<XTS::Position>");
    QSignalSpy spy(m_service, &TradingDataService::positionsUpdated);

    m_service->onPositionEvent(makePosition(49508, "NSEFO", "MIS", 50));

    QCOMPARE(m_service->getPositions().size(), 1);
    QCOMPARE(m_service->getPositions()[0].quantity, 50);
    QCOMPARE(spy.count(), 1);
}

void TestTradingDataService::testOnPositionEvent_updateExisting() {
    qRegisterMetaType<QVector<XTS::Position>>("QVector<XTS::Position>");

    // Add initial position
    m_service->onPositionEvent(makePosition(49508, "NSEFO", "MIS", 50));
    QCOMPARE(m_service->getPositions().size(), 1);

    // Update same position (same instrumentID + segment + product) — upsert
    auto updated = makePosition(49508, "NSEFO", "MIS", 100);
    updated.mtm = 500.0;
    m_service->onPositionEvent(updated);
    auto positions = m_service->getPositions();
    QCOMPARE(positions.size(), 1);  // Still 1
    QCOMPARE(positions[0].quantity, 100);
    QCOMPARE(positions[0].mtm, 500.0);

    // Different product type → new position
    m_service->onPositionEvent(makePosition(49508, "NSEFO", "NRML", 25));
    QCOMPARE(m_service->getPositions().size(), 2);

    // Different instrument → new position
    m_service->onPositionEvent(makePosition(49509, "NSEFO", "MIS", 75));
    QCOMPARE(m_service->getPositions().size(), 3);

    // Different segment → new position
    m_service->onPositionEvent(makePosition(49508, "BSEFO", "MIS", 10));
    QCOMPARE(m_service->getPositions().size(), 4);
}

// ─── Edge Cases ──────────────────────────────────────────

void TestTradingDataService::testEmptyGetters() {
    QCOMPARE(m_service->getPositions().size(), 0);
    QCOMPARE(m_service->getOrders().size(), 0);
    QCOMPARE(m_service->getTrades().size(), 0);
}

void TestTradingDataService::testOverwriteWithEmpty() {
    m_service->setOrders({ makeOrder(1), makeOrder(2), makeOrder(3) });
    QCOMPARE(m_service->getOrders().size(), 3);

    // Overwrite with empty
    m_service->setOrders({});
    QCOMPARE(m_service->getOrders().size(), 0);
}

void TestTradingDataService::testLargeDataSet() {
    QVector<XTS::Order> orders;
    for (int i = 0; i < 10000; ++i) {
        orders.append(makeOrder(i, "New", 100, 0));
    }

    m_service->setOrders(orders);
    auto result = m_service->getOrders();
    QCOMPARE(result.size(), 10000);
    QCOMPARE(result[9999].appOrderID, (int64_t)9999);
}

// ─── Thread Safety ───────────────────────────────────────

void TestTradingDataService::testConcurrentReadsAndWrites() {
    // Stress test: multiple writer threads and reader threads
    const int NUM_WRITERS = 4;
    const int NUM_READERS = 4;
    const int OPS_PER_THREAD = 500;
    std::atomic<int> totalWritesDone{0};
    std::atomic<int> totalReadsDone{0};
    std::atomic<bool> failed{false};

    QVector<QThread*> threads;

    // Writer threads — set positions, orders, trades concurrently
    for (int w = 0; w < NUM_WRITERS; ++w) {
        QThread* t = QThread::create([this, w, &totalWritesDone, &failed, OPS_PER_THREAD]() {
            for (int i = 0; i < OPS_PER_THREAD && !failed.load(); ++i) {
                int instrumentBase = w * 10000 + i;
                try {
                    m_service->setPositions({ makePosition(instrumentBase) });
                    m_service->setOrders({ makeOrder(instrumentBase) });
                    m_service->setTrades({ makeTrade(QString("E%1_%2").arg(w).arg(i), instrumentBase) });
                    totalWritesDone.fetch_add(1);
                } catch (...) {
                    failed.store(true);
                }
            }
        });
        threads.append(t);
    }

    // Reader threads — get data concurrently
    for (int r = 0; r < NUM_READERS; ++r) {
        QThread* t = QThread::create([this, &totalReadsDone, &failed, OPS_PER_THREAD]() {
            for (int i = 0; i < OPS_PER_THREAD && !failed.load(); ++i) {
                try {
                    auto pos = m_service->getPositions();
                    auto ord = m_service->getOrders();
                    auto trd = m_service->getTrades();
                    // Just verify no crash / corruption
                    (void)pos.size();
                    (void)ord.size();
                    (void)trd.size();
                    totalReadsDone.fetch_add(1);
                } catch (...) {
                    failed.store(true);
                }
            }
        });
        threads.append(t);
    }

    // Start all threads
    for (auto* t : threads) t->start();

    // Wait for all to finish (timeout 30s)
    for (auto* t : threads) {
        bool finished = t->wait(30000);
        QVERIFY2(finished, "Thread didn't finish within 30 seconds");
    }

    // Cleanup
    qDeleteAll(threads);

    QVERIFY2(!failed.load(), "A thread threw an exception");
    QCOMPARE(totalWritesDone.load(), NUM_WRITERS * OPS_PER_THREAD);
    QCOMPARE(totalReadsDone.load(), NUM_READERS * OPS_PER_THREAD);
}

void TestTradingDataService::testConcurrentOrderEvents() {
    // Stress test specifically for onOrderEvent upsert logic
    const int NUM_THREADS = 4;
    const int EVENTS_PER_THREAD = 200;
    std::atomic<bool> failed{false};

    // Pre-seed with some orders
    QVector<XTS::Order> initial;
    for (int i = 0; i < 10; ++i) {
        initial.append(makeOrder(i, "New"));
    }
    m_service->setOrders(initial);

    QVector<QThread*> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        QThread* thread = QThread::create([this, t, &failed, EVENTS_PER_THREAD]() {
            for (int i = 0; i < EVENTS_PER_THREAD && !failed.load(); ++i) {
                try {
                    // Update existing orders and add new ones
                    int64_t orderID = (i % 10);  // Update one of the 10 initial orders
                    m_service->onOrderEvent(makeOrder(orderID,
                        i % 2 == 0 ? "PartiallyFilled" : "Filled",
                        100, i % 2 == 0 ? 50 : 100));
                } catch (...) {
                    failed.store(true);
                }
            }
        });
        threads.append(thread);
    }

    for (auto* t : threads) t->start();
    for (auto* t : threads) {
        QVERIFY(t->wait(30000));
    }
    qDeleteAll(threads);

    QVERIFY2(!failed.load(), "A thread threw an exception during order events");

    // Should still have exactly 10 orders (all upserts, no new IDs added)
    auto orders = m_service->getOrders();
    QCOMPARE(orders.size(), 10);
}

// ─── Main ────────────────────────────────────────────────

QTEST_MAIN(TestTradingDataService)
#include "test_trading_data_service.moc"

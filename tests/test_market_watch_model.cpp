/**
 * @file test_market_watch_model.cpp
 * @brief Unit tests for MarketWatchModel — add/remove/duplicate/blank rows, price updates
 *
 * Tests:
 *  - Add/remove/insert scrips
 *  - Duplicate rejection (same exchange + token)
 *  - Blank separator rows
 *  - findScrip / findScripByToken lookup
 *  - moveRow
 *  - Price update methods (LTP, bid/ask, volume, OHLC, OI, Greeks)
 *  - Tick direction tracking (up/down)
 *  - Column profile switching
 *  - Signal emission
 *  - clearAll / overwrite
 *
 * Build: Requires Qt5::Core, Qt5::Test, Qt5::Gui (for QColor in data())
 *        Compiles MarketWatchModel.cpp + MarketWatchColumnProfile.cpp
 *        Provides a stub GreeksCalculationService singleton
 */

// ─── Stub GreeksCalculationService ──────────────────────
// MarketWatchModel.cpp #includes GreeksCalculationService.h and calls
// instance() in its constructor.  We provide a minimal stub here so
// the test links without pulling in the real service (which drags in
// RepositoryManager, PriceStoreGateway, TimeToExpiry, etc.).
//
// This file is compiled WITH MarketWatchModel.cpp, so we need to
// provide the singleton function and a constructor/destructor.

#include "services/GreeksCalculationService.h"

// Provide the singleton — just a static local
GreeksCalculationService& GreeksCalculationService::instance() {
    static GreeksCalculationService inst;
    return inst;
}

// Minimal constructor — no timers, no repo, no holidays
GreeksCalculationService::GreeksCalculationService(QObject* parent)
    : QObject(parent), m_timeTickTimer(nullptr), m_illiquidUpdateTimer(nullptr)
{
    // no-op for test stub
}

GreeksCalculationService::~GreeksCalculationService() = default;

// Stubs for any methods the .h exposes as non-inline and might be referenced
void GreeksCalculationService::initialize(const GreeksConfig&) {}
void GreeksCalculationService::loadConfiguration() {}
void GreeksCalculationService::setRepositoryManager(RepositoryManager*) {}
GreeksResult GreeksCalculationService::calculateForToken(uint32_t, int) { return {}; }
GreeksValidationResult GreeksCalculationService::validateGreeksInputs(uint32_t, int, double) const { return {}; }
std::optional<GreeksResult> GreeksCalculationService::getCachedGreeks(uint32_t) const { return std::nullopt; }
void GreeksCalculationService::forceRecalculateAll() {}
void GreeksCalculationService::clearCache() {}
void GreeksCalculationService::setRiskFreeRate(double) {}
void GreeksCalculationService::onPriceUpdate(uint32_t, double, int) {}
void GreeksCalculationService::onUnderlyingPriceUpdate(uint32_t, double, int) {}
void GreeksCalculationService::onTimeTick() {}
void GreeksCalculationService::processIlliquidUpdates() {}
double GreeksCalculationService::getUnderlyingPrice(uint32_t, int) { return 0.0; }
double GreeksCalculationService::calculateTimeToExpiry(const QString&) { return 0.0; }
double GreeksCalculationService::calculateTimeToExpiry(const QDate&) { return 0.0; }
int GreeksCalculationService::calculateTradingDays(const QDate&, const QDate&) { return 0; }
bool GreeksCalculationService::isNSETradingDay(const QDate&) { return true; }
bool GreeksCalculationService::shouldRecalculate(uint32_t, double, double) { return false; }
bool GreeksCalculationService::isOption(int) { return false; }
void GreeksCalculationService::loadNSEHolidays() {}

// ─── Now the actual test ─────────────────────────────────

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/qt/MarketWatchModel.h"

// ─── Helpers ─────────────────────────────────────────────

namespace {

ScripData makeScrip(const QString& symbol, int token, const QString& exchange = "NSE") {
    ScripData s;
    s.symbol = symbol;
    s.token = token;
    s.exchange = exchange;
    s.scripName = symbol;
    s.instrumentName = symbol;
    s.instrumentType = "EQUITY";
    s.marketType = "Normal";
    return s;
}

ScripData makeFOScrip(const QString& symbol, int token, double strike,
                      const QString& optType, const QString& expiry) {
    ScripData s = makeScrip(symbol, token, "NSE");
    s.instrumentType = "OPTSTK";
    s.strikePrice = strike;
    s.optionType = optType;
    s.seriesExpiry = expiry;
    return s;
}

} // anonymous namespace

// ─── Test Class ──────────────────────────────────────────

class TestMarketWatchModel : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // Add / Insert / Remove
    void testAddScrip();
    void testAddMultipleScrips();
    void testInsertScrip();
    void testRemoveScrip();
    void testRemoveMiddleRow();

    // Duplicate rejection
    void testRejectDuplicate_sameExchangeAndToken();
    void testAllowSameToken_differentExchange();

    // Blank rows
    void testInsertBlankRow();
    void testBlankRowIsNotCountedAsScrip();
    void testBlankRowData();

    // Find / Lookup
    void testFindScrip();
    void testFindScripByToken();
    void testFindScrip_notFound();
    void testFindScripByToken_invalid();

    // Move
    void testMoveRowDown();
    void testMoveRowUp();
    void testMoveRow_invalidIndices();

    // Clear
    void testClearAll();

    // Price updates
    void testUpdatePrice();
    void testUpdatePrice_tickDirection();
    void testUpdateBidAsk();
    void testUpdateBidAsk_tickDirection();
    void testUpdateVolume();
    void testUpdateHighLow();
    void testUpdateOHLC();
    void testUpdateOpenInterest();
    void testUpdateAveragePrice();
    void testUpdateGreeks();
    void testUpdateScripData();

    // Bounds checks
    void testUpdatePrice_outOfBounds();
    void testUpdatePrice_blankRow();

    // Model interface
    void testRowCount();
    void testColumnCount();
    void testDataRole_display();
    void testDataRole_userRole();

    // Signals
    void testScripAddedSignal();
    void testScripRemovedSignal();
    void testPriceUpdatedSignal();

    // Profile
    void testLoadProfile();
    void testAvailableProfiles();

    // ScripData helpers
    void testScripData_createBlankRow();
    void testScripData_isValid();

    // Scrip count
    void testScripCount_excludesBlanks();

private:
    MarketWatchModel* m_model = nullptr;
};

// ─── Setup / Teardown ────────────────────────────────────

void TestMarketWatchModel::init() {
    m_model = new MarketWatchModel(nullptr);
}

void TestMarketWatchModel::cleanup() {
    delete m_model;
    m_model = nullptr;
}

// ─── Add / Insert / Remove ───────────────────────────────

void TestMarketWatchModel::testAddScrip() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));
    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(m_model->getScripAt(0).symbol, QString("RELIANCE"));
    QCOMPARE(m_model->getScripAt(0).token, 2885);
}

void TestMarketWatchModel::testAddMultipleScrips() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));
    m_model->addScrip(makeScrip("TCS", 11536));
    m_model->addScrip(makeScrip("INFY", 1594));
    QCOMPARE(m_model->rowCount(), 3);
    QCOMPARE(m_model->getScripAt(0).symbol, QString("RELIANCE"));
    QCOMPARE(m_model->getScripAt(1).symbol, QString("TCS"));
    QCOMPARE(m_model->getScripAt(2).symbol, QString("INFY"));
}

void TestMarketWatchModel::testInsertScrip() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));
    m_model->addScrip(makeScrip("INFY", 1594));

    // Insert in the middle
    m_model->insertScrip(1, makeScrip("TCS", 11536));
    QCOMPARE(m_model->rowCount(), 3);
    QCOMPARE(m_model->getScripAt(0).symbol, QString("RELIANCE"));
    QCOMPARE(m_model->getScripAt(1).symbol, QString("TCS"));
    QCOMPARE(m_model->getScripAt(2).symbol, QString("INFY"));
}

void TestMarketWatchModel::testRemoveScrip() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));
    m_model->addScrip(makeScrip("TCS", 11536));
    QCOMPARE(m_model->rowCount(), 2);

    m_model->removeScrip(0);
    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(m_model->getScripAt(0).symbol, QString("TCS"));
}

void TestMarketWatchModel::testRemoveMiddleRow() {
    m_model->addScrip(makeScrip("A", 1));
    m_model->addScrip(makeScrip("B", 2));
    m_model->addScrip(makeScrip("C", 3));

    m_model->removeScrip(1);
    QCOMPARE(m_model->rowCount(), 2);
    QCOMPARE(m_model->getScripAt(0).symbol, QString("A"));
    QCOMPARE(m_model->getScripAt(1).symbol, QString("C"));
}

// ─── Duplicate Rejection ─────────────────────────────────

void TestMarketWatchModel::testRejectDuplicate_sameExchangeAndToken() {
    m_model->addScrip(makeScrip("RELIANCE", 2885, "NSE"));
    m_model->addScrip(makeScrip("RELIANCE", 2885, "NSE"));  // duplicate
    QCOMPARE(m_model->rowCount(), 1);  // Should still be 1
}

void TestMarketWatchModel::testAllowSameToken_differentExchange() {
    m_model->addScrip(makeScrip("RELIANCE", 2885, "NSE"));
    m_model->addScrip(makeScrip("RELIANCE", 2885, "BSE"));  // different exchange
    QCOMPARE(m_model->rowCount(), 2);  // Both should be added
}

// ─── Blank Rows ──────────────────────────────────────────

void TestMarketWatchModel::testInsertBlankRow() {
    m_model->addScrip(makeScrip("A", 1));
    m_model->insertBlankRow(1);
    m_model->addScrip(makeScrip("B", 2));

    QCOMPARE(m_model->totalRowCount(), 3);
    QVERIFY(!m_model->isBlankRow(0));
    QVERIFY(m_model->isBlankRow(1));
}

void TestMarketWatchModel::testBlankRowIsNotCountedAsScrip() {
    m_model->addScrip(makeScrip("A", 1));
    m_model->insertBlankRow();
    m_model->addScrip(makeScrip("B", 2));

    QCOMPARE(m_model->scripCount(), 2);     // excludes blanks
    QCOMPARE(m_model->totalRowCount(), 3);   // includes blanks
}

void TestMarketWatchModel::testBlankRowData() {
    m_model->insertBlankRow();

    auto idx = m_model->index(0, 0);
    // Display role for blank rows returns empty string (separator is painted via BackgroundRole)
    auto display = m_model->data(idx, Qt::DisplayRole);
    QCOMPARE(display.toString(), QString());

    // UserRole + 100 should be true (marks blank row for delegate)
    auto isBlank = m_model->data(idx, Qt::UserRole + 100);
    QCOMPARE(isBlank.toBool(), true);
}

// ─── Find / Lookup ───────────────────────────────────────

void TestMarketWatchModel::testFindScrip() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));
    m_model->addScrip(makeScrip("TCS", 11536));
    m_model->addScrip(makeScrip("INFY", 1594));

    QCOMPARE(m_model->findScrip("TCS"), 1);
    QCOMPARE(m_model->findScrip("INFY"), 2);
    QCOMPARE(m_model->findScrip("RELIANCE"), 0);
}

void TestMarketWatchModel::testFindScripByToken() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));
    m_model->addScrip(makeScrip("TCS", 11536));

    QCOMPARE(m_model->findScripByToken(2885), 0);
    QCOMPARE(m_model->findScripByToken(11536), 1);
}

void TestMarketWatchModel::testFindScrip_notFound() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));
    QCOMPARE(m_model->findScrip("NONEXISTENT"), -1);
}

void TestMarketWatchModel::testFindScripByToken_invalid() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));
    QCOMPARE(m_model->findScripByToken(0), -1);
    QCOMPARE(m_model->findScripByToken(-1), -1);
    QCOMPARE(m_model->findScripByToken(99999), -1);
}

// ─── Move ────────────────────────────────────────────────

void TestMarketWatchModel::testMoveRowDown() {
    m_model->addScrip(makeScrip("A", 1));
    m_model->addScrip(makeScrip("B", 2));
    m_model->addScrip(makeScrip("C", 3));

    m_model->moveRow(0, 2);  // Move A after B
    QCOMPARE(m_model->getScripAt(0).symbol, QString("B"));
    QCOMPARE(m_model->getScripAt(1).symbol, QString("A"));
    QCOMPARE(m_model->getScripAt(2).symbol, QString("C"));
}

void TestMarketWatchModel::testMoveRowUp() {
    m_model->addScrip(makeScrip("A", 1));
    m_model->addScrip(makeScrip("B", 2));
    m_model->addScrip(makeScrip("C", 3));

    m_model->moveRow(2, 0);  // Move C to top
    QCOMPARE(m_model->getScripAt(0).symbol, QString("C"));
    QCOMPARE(m_model->getScripAt(1).symbol, QString("A"));
    QCOMPARE(m_model->getScripAt(2).symbol, QString("B"));
}

void TestMarketWatchModel::testMoveRow_invalidIndices() {
    m_model->addScrip(makeScrip("A", 1));
    m_model->addScrip(makeScrip("B", 2));

    // Invalid source — should be no-op
    m_model->moveRow(-1, 0);
    m_model->moveRow(5, 0);
    QCOMPARE(m_model->getScripAt(0).symbol, QString("A"));
    QCOMPARE(m_model->getScripAt(1).symbol, QString("B"));
}

// ─── Clear ───────────────────────────────────────────────

void TestMarketWatchModel::testClearAll() {
    m_model->addScrip(makeScrip("A", 1));
    m_model->addScrip(makeScrip("B", 2));
    m_model->insertBlankRow();

    m_model->clearAll();
    QCOMPARE(m_model->rowCount(), 0);
    QCOMPARE(m_model->totalRowCount(), 0);
    QCOMPARE(m_model->scripCount(), 0);
}

// ─── Price Updates ───────────────────────────────────────

void TestMarketWatchModel::testUpdatePrice() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));

    m_model->updatePrice(0, 2500.50, 15.25, 0.61);

    const ScripData& s = m_model->getScripAt(0);
    QCOMPARE(s.ltp, 2500.50);
    QCOMPARE(s.change, 15.25);
    QCOMPARE(s.changePercent, 0.61);
}

void TestMarketWatchModel::testUpdatePrice_tickDirection() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));

    // First update establishes baseline
    m_model->updatePrice(0, 100.0, 0, 0);
    QCOMPARE(m_model->getScripAt(0).ltpTick, 0);  // no previous, no tick

    // Price up
    m_model->updatePrice(0, 105.0, 5.0, 5.0);
    QCOMPARE(m_model->getScripAt(0).ltpTick, 1);

    // Price down
    m_model->updatePrice(0, 98.0, -2.0, -2.0);
    QCOMPARE(m_model->getScripAt(0).ltpTick, -1);

    // Price same
    m_model->updatePrice(0, 98.0, -2.0, -2.0);
    // Tick should NOT change (implementation only sets tick on > or <)
    // Since 98 is not > 98 and not < 98, ltpTick stays at -1
    QCOMPARE(m_model->getScripAt(0).ltpTick, -1);
}

void TestMarketWatchModel::testUpdateBidAsk() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));

    m_model->updateBidAsk(0, 2499.0, 2501.0);

    const ScripData& s = m_model->getScripAt(0);
    QCOMPARE(s.bid, 2499.0);
    QCOMPARE(s.ask, 2501.0);
    QCOMPARE(s.buyPrice, 2499.0);   // buyPrice mirrors bid
    QCOMPARE(s.sellPrice, 2501.0);  // sellPrice mirrors ask
}

void TestMarketWatchModel::testUpdateBidAsk_tickDirection() {
    m_model->addScrip(makeScrip("X", 100));

    m_model->updateBidAsk(0, 100.0, 200.0);
    // Baseline — bid/ask were 0, so tick should be 0 (0 is not > 0, checked by impl)
    QCOMPARE(m_model->getScripAt(0).bidTick, 0);
    QCOMPARE(m_model->getScripAt(0).askTick, 0);

    // Bid up, ask up
    m_model->updateBidAsk(0, 105.0, 210.0);
    QCOMPARE(m_model->getScripAt(0).bidTick, 1);
    QCOMPARE(m_model->getScripAt(0).askTick, 1);

    // Bid down, ask down
    m_model->updateBidAsk(0, 99.0, 195.0);
    QCOMPARE(m_model->getScripAt(0).bidTick, -1);
    QCOMPARE(m_model->getScripAt(0).askTick, -1);
}

void TestMarketWatchModel::testUpdateVolume() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));
    m_model->updateVolume(0, 1500000);
    QCOMPARE(m_model->getScripAt(0).volume, (qint64)1500000);
}

void TestMarketWatchModel::testUpdateHighLow() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));
    m_model->updateHighLow(0, 2520.0, 2475.0);
    QCOMPARE(m_model->getScripAt(0).high, 2520.0);
    QCOMPARE(m_model->getScripAt(0).low, 2475.0);
}

void TestMarketWatchModel::testUpdateOHLC() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));
    m_model->updateOHLC(0, 2490.0, 2520.0, 2475.0, 2485.0);

    const ScripData& s = m_model->getScripAt(0);
    QCOMPARE(s.open, 2490.0);
    QCOMPARE(s.high, 2520.0);
    QCOMPARE(s.low, 2475.0);
    QCOMPARE(s.close, 2485.0);
}

void TestMarketWatchModel::testUpdateOpenInterest() {
    m_model->addScrip(makeFOScrip("NIFTY 20000 CE", 49508, 20000, "CE", "27FEB2026"));
    m_model->updateOpenInterest(0, 5000000);
    QCOMPARE(m_model->getScripAt(0).openInterest, (qint64)5000000);
}

void TestMarketWatchModel::testUpdateAveragePrice() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));
    m_model->updateAveragePrice(0, 2498.75);
    QCOMPARE(m_model->getScripAt(0).avgTradedPrice, 2498.75);
}

void TestMarketWatchModel::testUpdateGreeks() {
    m_model->addScrip(makeFOScrip("NIFTY 20000 CE", 49508, 20000, "CE", "27FEB2026"));

    m_model->updateGreeks(0, 0.18, 0.17, 0.19, 0.55, 0.0002, 12.5, -8.3);

    const ScripData& s = m_model->getScripAt(0);
    QCOMPARE(s.iv, 0.18);
    QCOMPARE(s.bidIV, 0.17);
    QCOMPARE(s.askIV, 0.19);
    QCOMPARE(s.delta, 0.55);
    QCOMPARE(s.gamma, 0.0002);
    QCOMPARE(s.vega, 12.5);
    QCOMPARE(s.theta, -8.3);
}

void TestMarketWatchModel::testUpdateScripData() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));

    ScripData updated = makeScrip("RELIANCE-UPDATED", 2885);
    updated.ltp = 2600.0;
    updated.volume = 999999;
    m_model->updateScripData(0, updated);

    QCOMPARE(m_model->getScripAt(0).symbol, QString("RELIANCE-UPDATED"));
    QCOMPARE(m_model->getScripAt(0).ltp, 2600.0);
    QCOMPARE(m_model->getScripAt(0).volume, (qint64)999999);
}

// ─── Bounds Checks ───────────────────────────────────────

void TestMarketWatchModel::testUpdatePrice_outOfBounds() {
    m_model->addScrip(makeScrip("A", 1));

    // Should not crash on out-of-bounds
    m_model->updatePrice(-1, 100, 0, 0);
    m_model->updatePrice(5, 100, 0, 0);
    QCOMPARE(m_model->getScripAt(0).ltp, 0.0);  // Unchanged
}

void TestMarketWatchModel::testUpdatePrice_blankRow() {
    m_model->insertBlankRow();

    // Updates should be ignored for blank rows
    m_model->updatePrice(0, 100, 5, 5);
    // Blank row should remain a blank row
    QVERIFY(m_model->isBlankRow(0));
}

// ─── Model Interface ────────────────────────────────────

void TestMarketWatchModel::testRowCount() {
    QCOMPARE(m_model->rowCount(), 0);
    m_model->addScrip(makeScrip("A", 1));
    QCOMPARE(m_model->rowCount(), 1);
    m_model->insertBlankRow();
    QCOMPARE(m_model->rowCount(), 2);
}

void TestMarketWatchModel::testColumnCount() {
    // Should match the default profile's visible column count
    QVERIFY(m_model->columnCount() > 0);
}

void TestMarketWatchModel::testDataRole_display() {
    m_model->addScrip(makeScrip("RELIANCE", 2885));
    m_model->updatePrice(0, 2500.50, 15.25, 0.61);

    // Column 0 in default profile is SYMBOL
    auto idx = m_model->index(0, 0);
    auto display = m_model->data(idx, Qt::DisplayRole);
    QVERIFY(!display.isNull());
    // The exact value depends on profile column order
}

void TestMarketWatchModel::testDataRole_userRole() {
    m_model->addScrip(makeScrip("RELIANCE", 2885, "NSE"));

    // UserRole + 1 = token
    auto idx = m_model->index(0, 0);
    auto token = m_model->data(idx, Qt::UserRole + 1);
    QCOMPARE(token.toLongLong(), (qlonglong)2885);

    // UserRole + 2 = exchange
    auto exchange = m_model->data(idx, Qt::UserRole + 2);
    QCOMPARE(exchange.toString(), QString("NSE"));
}

// ─── Signals ─────────────────────────────────────────────

void TestMarketWatchModel::testScripAddedSignal() {
    QSignalSpy spy(m_model, &MarketWatchModel::scripAdded);
    QVERIFY(spy.isValid());

    m_model->addScrip(makeScrip("RELIANCE", 2885));

    QCOMPARE(spy.count(), 1);
    auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 0);  // row
}

void TestMarketWatchModel::testScripRemovedSignal() {
    m_model->addScrip(makeScrip("A", 1));

    QSignalSpy spy(m_model, &MarketWatchModel::scripRemoved);
    QVERIFY(spy.isValid());

    m_model->removeScrip(0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toInt(), 0);
}

void TestMarketWatchModel::testPriceUpdatedSignal() {
    m_model->addScrip(makeScrip("A", 1));

    // priceUpdated is emitted inside updatePrice? Let's check
    // Actually MarketWatchModel doesn't have priceUpdated signal for per-cell
    // It uses dataChanged from QAbstractTableModel
    QSignalSpy spy(m_model, &QAbstractTableModel::dataChanged);
    QVERIFY(spy.isValid());

    m_model->updatePrice(0, 100.0, 5.0, 5.0);

    QVERIFY(spy.count() >= 1);  // At least one dataChanged emission
}

// ─── Profile ─────────────────────────────────────────────

void TestMarketWatchModel::testLoadProfile() {
    int defaultCols = m_model->columnCount();
    m_model->loadProfile("Compact");
    int compactCols = m_model->columnCount();

    // Compact should have fewer columns than default (or at least different)
    // Just verify it didn't crash and column count changed or stayed valid
    QVERIFY(compactCols > 0);

    m_model->loadProfile("Detailed");
    int detailedCols = m_model->columnCount();
    QVERIFY(detailedCols > 0);
    // Detailed should have >= compact
    QVERIFY(detailedCols >= compactCols);
}

void TestMarketWatchModel::testAvailableProfiles() {
    auto profiles = m_model->getAvailableProfiles();
    QVERIFY(profiles.contains("Default"));
    QVERIFY(profiles.contains("Compact"));
    QVERIFY(profiles.contains("Detailed"));
    QVERIFY(profiles.contains("F&O"));
    QVERIFY(profiles.contains("Equity"));
    QVERIFY(profiles.contains("Trading"));
}

// ─── ScripData Helpers ───────────────────────────────────

void TestMarketWatchModel::testScripData_createBlankRow() {
    ScripData blank = ScripData::createBlankRow();
    QVERIFY(blank.isBlankRow);
    QCOMPARE(blank.token, -1);
    QVERIFY(!blank.isValid());
}

void TestMarketWatchModel::testScripData_isValid() {
    ScripData s;
    s.token = 0;
    QVERIFY(!s.isValid());

    s.token = 100;
    QVERIFY(s.isValid());

    s.isBlankRow = true;
    QVERIFY(!s.isValid());  // blank rows are never valid
}

// ─── Scrip Count ─────────────────────────────────────────

void TestMarketWatchModel::testScripCount_excludesBlanks() {
    m_model->addScrip(makeScrip("A", 1));
    m_model->insertBlankRow();
    m_model->addScrip(makeScrip("B", 2));
    m_model->insertBlankRow();
    m_model->addScrip(makeScrip("C", 3));

    QCOMPARE(m_model->scripCount(), 3);
    QCOMPARE(m_model->totalRowCount(), 5);
}

// ─── Main ────────────────────────────────────────────────

QTEST_MAIN(TestMarketWatchModel)
#include "test_market_watch_model.moc"

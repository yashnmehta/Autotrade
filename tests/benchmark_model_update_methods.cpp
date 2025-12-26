/*
 * Benchmark: Qt Model Update Methods - Performance Comparison
 * 
 * Tests 3 approaches to updating QTableView from QAbstractTableModel:
 * 1. Standard Qt signals (emit dataChanged)
 * 2. Direct viewport update (manual repaint)
 * 3. Custom callback system (bypass signals)
 * 
 * Simulates real-time market data updates with high-frequency price changes.
 */

#include <QApplication>
#include <QTableView>
#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <QHeaderView>
#include <QTimer>
#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDebug>
#include <vector>
#include <random>
#include <chrono>
#include <numeric>
#include <algorithm>

// ============================================================================
// CONFIGURATION
// ============================================================================

constexpr int NUM_ROWS = 50;           // Number of scrips
constexpr int NUM_COLS = 10;           // Columns (symbol, LTP, bid, ask, etc.)
constexpr int UPDATES_PER_SECOND = 100; // Market data frequency
constexpr int TEST_DURATION_SEC = 10;  // How long to run each test

// ============================================================================
// INTERFACE: Custom callback for direct view updates
// ============================================================================

class IViewUpdateCallback {
public:
    virtual ~IViewUpdateCallback() = default;
    virtual void onCellUpdated(int row, int col) = 0;
    virtual void onRangeUpdated(int row, int firstCol, int lastCol) = 0;
};

// ============================================================================
// MODEL: Simulated market data model with multiple update strategies
// ============================================================================

enum class UpdateStrategy {
    QT_SIGNALS,          // Standard: emit dataChanged()
    DIRECT_VIEWPORT,     // Manual: direct viewport()->update()
    CUSTOM_CALLBACK      // Bypass: custom C++ callback
};

class MarketDataModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit MarketDataModel(QObject* parent = nullptr) 
        : QAbstractTableModel(parent)
        , m_callback(nullptr)
        , m_strategy(UpdateStrategy::QT_SIGNALS)
        , m_updateCount(0)
        , m_totalLatency(0)
    {
        // Initialize with random data
        m_data.resize(NUM_ROWS);
        std::random_device rd;
        m_rng.seed(rd());
        m_dist = std::uniform_real_distribution<double>(100.0, 500.0);
        
        for (int i = 0; i < NUM_ROWS; ++i) {
            m_data[i].resize(NUM_COLS);
            for (int j = 0; j < NUM_COLS; ++j) {
                m_data[i][j] = m_dist(m_rng);
            }
        }
    }

    // QAbstractTableModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : NUM_ROWS;
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : NUM_COLS;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || role != Qt::DisplayRole)
            return QVariant();

        return QString::number(m_data[index.row()][index.column()], 'f', 2);
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role != Qt::DisplayRole)
            return QVariant();

        if (orientation == Qt::Horizontal) {
            static const QStringList headers = {
                "Symbol", "LTP", "Bid", "Ask", "Volume", "High", "Low", "Open", "Close", "Change"
            };
            return headers.value(section, QString::number(section));
        }
        return QString::number(section);
    }

    // Update strategy control
    void setUpdateStrategy(UpdateStrategy strategy) {
        m_strategy = strategy;
        qDebug() << "[MODEL] Strategy changed to:" << static_cast<int>(strategy);
    }

    void setViewCallback(IViewUpdateCallback* callback) {
        m_callback = callback;
    }

    // Simulate market data update
    void updateRandomCell() {
        auto t_start = std::chrono::high_resolution_clock::now();

        // Pick random cell
        std::uniform_int_distribution<int> rowDist(0, NUM_ROWS - 1);
        std::uniform_int_distribution<int> colDist(0, NUM_COLS - 1);
        
        int row = rowDist(m_rng);
        int col = colDist(m_rng);

        // Update value (simulate price change)
        double change = m_dist(m_rng) * 0.01; // ±1% change
        m_data[row][col] += (m_rng() % 2 ? change : -change);

        // Notify view based on strategy
        notifyUpdate(row, col);

        auto t_end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
        
        m_updateCount++;
        m_totalLatency += latency;
    }

    // Simulate row update (multiple columns)
    void updateRandomRow() {
        auto t_start = std::chrono::high_resolution_clock::now();

        std::uniform_int_distribution<int> rowDist(0, NUM_ROWS - 1);
        int row = rowDist(m_rng);

        // Update all columns in row (simulate tick update)
        for (int col = 0; col < NUM_COLS; ++col) {
            double change = m_dist(m_rng) * 0.01;
            m_data[row][col] += (m_rng() % 2 ? change : -change);
        }

        // Notify view based on strategy
        notifyRangeUpdate(row, 0, NUM_COLS - 1);

        auto t_end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
        
        m_updateCount++;
        m_totalLatency += latency;
    }

    // Statistics
    double getAverageLatencyNs() const {
        return m_updateCount > 0 ? static_cast<double>(m_totalLatency) / m_updateCount : 0.0;
    }

    int getUpdateCount() const {
        return m_updateCount;
    }

    void resetStats() {
        m_updateCount = 0;
        m_totalLatency = 0;
    }

private:
    void notifyUpdate(int row, int col) {
        switch (m_strategy) {
            case UpdateStrategy::QT_SIGNALS:
                // Standard Qt way
                emit dataChanged(index(row, col), index(row, col));
                break;

            case UpdateStrategy::DIRECT_VIEWPORT:
                // This is handled in the view
                emit dataChanged(index(row, col), index(row, col));
                break;

            case UpdateStrategy::CUSTOM_CALLBACK:
                // Bypass Qt signals entirely
                if (m_callback) {
                    m_callback->onCellUpdated(row, col);
                }
                break;
        }
    }

    void notifyRangeUpdate(int row, int firstCol, int lastCol) {
        switch (m_strategy) {
            case UpdateStrategy::QT_SIGNALS:
                emit dataChanged(index(row, firstCol), index(row, lastCol));
                break;

            case UpdateStrategy::DIRECT_VIEWPORT:
                emit dataChanged(index(row, firstCol), index(row, lastCol));
                break;

            case UpdateStrategy::CUSTOM_CALLBACK:
                if (m_callback) {
                    m_callback->onRangeUpdated(row, firstCol, lastCol);
                }
                break;
        }
    }

    std::vector<std::vector<double>> m_data;
    IViewUpdateCallback* m_callback;
    UpdateStrategy m_strategy;
    
    // Random number generation
    std::mt19937 m_rng;
    std::uniform_real_distribution<double> m_dist;
    
    // Statistics
    int m_updateCount;
    uint64_t m_totalLatency;
};

// ============================================================================
// VIEW: Custom table view with multiple update strategies
// ============================================================================

class BenchmarkTableView : public QTableView, public IViewUpdateCallback {
    Q_OBJECT

public:
    explicit BenchmarkTableView(QWidget* parent = nullptr)
        : QTableView(parent)
        , m_paintCount(0)
        , m_totalPaintTime(0)
    {
        // Optimize for performance
        setAlternatingRowColors(true);
        horizontalHeader()->setStretchLastSection(true);
        verticalHeader()->setDefaultSectionSize(25);
        
        // Disable scrollbars for benchmark (reduces overhead)
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    void setUpdateStrategy(UpdateStrategy strategy) {
        m_strategy = strategy;
        
        if (strategy == UpdateStrategy::CUSTOM_CALLBACK) {
            // Register callback
            if (auto* model = qobject_cast<MarketDataModel*>(this->model())) {
                model->setViewCallback(this);
            }
        }
    }

    // IViewUpdateCallback implementation
    void onCellUpdated(int row, int col) override {
        // Direct viewport update - bypass Qt signals
        QModelIndex idx = model()->index(row, col);
        QRect rect = visualRect(idx);
        viewport()->update(rect);
    }

    void onRangeUpdated(int row, int firstCol, int lastCol) override {
        // Direct viewport update for range
        QModelIndex topLeft = model()->index(row, firstCol);
        QModelIndex bottomRight = model()->index(row, lastCol);
        QRect rect = visualRect(topLeft).united(visualRect(bottomRight));
        viewport()->update(rect);
    }

    // Override dataChanged to add custom behavior for DIRECT_VIEWPORT strategy
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, 
                     const QVector<int> &roles = QVector<int>()) override {
        
        if (m_strategy == UpdateStrategy::DIRECT_VIEWPORT) {
            // Manually trigger viewport update instead of letting Qt handle it
            QRect rect;
            for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
                for (int col = topLeft.column(); col <= bottomRight.column(); ++col) {
                    rect = rect.united(visualRect(model()->index(row, col)));
                }
            }
            viewport()->update(rect);
        } else {
            // Let Qt handle it normally
            QTableView::dataChanged(topLeft, bottomRight, roles);
        }
    }

    // Track paint events
    void paintEvent(QPaintEvent* event) override {
        auto t_start = std::chrono::high_resolution_clock::now();
        
        QTableView::paintEvent(event);
        
        auto t_end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count();
        
        m_paintCount++;
        m_totalPaintTime += latency;
    }

    // Statistics
    double getAveragePaintTimeUs() const {
        return m_paintCount > 0 ? static_cast<double>(m_totalPaintTime) / m_paintCount : 0.0;
    }

    int getPaintCount() const {
        return m_paintCount;
    }

    void resetStats() {
        m_paintCount = 0;
        m_totalPaintTime = 0;
    }

private:
    UpdateStrategy m_strategy = UpdateStrategy::QT_SIGNALS;
    int m_paintCount;
    uint64_t m_totalPaintTime;
};

// ============================================================================
// BENCHMARK CONTROLLER: Manages test execution and results
// ============================================================================

class BenchmarkController : public QWidget {
    Q_OBJECT

public:
    BenchmarkController(QWidget* parent = nullptr) : QWidget(parent) {
        setupUI();
        setupModel();
        
        // Update timer
        m_updateTimer = new QTimer(this);
        connect(m_updateTimer, &QTimer::timeout, this, &BenchmarkController::onUpdateTick);
        
        // Test duration timer
        m_testTimer = new QTimer(this);
        m_testTimer->setSingleShot(true);
        connect(m_testTimer, &QTimer::timeout, this, &BenchmarkController::onTestComplete);
        
        // Stats update timer
        m_statsTimer = new QTimer(this);
        connect(m_statsTimer, &QTimer::timeout, this, &BenchmarkController::updateStats);
        m_statsTimer->start(100); // Update stats display every 100ms
    }

private slots:
    void onStartTest() {
        UpdateStrategy strategy = static_cast<UpdateStrategy>(m_strategyCombo->currentIndex());
        int updateFreq = m_frequencyCombo->currentText().toInt();
        
        qDebug() << "\n========================================";
        qDebug() << "Starting test:";
        qDebug() << "  Strategy:" << m_strategyCombo->currentText();
        qDebug() << "  Frequency:" << updateFreq << "updates/sec";
        qDebug() << "  Duration:" << TEST_DURATION_SEC << "seconds";
        qDebug() << "========================================\n";
        
        // Reset stats
        m_model->resetStats();
        m_view->resetStats();
        m_testStartTime = std::chrono::high_resolution_clock::now();
        
        // Configure strategy
        m_model->setUpdateStrategy(strategy);
        m_view->setUpdateStrategy(strategy);
        
        // Start test
        m_startButton->setEnabled(false);
        m_updateTimer->start(1000 / updateFreq);
        m_testTimer->start(TEST_DURATION_SEC * 1000);
    }

    void onUpdateTick() {
        // Simulate market data update
        if (m_updateTypeCombo->currentIndex() == 0) {
            m_model->updateRandomCell();
        } else {
            m_model->updateRandomRow();
        }
    }

    void onTestComplete() {
        m_updateTimer->stop();
        
        auto t_end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            t_end - m_testStartTime).count();
        
        // Collect results
        QString strategy = m_strategyCombo->currentText();
        int updates = m_model->getUpdateCount();
        double modelLatency = m_model->getAverageLatencyNs();
        int paints = m_view->getPaintCount();
        double paintTime = m_view->getAveragePaintTimeUs();
        
        double actualFPS = (paints * 1000.0) / duration;
        double updateRate = (updates * 1000.0) / duration;
        
        // Display results
        QString result = QString(
            "========================================\n"
            "TEST RESULTS: %1\n"
            "========================================\n"
            "Duration:         %2 ms\n"
            "Total updates:    %3\n"
            "Update rate:      %4 updates/sec\n"
            "Model latency:    %5 µs (avg)\n"
            "Paint count:      %6\n"
            "Paint time:       %7 µs (avg)\n"
            "Actual FPS:       %8\n"
            "========================================\n"
        ).arg(strategy)
         .arg(duration)
         .arg(updates)
         .arg(updateRate, 0, 'f', 1)
         .arg(modelLatency / 1000.0, 0, 'f', 2)
         .arg(paints)
         .arg(paintTime, 0, 'f', 2)
         .arg(actualFPS, 0, 'f', 1);
        
        qDebug().noquote() << result;
        m_resultsLabel->setText(result);
        
        m_startButton->setEnabled(true);
    }

    void updateStats() {
        if (!m_updateTimer->isActive()) return;
        
        int updates = m_model->getUpdateCount();
        double modelLatency = m_model->getAverageLatencyNs();
        int paints = m_view->getPaintCount();
        double paintTime = m_view->getAveragePaintTimeUs();
        
        m_statsLabel->setText(QString(
            "Updates: %1 | Model: %2 µs | Paints: %3 | Paint: %4 µs"
        ).arg(updates)
         .arg(modelLatency / 1000.0, 0, 'f', 2)
         .arg(paints)
         .arg(paintTime, 0, 'f', 2));
    }

private:
    void setupUI() {
        auto* layout = new QVBoxLayout(this);
        
        // Title
        auto* titleLabel = new QLabel("<h2>Model Update Strategy Benchmark</h2>");
        layout->addWidget(titleLabel);
        
        // Controls
        auto* controlsLayout = new QHBoxLayout();
        
        controlsLayout->addWidget(new QLabel("Strategy:"));
        m_strategyCombo = new QComboBox();
        m_strategyCombo->addItem("Qt Signals (emit dataChanged)");
        m_strategyCombo->addItem("Direct Viewport (manual update)");
        m_strategyCombo->addItem("Custom Callback (bypass signals)");
        controlsLayout->addWidget(m_strategyCombo);
        
        controlsLayout->addWidget(new QLabel("Frequency:"));
        m_frequencyCombo = new QComboBox();
        m_frequencyCombo->addItems({"10", "50", "100", "200", "500", "1000"});
        m_frequencyCombo->setCurrentText("100");
        controlsLayout->addWidget(m_frequencyCombo);
        
        controlsLayout->addWidget(new QLabel("Update Type:"));
        m_updateTypeCombo = new QComboBox();
        m_updateTypeCombo->addItems({"Single Cell", "Full Row"});
        m_updateTypeCombo->setCurrentIndex(1); // Default to row updates
        controlsLayout->addWidget(m_updateTypeCombo);
        
        m_startButton = new QPushButton("Start Test");
        connect(m_startButton, &QPushButton::clicked, this, &BenchmarkController::onStartTest);
        controlsLayout->addWidget(m_startButton);
        
        layout->addLayout(controlsLayout);
        
        // Stats display
        m_statsLabel = new QLabel("Ready to test...");
        m_statsLabel->setStyleSheet("QLabel { background: #f0f0f0; padding: 5px; }");
        layout->addWidget(m_statsLabel);
        
        // Table view
        m_view = new BenchmarkTableView();
        layout->addWidget(m_view);
        
        // Results
        m_resultsLabel = new QLabel();
        m_resultsLabel->setStyleSheet("QLabel { background: #e8f4f8; padding: 10px; font-family: monospace; }");
        layout->addWidget(m_resultsLabel);
        
        setMinimumSize(1200, 800);
    }

    void setupModel() {
        m_model = new MarketDataModel(this);
        m_view->setModel(m_model);
    }

    MarketDataModel* m_model;
    BenchmarkTableView* m_view;
    
    QComboBox* m_strategyCombo;
    QComboBox* m_frequencyCombo;
    QComboBox* m_updateTypeCombo;
    QPushButton* m_startButton;
    QLabel* m_statsLabel;
    QLabel* m_resultsLabel;
    
    QTimer* m_updateTimer;
    QTimer* m_testTimer;
    QTimer* m_statsTimer;
    
    std::chrono::high_resolution_clock::time_point m_testStartTime;
};

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    qDebug() << "========================================";
    qDebug() << "Model Update Strategy Benchmark";
    qDebug() << "========================================";
    qDebug() << "Configuration:";
    qDebug() << "  Rows:" << NUM_ROWS;
    qDebug() << "  Columns:" << NUM_COLS;
    qDebug() << "  Test duration:" << TEST_DURATION_SEC << "seconds";
    qDebug() << "========================================\n";
    
    BenchmarkController controller;
    controller.show();
    
    return app.exec();
}

#include "benchmark_model_update_methods.moc"

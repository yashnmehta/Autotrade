#ifndef CHARTSWINDOW_H
#define CHARTSWINDOW_H

/**
 * @file ChartsWindow.h
 * @brief Multi-chart TradingView window with grid layouts.
 *
 * Features:
 *   - 1–4 simultaneous chart panels
 *   - 8 layout modes (Single, TwoHoriz/Vert, Three*, FourGrid)
 *   - ScripBar for native symbol search
 *   - Timeline synchronisation across panels
 *   - Per-chart header with symbol name, LTP, and close button
 *   - Deferred QWebEngine initialisation for fast window opening
 */

#include <QWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QWebEngineView>
#include <QWebChannel>
#include <QVector>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QKeyEvent>
#include "app/ScripBar.h"
#include "models/domain/WindowContext.h"
#include "views/ChartDataFeed.h"

class XTSMarketDataClient;

enum class TVChartLayout {
    Single,
    TwoHoriz,
    TwoVert,
    ThreeLeft,
    ThreeRight,
    ThreeTop,
    ThreeBottom,
    FourGrid
};

struct ChartInstance {
    QWebEngineView *webView        = nullptr;
    QWebChannel    *webChannel     = nullptr;
    ChartDataFeed  *dataFeed       = nullptr;
    QWidget        *container      = nullptr;
    QWidget        *placeholder    = nullptr;
    QLabel         *titleLabel     = nullptr;
    QLabel         *ltpLabel       = nullptr;
    QPushButton    *closeBtn       = nullptr;
    WindowContext   context;
    bool            webEngineInitialized = false;
};

class ChartsWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ChartsWindow(QWidget *parent = nullptr);
    explicit ChartsWindow(const WindowContext &context, QWidget *parent = nullptr);
    ~ChartsWindow();

    void loadFromContext(const WindowContext &context);
    WindowContext getContext() const { return m_lastContext; }

    void setMarketDataClient(XTSMarketDataClient *client);

    void setChartLayout(TVChartLayout layout);
    TVChartLayout chartLayout() const { return m_layout; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void orderFromChartRequested(const QJsonObject &orderData);

private slots:
    void onScripSelected(const InstrumentData &data);
    void removeChart(ChartInstance *chart);
    void initLayoutToolbar();

private:
    void initUI();
    void buildChartContainer(ChartInstance *chart);
    void addChart(const WindowContext &context);
    void initWebEngineForChart(ChartInstance *chart);
    void rearrangeGridLayout();

    ScripBar   *m_scripBar          = nullptr;
    QWidget    *m_layoutToolbar     = nullptr;
    QComboBox  *m_layoutCombo       = nullptr;
    QCheckBox  *m_syncTimelineCheck = nullptr;

    QGridLayout *m_gridLayout            = nullptr;
    QWidget     *m_chartContainerWidget  = nullptr;

    QVector<ChartInstance *> m_charts;
    WindowContext            m_lastContext;
    TVChartLayout              m_layout = TVChartLayout::Single;

    XTSMarketDataClient *m_marketDataClient = nullptr;
};

#endif // CHARTSWINDOW_H

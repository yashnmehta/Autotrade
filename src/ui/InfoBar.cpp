#include "ui/InfoBar.h"
#include "ui_InfoBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMenu>
#include <QContextMenuEvent>
#include <QPainter>
#include <QFrame>
#include <QMap>
#include <QVBoxLayout>

static QWidget *createMiniPanel(const QString &title, const QString &value, QWidget *parent = nullptr)
{
    QFrame *frame = new QFrame(parent);
    frame->setObjectName("statPanel");
    frame->setStyleSheet(
        "QFrame#statPanel{ background-color:#252526; border: 1px solid #3e3e42; padding: 3px 6px; border-radius: 2px; }");
    QVBoxLayout *l = new QVBoxLayout(frame);
    // l->setContentsMargins(3, 2, 3, 2);
    l->setSpacing(1);
    QLabel *t = new QLabel(title, frame);
    t->setStyleSheet("color:#888888; font-size: 9px;");
    t->setAlignment(Qt::AlignCenter);
    QLabel *v = new QLabel(value, frame);
    v->setStyleSheet("color:#ffffff; font-weight:bold; font-size:10px;");
    v->setAlignment(Qt::AlignCenter);
    l->addWidget(t);
    l->addWidget(v);
    return frame;
}

InfoBar::InfoBar(QWidget *parent)
    : QWidget(parent), ui(new Ui::InfoBar)
{
    ui->setupUi(this);
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
    setMaximumHeight(50);
    setMinimumHeight(50);

    setStyleSheet(
        "QWidget{ background-color: #1e1e1e; }"
        "QLabel { color: #cccccc; font-size: 10px; padding: 0px 2px; }"
        "QFrame#panel{ background-color: #2d2d30; border: 1px solid #3e3e42; }");

    // Wire up referenced labels to our members for backward compatibility
    m_openOrdersLabel = ui->openOrdersLabel;
    m_totalOrdersLabel = ui->totalOrdersLabel;
    m_totalTradesLabel = ui->totalTradesLabel;

    // Setup default conn indicator
    QPixmap px(12, 12);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QBrush(QColor("#888888")));
    p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, 12, 12);
    ui->connLabel->setPixmap(px);
    ui->connLabel->setToolTip("Disconnected");
}

void InfoBar::setVersionText(const QString &ver)
{
    if (ui && ui->versionLabel)
        ui->versionLabel->setText(ver);
}

void InfoBar::setInfoText(const QString &text)
{
    if (ui && ui->infoLabel)
        ui->infoLabel->setText(text);
}

void InfoBar::setLastUpdateText(const QString &text)
{
    if (ui && ui->lastUpdateLabel)
        ui->lastUpdateLabel->setText(text);
}

void InfoBar::setConnected(bool connected, int latencyMs)
{
    if (!ui || !ui->connLabel)
        return;
    QPixmap px(12, 12);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    if (connected)
    {
        p.setBrush(QBrush(QColor("#57b846"))); // green
        ui->connLabel->setToolTip(QString("Connected (%1 ms)").arg(latencyMs));
    }
    else
    {
        p.setBrush(QBrush(QColor("#888888"))); // gray
        ui->connLabel->setToolTip("Disconnected");
    }
    p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, 12, 12);
    p.end();
    ui->connLabel->setPixmap(px);
}

void InfoBar::setUserId(const QString &user)
{
    if (ui && ui->userLabel)
        ui->userLabel->setText(user);
}

void InfoBar::setSegmentStats(const QMap<QString, QString> &stats)
{
    if (!ui || !ui->segmentLayout)
        return;

    // Remove existing
    QLayoutItem *child;
    while ((child = ui->segmentLayout->takeAt(0)) != nullptr)
    {
        if (child->widget())
            delete child->widget();
        delete child;
    }
    // Add new panels in order
    for (auto it = stats.constBegin(); it != stats.constEnd(); ++it)
    {
        QWidget *panel = createMiniPanel(it.key(), it.value(), this);
        ui->segmentLayout->addWidget(panel);
    }
}

void InfoBar::setTotalCounts(int openOrders, int totalOrders, int totalTrades)
{
    if (m_openOrdersLabel)
        m_openOrdersLabel->setText(QString("Open: %1").arg(openOrders));
    if (m_totalOrdersLabel)
        m_totalOrdersLabel->setText(QString("Orders: %1").arg(totalOrders));
    if (m_totalTradesLabel)
        m_totalTradesLabel->setText(QString("Trades: %1").arg(totalTrades));
}

void InfoBar::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *hide = menu.addAction("Hide Info Bar");
    QAction *details = menu.addAction("Details...");
    QAction *res = menu.exec(event->globalPos());
    if (res == hide)
    {
        emit hideRequested();
    }
    else if (res == details)
    {
        emit detailsRequested();
    }
}

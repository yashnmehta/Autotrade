#ifndef INFOBAR_H
#define INFOBAR_H

#include <QWidget>

// Forward declarations to keep this header lightweight and avoid pulling
// heavy Qt template code into every translation unit that includes it.
class QString;
template <typename K, typename V>
class QMap;
namespace Ui
{
    class InfoBar;
}
class QHBoxLayout;
class QVBoxLayout;
class QContextMenuEvent;
class QFrame;

class QLabel;
class QToolButton;

class InfoBar : public QWidget
{
    Q_OBJECT
public:
    explicit InfoBar(QWidget *parent = nullptr);

    void setVersionText(const QString &ver);
    void setInfoText(const QString &text);
    void setLastUpdateText(const QString &text);
    void setConnected(bool connected, int latencyMs = 0);
    void setUserId(const QString &user);
    void setSegmentStats(const QMap<QString, QString> &stats); // key = label, value = value to show
    void setTotalCounts(int openOrders, int totalOrders, int totalTrades);

signals:
    void hideRequested();
    void detailsRequested();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    Ui::InfoBar *ui;
    QLabel *m_openOrdersLabel;
    QLabel *m_totalOrdersLabel;
    QLabel *m_totalTradesLabel;
};

#endif // INFOBAR_H

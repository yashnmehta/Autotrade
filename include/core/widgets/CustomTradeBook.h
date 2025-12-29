#ifndef CUSTOMTRADEBOOK_H
#define CUSTOMTRADEBOOK_H

#include <QTableView>
#include <QMenu>

class CustomTradeBook : public QTableView
{
    Q_OBJECT

public:
    explicit CustomTradeBook(QWidget *parent = nullptr);
    ~CustomTradeBook();

public slots:
    void setSummaryRowEnabled(bool enabled);
    bool isSummaryRowEnabled() const { return m_summaryRowEnabled; }

signals:
    void tradeDetailsRequested();
    void exportRequested();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void applyDefaultStyling();
    void setupHeader();
    QMenu* createContextMenu();

    QMenu *m_contextMenu;
    bool m_summaryRowEnabled;
};

#endif // CUSTOMTRADEBOOK_H

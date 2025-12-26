#ifndef CUSTOMORDERBOOK_H
#define CUSTOMORDERBOOK_H

#include <QTableView>
#include <QMenu>

class CustomOrderBook : public QTableView
{
    Q_OBJECT

public:
    explicit CustomOrderBook(QWidget *parent = nullptr);
    virtual ~CustomOrderBook();

    void applyDefaultStyling();
    void setupHeader();
    void setSummaryRowEnabled(bool enabled);
    bool isSummaryRowEnabled() const { return m_summaryRowEnabled; }

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    virtual QMenu* createContextMenu();

signals:
    void orderDetailsRequested();
    void cancelOrderRequested();
    void modifyOrderRequested();
    void exportRequested();

private:
    QMenu *m_contextMenu;
    bool m_summaryRowEnabled;
};

#endif // CUSTOMORDERBOOK_H

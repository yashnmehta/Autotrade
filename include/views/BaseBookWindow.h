#ifndef BASEBOOKWINDOW_H
#define BASEBOOKWINDOW_H

#include <QWidget>
#include <QTableView>
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QShortcut>
#include "models/GenericTableProfile.h"

class BaseBookWindow : public QWidget {
    Q_OBJECT
public:
    explicit BaseBookWindow(const QString& windowName, QWidget* parent = nullptr);
    virtual ~BaseBookWindow();

protected:
    virtual void setupUI() = 0;
    virtual void loadInitialProfile();
    virtual void saveCurrentProfile();
    
    void toggleFilterRow(QAbstractItemModel* model, QTableView* tableView);
    void exportToCSV(QAbstractItemModel* model, QTableView* tableView);

    QString m_windowName;
    QTableView* m_tableView;
    QAbstractItemModel* m_model;
    QSortFilterProxyModel* m_proxyModel;
    GenericTableProfile m_columnProfile;
    bool m_filterRowVisible;
    QList<QWidget*> m_filterWidgets;
    QShortcut* m_filterShortcut;

protected slots:
    virtual void showColumnProfileDialog();
    virtual void onColumnFilterChanged(int col, const QStringList& filter);

protected:
    void closeEvent(QCloseEvent* event) override;
};

#endif // BASEBOOKWINDOW_H

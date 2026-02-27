#ifndef BASEBOOKWINDOW_H
#define BASEBOOKWINDOW_H

#include <QWidget>
#include <QTableView>
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QShortcut>
#include <QSettings>
#include <QMap>
#include "models/profiles/GenericTableProfile.h"

class BaseBookWindow : public QWidget {
    Q_OBJECT
public:
    explicit BaseBookWindow(const QString& windowName, QWidget* parent = nullptr);
    virtual ~BaseBookWindow();

    virtual void saveState(QSettings &settings);
    virtual void restoreState(QSettings &settings);

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
    QMap<int, QString> m_textFilters;  ///< Column -> filter text for inline filtering

protected slots:
    virtual void showColumnProfileDialog();
    virtual void onColumnFilterChanged(int col, const QStringList& filter);
    virtual void onTextFilterChanged(int col, const QString& text);  ///< For inline text filters

protected:
    void closeEvent(QCloseEvent* event) override;
};

#endif // BASEBOOKWINDOW_H


#include "views/BaseBookWindow.h"
#include "utils/TableProfileHelper.h"
#include "utils/WindowSettingsHelper.h"
#include "utils/BookWindowHelper.h"
#include "views/helpers/GenericTableFilter.h"
#include <QCloseEvent>

BaseBookWindow::BaseBookWindow(const QString& windowName, QWidget* parent)
    : QWidget(parent), m_windowName(windowName), m_tableView(nullptr), m_model(nullptr), 
      m_proxyModel(nullptr), m_filterRowVisible(false) 
{
    m_filterShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
}

BaseBookWindow::~BaseBookWindow() {}

void BaseBookWindow::loadInitialProfile() {
    if (m_tableView && m_model) {
        TableProfileHelper::loadProfile(m_windowName, m_tableView, m_model, m_columnProfile);
    }
}

void BaseBookWindow::saveCurrentProfile() {
    if (m_tableView && m_model) {
        TableProfileHelper::saveCurrentProfile(m_windowName, m_tableView, m_model, m_columnProfile);
    }
}

void BaseBookWindow::showColumnProfileDialog() {
    if (m_tableView && m_model) {
        TableProfileHelper::showProfileDialog(m_windowName, m_tableView, m_model, m_columnProfile, this);
    }
}

void BaseBookWindow::toggleFilterRow(QAbstractItemModel* model, QTableView* tableView) {
    m_filterRowVisible = !m_filterRowVisible;
    // Assuming model has setFilterRowVisible
    QMetaObject::invokeMethod(model, "setFilterRowVisible", Q_ARG(bool, m_filterRowVisible));

    if (m_filterRowVisible) {
        m_filterWidgets.clear();
        for (int i = 0; i < model->columnCount(); ++i) {
            GenericTableFilter* fw = new GenericTableFilter(i, model, tableView, tableView);
            fw->setFilterRowOffset(1);
            m_filterWidgets.append(fw);
            tableView->setIndexWidget(model->index(0, i), fw);
            connect(fw, &GenericTableFilter::filterChanged, this, &BaseBookWindow::onColumnFilterChanged);
        }
    } else {
        for (int i = 0; i < model->columnCount(); ++i) {
            tableView->setIndexWidget(model->index(0, i), nullptr);
        }
        // Notify subclasses to refresh data without column filters
        onColumnFilterChanged(-1, QStringList()); 
    }
}

void BaseBookWindow::onColumnFilterChanged(int col, const QStringList& filter) {
    // To be implemented by subclasses if they need custom filtering logic
}

void BaseBookWindow::exportToCSV(QAbstractItemModel* model, QTableView* tableView) {
    BookWindowHelper::exportToCSV(tableView, model, m_filterRowVisible, this);
}

void BaseBookWindow::closeEvent(QCloseEvent* event) {
    saveCurrentProfile();
    WindowSettingsHelper::saveWindowSettings(this, m_windowName);
    QWidget::closeEvent(event);
}

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
        // Fix memory leak: delete existing widgets before clearing
        qDeleteAll(m_filterWidgets);
        m_filterWidgets.clear();
        
        QAbstractItemModel* viewModel = tableView->model();
        QSortFilterProxyModel* proxy = qobject_cast<QSortFilterProxyModel*>(viewModel);

        for (int i = 0; i < model->columnCount(); ++i) {
            GenericTableFilter* fw = new GenericTableFilter(i, model, tableView, GenericTableFilter::CombinedMode, tableView);
            fw->setFilterRowOffset(1);
            m_filterWidgets.append(fw);
            
            QModelIndex sourceIdx = model->index(0, i);
            QModelIndex viewIdx = sourceIdx;
            
            // Map index if view uses a proxy wrapping our model
            if (proxy && proxy->sourceModel() == model) {
                viewIdx = proxy->mapFromSource(sourceIdx);
            }
            
            if (viewIdx.isValid()) {
                tableView->setIndexWidget(viewIdx, fw);
            }
            
            // Connect text filter signal for inline mode
            connect(fw, &GenericTableFilter::textFilterChanged, this, &BaseBookWindow::onTextFilterChanged);
            connect(fw, &GenericTableFilter::filterChanged, this, &BaseBookWindow::onColumnFilterChanged);
            connect(fw, &GenericTableFilter::sortRequested, [tableView](int col, Qt::SortOrder order){
                tableView->sortByColumn(col, order);
            });
        }
    } else {
        QAbstractItemModel* viewModel = tableView->model();
        QSortFilterProxyModel* proxy = qobject_cast<QSortFilterProxyModel*>(viewModel);

        for (int i = 0; i < model->columnCount(); ++i) {
             QModelIndex sourceIdx = model->index(0, i);
             QModelIndex viewIdx = sourceIdx;
             
             if (proxy && proxy->sourceModel() == model) {
                 viewIdx = proxy->mapFromSource(sourceIdx);
             }
             
             if (viewIdx.isValid()) {
                 tableView->setIndexWidget(viewIdx, nullptr);
             }
        }
        
        // Fix memory leak: delete widgets after removing from view
        qDeleteAll(m_filterWidgets);
        m_filterWidgets.clear();
        
        // Notify subclasses to refresh data without column filters
        onColumnFilterChanged(-1, QStringList()); 
    }
}

void BaseBookWindow::onColumnFilterChanged(int col, const QStringList& filter) {
    // To be implemented by subclasses if they need custom filtering logic
}

void BaseBookWindow::onTextFilterChanged(int col, const QString& text) {
    qDebug() << "[BaseBookWindow] onTextFilterChanged called: col=" << col << "text=" << text;
    
    // Store or clear the text filter
    if (text.isEmpty()) {
        m_textFilters.remove(col);
    } else {
        m_textFilters[col] = text;
    }
    
    qDebug() << "[BaseBookWindow] m_textFilters now has" << m_textFilters.size() << "entries:" << m_textFilters;
    
    // Notify subclasses to refresh data with text filter applied
    // Subclasses should override this and implement filtering in their applyFilterToModel()
}
void BaseBookWindow::exportToCSV(QAbstractItemModel* model, QTableView* tableView) {
    BookWindowHelper::exportToCSV(tableView, model, m_filterRowVisible, this);
}

void BaseBookWindow::closeEvent(QCloseEvent* event) {
    saveCurrentProfile();
    WindowSettingsHelper::saveWindowSettings(this, m_windowName);
    QWidget::closeEvent(event);
}

void BaseBookWindow::saveState(QSettings &settings)
{
    if (m_tableView && m_model) {
        // Capture current visual state into m_columnProfile
        TableProfileHelper::captureProfile(m_tableView, m_model, m_columnProfile);
        
        // Save to QSettings
        settings.setValue("columnProfile", QJsonDocument(m_columnProfile.toJson()).toJson(QJsonDocument::Compact));
    }
}

void BaseBookWindow::restoreState(QSettings &settings)
{
    if (settings.contains("columnProfile")) {
        QByteArray data = settings.value("columnProfile").toByteArray();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            m_columnProfile.fromJson(doc.object());
            
            if (m_tableView && m_model) {
                TableProfileHelper::applyProfile(m_tableView, m_model, m_columnProfile);
            }
        }
    }
}

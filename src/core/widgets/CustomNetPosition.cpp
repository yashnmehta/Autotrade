#include "core/widgets/CustomNetPosition.h"
#include <QContextMenuEvent>
#include <QAction>
#include <QDebug>

CustomNetPosition::CustomNetPosition(QWidget *parent)
    : QTableView(parent)
    , m_contextMenu(nullptr)
    , m_summaryRowEnabled(false)
{
    qDebug() << "[CustomNetPosition] Constructor called";
    
    // Apply default styling
    applyDefaultStyling();
    setupHeader();
    
    qDebug() << "[CustomNetPosition] Constructor complete";
}

CustomNetPosition::~CustomNetPosition()
{
    qDebug() << "[CustomNetPosition] Destructor called";
}

void CustomNetPosition::applyDefaultStyling()
{
    // Table appearance - optimized for financial data
    setAlternatingRowColors(false);
    setShowGrid(false);
    setStyleSheet("QTableView { "
                  "background-color: white; "
                  "gridline-color: #E0E0E0; "
                  "selection-background-color: #E3F2FD; "
                  "selection-color: black; "
                  "}"
                  "QTableView::item { "
                  "padding: 4px 8px; "
                  "}");
    
    // Selection behavior
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    
    // Performance optimization
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    
    // Row height
    verticalHeader()->setDefaultSectionSize(28);
    verticalHeader()->hide();
}

void CustomNetPosition::setupHeader()
{
    // Header styling
    horizontalHeader()->setStyleSheet(
        "QHeaderView::section { "
        "background-color: #F5F5F5; "
        "border: none; "
        "border-bottom: 1px solid #E0E0E0; "
        "border-right: 1px solid #E0E0E0; "
        "padding: 4px 8px; "
        "font-weight: bold; "
        "}"
    );
    
    // Header behavior
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionsClickable(true);
    horizontalHeader()->setHighlightSections(false);
}

void CustomNetPosition::setSummaryRowEnabled(bool enabled)
{
    if (m_summaryRowEnabled != enabled) {
        m_summaryRowEnabled = enabled;
        qDebug() << "[CustomNetPosition] Summary row" << (enabled ? "enabled" : "disabled");
        viewport()->update();
    }
}

void CustomNetPosition::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createContextMenu();
    if (menu) {
        menu->exec(event->globalPos());
        menu->deleteLater();
    }
}

QMenu* CustomNetPosition::createContextMenu()
{
    QMenu *menu = new QMenu(this);
    
    // Add standard actions
    QAction *closeAction = menu->addAction("Close Position");
    closeAction->setEnabled(selectionModel() && selectionModel()->hasSelection());
    connect(closeAction, &QAction::triggered, this, &CustomNetPosition::closePositionRequested);
    
    menu->addSeparator();
    
    QAction *exportAction = menu->addAction("Export to CSV");
    connect(exportAction, &QAction::triggered, this, &CustomNetPosition::exportRequested);
    
    menu->addSeparator();
    
    QAction *refreshAction = menu->addAction("Refresh");
    connect(refreshAction, &QAction::triggered, [this]() {
        viewport()->update();
    });
    
    return menu;
}

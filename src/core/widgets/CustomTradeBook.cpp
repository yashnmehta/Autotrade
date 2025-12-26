#include "core/widgets/CustomTradeBook.h"
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QAction>
#include <QDebug>

CustomTradeBook::CustomTradeBook(QWidget *parent)
    : QTableView(parent)
    , m_contextMenu(nullptr)
    , m_summaryRowEnabled(false)
{
    qDebug() << "[CustomTradeBook] Constructor called";
    
    // Apply default styling
    applyDefaultStyling();
    setupHeader();
    
    qDebug() << "[CustomTradeBook] Constructor complete";
}

CustomTradeBook::~CustomTradeBook()
{
    qDebug() << "[CustomTradeBook] Destructor called";
}

void CustomTradeBook::applyDefaultStyling()
{
    // Table appearance - optimized for trade data
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

void CustomTradeBook::setupHeader()
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

void CustomTradeBook::setSummaryRowEnabled(bool enabled)
{
    if (m_summaryRowEnabled != enabled) {
        m_summaryRowEnabled = enabled;
        qDebug() << "[CustomTradeBook] Summary row" << (enabled ? "enabled" : "disabled");
        viewport()->update();
    }
}

void CustomTradeBook::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createContextMenu();
    if (menu) {
        menu->exec(event->globalPos());
        menu->deleteLater();
    }
}

QMenu* CustomTradeBook::createContextMenu()
{
    QMenu *menu = new QMenu(this);
    
    // Add standard actions
    QAction *detailsAction = menu->addAction("View Trade Details");
    detailsAction->setEnabled(selectionModel() && selectionModel()->hasSelection());
    connect(detailsAction, &QAction::triggered, this, &CustomTradeBook::tradeDetailsRequested);
    
    menu->addSeparator();
    
    QAction *exportAction = menu->addAction("Export to CSV");
    connect(exportAction, &QAction::triggered, this, &CustomTradeBook::exportRequested);
    
    menu->addSeparator();
    
    QAction *refreshAction = menu->addAction("Refresh");
    connect(refreshAction, &QAction::triggered, [this]() {
        viewport()->update();
    });
    
    return menu;
}

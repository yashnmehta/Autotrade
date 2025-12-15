#include "core/widgets/CustomMarketWatch.h"
#include <QContextMenuEvent>
#include <QAction>
#include <QDebug>

CustomMarketWatch::CustomMarketWatch(QWidget *parent)
    : QTableView(parent)
    , m_contextMenu(nullptr)
{
    qDebug() << "[CustomMarketWatch] Constructor called";
    
    // Apply default styling
    applyDefaultStyling();
    setupHeader();
    
    qDebug() << "[CustomMarketWatch] Constructor complete";
}

CustomMarketWatch::~CustomMarketWatch()
{
    qDebug() << "[CustomMarketWatch] Destructor called";
}

void CustomMarketWatch::applyDefaultStyling()
{
    // Table appearance
    setAlternatingRowColors(false);
    setShowGrid(false);
    setStyleSheet("QTableView { "
                  "background-color: white; "
                  "gridline-color: #E0E0E0; "
                  "selection-background-color: #E3F2FD; "
                  "selection-color: black; "
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

void CustomMarketWatch::setupHeader()
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

void CustomMarketWatch::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createContextMenu();
    if (menu) {
        menu->exec(event->globalPos());
        menu->deleteLater();
    }
}

QMenu* CustomMarketWatch::createContextMenu()
{
    QMenu *menu = new QMenu(this);
    
    // Add standard actions
    QAction *addAction = menu->addAction("Add Scrip");
    connect(addAction, &QAction::triggered, this, &CustomMarketWatch::addScripRequested);
    
    QAction *removeAction = menu->addAction("Remove Scrip");
    removeAction->setEnabled(selectionModel() && selectionModel()->hasSelection());
    connect(removeAction, &QAction::triggered, this, &CustomMarketWatch::removeScripRequested);
    
    menu->addSeparator();
    
    QAction *refreshAction = menu->addAction("Refresh");
    connect(refreshAction, &QAction::triggered, [this]() {
        viewport()->update();
    });
    
    return menu;
}

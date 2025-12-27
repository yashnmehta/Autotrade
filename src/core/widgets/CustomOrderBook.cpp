#include "core/widgets/CustomOrderBook.h"
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QAction>
#include <QDebug>

CustomOrderBook::CustomOrderBook(QWidget *parent)
    : QTableView(parent)
    , m_contextMenu(nullptr)
    , m_summaryRowEnabled(false)
{
    qDebug() << "[CustomOrderBook] Constructor called";
    
    // Apply default styling
    applyDefaultStyling();
    setupHeader();
    
    qDebug() << "[CustomOrderBook] Constructor complete";
}

CustomOrderBook::~CustomOrderBook()
{
    qDebug() << "[CustomOrderBook] Destructor called";
}

void CustomOrderBook::applyDefaultStyling()
{
    // Table styling - Premium Dark Theme
    setStyleSheet(
        "QTableView {"
        "   background-color: #1e1e1e;"
        "   alternate-background-color: #252526;"
        "   selection-background-color: #094771;"
        "   selection-color: #ffffff;"
        "   gridline-color: #333333;"
        "   border: none;"
        "   color: #cccccc;"
        "   font-size: 11px;"
        "}"
        "QTableView::item {"
        "   padding: 4px;"
        "   border: none;"
        "}"
        "QHeaderView::section {"
        "   background-color: #252526;"
        "   color: #e0e0e0;"
        "   padding: 6px;"
        "   border: none;"
        "   border-right: 1px solid #333333;"
        "   border-bottom: 1px solid #333333;"
        "   font-weight: bold;"
        "   font-size: 11px;"
        "}"
    );
    
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setShowGrid(true);
    setWordWrap(false);
}

void CustomOrderBook::setupHeader()
{
    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setSectionsClickable(true);
    
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(28);
}

void CustomOrderBook::setSummaryRowEnabled(bool enabled)
{
    m_summaryRowEnabled = enabled;
}

void CustomOrderBook::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createContextMenu();
    if (menu) {
        menu->exec(event->globalPos());
        delete menu;
    }
}

QMenu* CustomOrderBook::createContextMenu()
{
    QMenu *menu = new QMenu(this);
    
    QAction *viewDetailsAction = menu->addAction("View Order Details");
    connect(viewDetailsAction, &QAction::triggered, this, &CustomOrderBook::orderDetailsRequested);
    
    QAction *modifyAction = menu->addAction("Modify Order");
    connect(modifyAction, &QAction::triggered, this, &CustomOrderBook::modifyOrderRequested);
    
    QAction *cancelAction = menu->addAction("Cancel Order");
    connect(cancelAction, &QAction::triggered, this, &CustomOrderBook::cancelOrderRequested);
    
    menu->addSeparator();
    
    QAction *exportAction = menu->addAction("Export to CSV");
    connect(exportAction, &QAction::triggered, this, &CustomOrderBook::exportRequested);
    
    QAction *refreshAction = menu->addAction("Refresh");
    connect(refreshAction, &QAction::triggered, [this]() {
        qDebug() << "[CustomOrderBook] Refresh requested";
    });
    
    return menu;
}

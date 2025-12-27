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
        "   background-color: #ffffff;"
        "   alternate-background-color: #f9f9f9;"
        "   selection-background-color: #e3f2fd;"
        "   selection-color: #000000;"
        "   gridline-color: #e0e0e0;"
        "   border: none;"
        "   color: #333333;"
        "   font-size: 11px;"
        "}"
        "QTableView::item {"
        "   padding: 4px;"
        "   border: none;"
        "}"
        "QHeaderView::section {"
        "   background-color: #f5f5f5;"
        "   color: #333333;"
        "   padding: 6px;"
        "   border: none;"
        "   border-right: 1px solid #e0e0e0;"
        "   border-bottom: 1px solid #e0e0e0;"
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

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
    // Table styling
    setStyleSheet(
        "QTableView {"
        "   background-color: white;"
        "   alternate-background-color: #F5F5F5;"
        "   selection-background-color: #E3F2FD;"
        "   selection-color: black;"
        "   gridline-color: #E0E0E0;"
        "   border: none;"
        "}"
        "QTableView::item {"
        "   padding: 4px;"
        "   border: none;"
        "}"
        "QTableView::item:selected {"
        "   background-color: #E3F2FD;"
        "}"
        "QHeaderView::section {"
        "   background-color: #F5F5F5;"
        "   padding: 6px;"
        "   border: none;"
        "   border-right: 1px solid #E0E0E0;"
        "   border-bottom: 1px solid #E0E0E0;"
        "   font-weight: bold;"
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

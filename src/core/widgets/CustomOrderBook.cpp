#include "core/widgets/CustomOrderBook.h"
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QAction>
#include <QDebug>

#include <QStyleFactory>

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
    // Table styling - Adjusted to allow model colors to show
    setStyleSheet("QTableView { "
                  "gridline-color: #f1f5f9; "
                  "selection-background-color: #dbeafe; "
                  "selection-color: #1e40af; "
                  "color: #1e293b; "
                  "border: none; "
                  "font-size: 11px; "
                  "}"
                  "QTableView::item { "
                  "padding: 4px 8px; "
                  "}");
    
    setAlternatingRowColors(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setShowGrid(true);
    setWordWrap(false);
}

void CustomOrderBook::setupHeader()
{
    horizontalHeader()->setStyleSheet(
        "QHeaderView::section { "
        "background-color: #f8fafc; "
        "color: #475569; "
        "border: none; "
        "border-bottom: 1px solid #e2e8f0; "
        "border-right: 1px solid #e2e8f0; "
        "padding: 6px 8px; "
        "font-weight: bold; "
        "font-size: 11px; "
        "}"
    );
    // Header behavior
    horizontalHeader()->setSectionsMovable(true);
    horizontalHeader()->setStretchLastSection(true);
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
    // If the parent has set CustomContextMenu policy, defer to its handler
    if (contextMenuPolicy() == Qt::CustomContextMenu) {
        QTableView::contextMenuEvent(event);
        return;
    }
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

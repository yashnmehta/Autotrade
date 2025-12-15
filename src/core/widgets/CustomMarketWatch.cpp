#include "core/widgets/CustomMarketWatch.h"
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QAction>
#include <QDebug>
#include <QAbstractItemModel>
#include <QApplication>
#include <QCursor>
#include <QTimer>

CustomMarketWatch::CustomMarketWatch(QWidget *parent)
    : QTableView(parent)
    , m_proxyModel(nullptr)
    , m_isDragging(false)
    , m_selectionAnchor(-1)
{
    qDebug() << "[CustomMarketWatch] Constructor called";
    
    // Create proxy model for sorting
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSortRole(Qt::UserRole);
    
    // Set proxy model as the view's model
    QTableView::setModel(m_proxyModel);
    
    // Apply default styling
    applyDefaultStyling();
    setupHeader();
    
    // Install event filter for drag-and-drop
    viewport()->installEventFilter(this);
    installEventFilter(this);
    
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragEnabled(false);  // Custom drag handling
    
    // Enable column drag & drop in header
    horizontalHeader()->setSectionsMovable(true);
    horizontalHeader()->setDragEnabled(true);
    horizontalHeader()->setDragDropMode(QAbstractItemView::InternalMove);
    
    qDebug() << "[CustomMarketWatch] Constructor complete";
}

CustomMarketWatch::~CustomMarketWatch()
{
    qDebug() << "[CustomMarketWatch] Destructor called";
}

void CustomMarketWatch::setSourceModel(QAbstractItemModel *model)
{
    m_proxyModel->setSourceModel(model);
    
    // Enable sorting
    setSortingEnabled(true);
    m_proxyModel->setDynamicSortFilter(false);  // Manual sorting only
}

void CustomMarketWatch::applyDefaultStyling()
{
    // Dark theme styling
    setAlternatingRowColors(false);
    setShowGrid(true);
    setStyleSheet(
        "QTableView {"
        "    background-color: #1e1e1e;"
        "    color: #ffffff;"
        "    gridline-color: #3f3f46;"
        "    selection-background-color: #094771;"
        "    selection-color: #ffffff;"
        "}"
        "QHeaderView::section {"
        "    background-color: #2d2d30;"
        "    color: #ffffff;"
        "    padding: 4px;"
        "    border: 1px solid #3f3f46;"
        "    font-weight: bold;"
        "}"
        "QHeaderView::section:hover {"
        "    background-color: #3e3e42;"
        "}"
    );
    
    // Selection behavior
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);  // Start with single, switch to extended on Ctrl/Shift
    
    // Performance optimization
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    
    // Row height
    verticalHeader()->setDefaultSectionSize(28);
    verticalHeader()->hide();
}

void CustomMarketWatch::setupHeader()
{
    // Header behavior
    horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    horizontalHeader()->setStretchLastSection(false);
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
    menu->setStyleSheet(
        "QMenu {"
        "    background-color: #252526;"
        "    color: #ffffff;"
        "    border: 1px solid #3e3e42;"
        "}"
        "QMenu::item {"
        "    padding: 6px 20px;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #094771;"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background: #3e3e42;"
        "    margin: 4px 0px;"
        "}"
    );
    
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

bool CustomMarketWatch::eventFilter(QObject *obj, QEvent *event)
{
    // Handle keyboard navigation for Shift+Arrow keys
    if (obj == this) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            int key = keyEvent->key();
            bool shift = keyEvent->modifiers() & Qt::ShiftModifier;
            bool ctrl = keyEvent->modifiers() & Qt::ControlModifier;

            if (key == Qt::Key_Up || key == Qt::Key_Down) {
                if (shift || ctrl) {
                    // Switch to extended selection mode for multi-select
                    if (selectionMode() != QAbstractItemView::ExtendedSelection) {
                        setSelectionMode(QAbstractItemView::ExtendedSelection);
                        qDebug() << "[EventFilter] Switched to ExtendedSelection mode (Shift/Ctrl detected)";
                    }
                } else {
                    // Switch back to single selection
                    if (selectionMode() != QAbstractItemView::SingleSelection) {
                        setSelectionMode(QAbstractItemView::SingleSelection);
                        qDebug() << "[EventFilter] Switched to SingleSelection mode";
                    }
                }
            }
        }
    }

    // Handle mouse events for drag-and-drop
    if (obj == viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                QModelIndex proxyIndex = indexAt(mouseEvent->pos());
                int proxyRow = proxyIndex.row();
                bool shift = mouseEvent->modifiers() & Qt::ShiftModifier;
                bool ctrl = mouseEvent->modifiers() & Qt::ControlModifier;
                bool isSelected = selectionModel()->isRowSelected(proxyRow, QModelIndex());

                qDebug() << "[EventFilter] MousePress at row:" << proxyRow << "isSelected:" << isSelected 
                         << "modifiers:" << (shift ? "SHIFT" : (ctrl ? "CTRL" : "NO"));

                if (proxyIndex.isValid()) {
                    if (shift) {
                        // Shift-click: range selection
                        setSelectionMode(QAbstractItemView::ExtendedSelection);
                        if (m_selectionAnchor < 0) {
                            m_selectionAnchor = proxyRow;
                        }
                        qDebug() << "[EventFilter] Shift-click selection from anchor" << m_selectionAnchor << "to" << proxyRow;
                    } else if (ctrl) {
                        // Ctrl-click: toggle selection
                        setSelectionMode(QAbstractItemView::ExtendedSelection);
                        m_selectionAnchor = proxyRow;
                        qDebug() << "[EventFilter] Ctrl-click toggle at row" << proxyRow;
                    } else {
                        // Normal click
                        setSelectionMode(QAbstractItemView::SingleSelection);
                        m_selectionAnchor = proxyRow;
                        qDebug() << "[EventFilter] Set selection anchor to row" << proxyRow;

                        if (isSelected) {
                            // Prepare for drag
                            m_dragStartPos = mouseEvent->pos();
                            m_isDragging = false;
                            m_draggedTokens.clear();

                            // Collect tokens from selected rows
                            QModelIndexList selected = selectionModel()->selectedRows();
                            for (const QModelIndex &idx : selected) {
                                int sourceRow = mapToSource(idx.row());
                                int token = getTokenForRow(sourceRow);
                                if (token > 0 && !isBlankRow(sourceRow)) {
                                    m_draggedTokens.append(token);
                                    qDebug() << "[EventFilter] Drag prep: token" << token;
                                }
                            }
                            return false;  // Let normal selection happen
                        } else {
                            qDebug() << "[EventFilter] Normal single selection";
                        }
                    }
                }
            }
        }
        else if (event->type() == QEvent::MouseMove) {
            if (!m_isDragging && !m_dragStartPos.isNull() && !m_draggedTokens.isEmpty()) {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                int distance = (mouseEvent->pos() - m_dragStartPos).manhattanLength();

                if (distance >= QApplication::startDragDistance()) {
                    m_isDragging = true;
                    viewport()->setCursor(Qt::ClosedHandCursor);
                    qDebug() << "[EventFilter] Starting drag - distance:" << distance << "tokens:" << m_draggedTokens.size();
                }
            }

            if (m_isDragging) {
                viewport()->update();
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                qDebug() << "[EventFilter] MouseRelease - isDragging:" << m_isDragging;

                if (m_isDragging && !m_draggedTokens.isEmpty()) {
                    // Get drop position (in source model coordinates)
                    int targetSourceRow = getDropRow(mouseEvent->pos());
                    qDebug() << "[EventFilter] Drop detected, target source row:" << targetSourceRow;

                    // Perform the move (subclass implements actual logic)
                    performRowMoveByTokens(m_draggedTokens, targetSourceRow);

                    // Reset drag state
                    m_isDragging = false;
                    m_draggedTokens.clear();
                    m_dragStartPos = QPoint();
                    viewport()->setCursor(Qt::ArrowCursor);
                    viewport()->update();
                    return true;
                }

                // Reset drag state
                m_isDragging = false;
                m_dragStartPos = QPoint();
                viewport()->setCursor(Qt::ArrowCursor);
            }
        }
    }
    
    return QTableView::eventFilter(obj, event);
}

int CustomMarketWatch::mapToSource(int proxyRow) const
{
    if (proxyRow < 0) return -1;
    QModelIndex proxyIndex = m_proxyModel->index(proxyRow, 0);
    QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
    return sourceIndex.row();
}

int CustomMarketWatch::mapToProxy(int sourceRow) const
{
    if (sourceRow < 0 || !m_proxyModel->sourceModel()) return -1;
    QModelIndex sourceIndex = m_proxyModel->sourceModel()->index(sourceRow, 0);
    QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
    return proxyIndex.row();
}

int CustomMarketWatch::getDropRow(const QPoint &pos) const
{
    QModelIndex index = indexAt(pos);
    
    qDebug() << "[GetDropRow] Mouse pos:" << pos << "indexAt valid:" << index.isValid();
    
    if (index.isValid()) {
        int proxyRow = index.row();
        int targetRow = mapToSource(proxyRow);
        
        qDebug() << "[GetDropRow] Proxy row:" << proxyRow << "Source row:" << targetRow;
        
        // Determine if we should insert before or after
        QRect rect = visualRect(index);
        bool insertAfter = (pos.y() > rect.center().y());
        
        qDebug() << "[GetDropRow] Visual rect:" << rect << "center.y:" << rect.center().y() 
                 << "mouse.y:" << pos.y() << "insertAfter:" << insertAfter;
        
        if (insertAfter) {
            targetRow++;
        }
        
        qDebug() << "[GetDropRow] Final target row:" << targetRow;
        return targetRow;
    }
    
    // Drop at the end
    if (m_proxyModel->sourceModel()) {
        int endRow = m_proxyModel->sourceModel()->rowCount();
        qDebug() << "[GetDropRow] Dropping at end, row:" << endRow;
        return endRow;
    }
    
    return 0;
}

void CustomMarketWatch::performRowMoveByTokens(const QList<int> &tokens, int targetSourceRow)
{
    // Default implementation does nothing
    // Subclasses override this to implement actual move logic
    qDebug() << "[CustomMarketWatch] performRowMoveByTokens called (override in subclass)";
    qDebug() << "  Tokens:" << tokens << "Target:" << targetSourceRow;
}

void CustomMarketWatch::highlightRow(int sourceRow)
{
    if (sourceRow < 0 || !m_proxyModel->sourceModel()) {
        return;
    }
    
    int rowCount = m_proxyModel->sourceModel()->rowCount();
    if (sourceRow >= rowCount) {
        return;
    }
    
    // Map to proxy coordinates
    int proxyRow = mapToProxy(sourceRow);
    if (proxyRow < 0) {
        return;
    }
    
    // Select and scroll to the row
    selectRow(proxyRow);
    QModelIndex proxyIndex = m_proxyModel->index(proxyRow, 0);
    scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
    
    // Flash effect: clear selection after 150ms, then re-select after 300ms
    QTimer::singleShot(150, this, [this, proxyRow]() {
        clearSelection();
        
        QTimer::singleShot(150, this, [this, proxyRow]() {
            selectRow(proxyRow);
        });
    });
    
    qDebug() << "[CustomMarketWatch] Highlighted source row" << sourceRow << "(proxy row" << proxyRow << ")";
}

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
    
    // Install event filter for drag-and-drop on viewport only
    viewport()->installEventFilter(this);
    
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
    m_proxyModel->setDynamicSortFilter(true);  // Enable automatic re-sorting
}

void CustomMarketWatch::applyDefaultStyling()
{
    // Light theme styling
    setAlternatingRowColors(false);
    setShowGrid(false);
    setStyleSheet(
        "QTableView {"
        "    background-color: #ffffff;"
        "    color: #1e293b;"
        "    gridline-color: #f1f5f9;"
        "    selection-background-color: #dbeafe;"
        "    selection-color: #1e40af;"
        "    outline: 0;"
        "}"
        "QHeaderView::section {"
        "    background-color: #f8fafc;"
        "    color: #475569;"
        "    padding: 4px;"
        "    border: 1px solid #e2e8f0;"
        "    font-weight: bold;"
        "}"
        "QHeaderView::section:hover {"
        "    background-color: #e2e8f0;"
        "}"
    );
    
    // Selection behavior
    setSelectionBehavior(QAbstractItemView::SelectRows);
    // Set the default selection mode to single selection.
    // It will be dynamically changed to ExtendedSelection when modifier keys are pressed.
    setSelectionMode(QAbstractItemView::SingleSelection);
    
    // Performance optimization
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    
    // Row height
    verticalHeader()->setDefaultSectionSize(28);
    verticalHeader()->hide();
    
    // Disable tab key navigation to prevent focus jumping between cells
    setTabKeyNavigation(false);
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
        "    background-color: #ffffff;"
        "    color: #1e293b;"
        "    border: 1px solid #e2e8f0;"
        "}"
        "QMenu::item {"
        "    padding: 6px 20px;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #dbeafe;"
        "    color: #1e40af;"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background: #e2e8f0;"
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

// mousePressEvent is no longer needed; selection is handled by the base class
// based on the dynamically changing selection mode.

bool CustomMarketWatch::eventFilter(QObject *obj, QEvent *event)
{
    // This event filter handles the custom drag & drop implementation.
    if (obj != viewport()) {
        return QTableView::eventFilter(obj, event);
    }

    switch (event->type()) {
        case QEvent::MouseButtonPress: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                QModelIndex index = indexAt(mouseEvent->pos());
                if (index.isValid()) {
                    bool isModifierPressed = (mouseEvent->modifiers() & Qt::ShiftModifier) || (mouseEvent->modifiers() & Qt::ControlModifier);
                    bool isRowSelected = selectionModel()->isRowSelected(index.row(), QModelIndex());

                    // If clicking on an already selected row without modifiers, it's a potential drag of that selection.
                    // We must consume the event to prevent the base class from clearing the multi-selection.
                    if (!isModifierPressed && isRowSelected) {
                        m_dragStartPos = mouseEvent->pos();
                        m_isDragging = false;
                        m_draggedTokens.clear();
                        return true; // Consume the event to preserve the selection for dragging.
                    }
                }
                // For all other cases (new selection, modifier clicks), just prepare for a potential drag
                // but let the view handle the actual selection change.
                m_dragStartPos = mouseEvent->pos();
                m_isDragging = false;
                m_draggedTokens.clear();
            }
            break;
        }

        case QEvent::MouseMove: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if ((mouseEvent->buttons() & Qt::LeftButton) && !m_dragStartPos.isNull() && !m_isDragging) {
                if ((mouseEvent->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                    // Drag gesture confirmed.
                    m_draggedTokens.clear();
                    QModelIndexList selectedRows = selectionModel()->selectedRows();
                    for (const QModelIndex &idx : selectedRows) {
                        int sourceRow = mapToSource(idx.row());
                        int token = getTokenForRow(sourceRow);
                        if (token > 0 && !isBlankRow(sourceRow)) {
                            m_draggedTokens.append(token);
                        }
                    }

                    if (!m_draggedTokens.isEmpty()) {
                        m_isDragging = true;
                        viewport()->setCursor(Qt::ClosedHandCursor);
                        qDebug() << "[EventFilter] Drag started with" << m_draggedTokens.size() << "tokens.";
                    } else {
                        m_dragStartPos = QPoint(); // No valid tokens, cancel drag.
                    }
                }
            }
            break;
        }

        case QEvent::MouseButtonRelease: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                if (m_isDragging) {
                    qDebug() << "[EventFilter] Drop detected.";
                    int targetSourceRow = getDropRow(mouseEvent->pos());
                    performRowMoveByTokens(m_draggedTokens, targetSourceRow);
                }
                // Always reset drag state on release.
                m_isDragging = false;
                m_draggedTokens.clear();
                m_dragStartPos = QPoint();
                viewport()->setCursor(Qt::ArrowCursor);
            }
            break;
        }

        default:
            break;
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

void CustomMarketWatch::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) {
        QTableView::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control) {
        qDebug() << "[keyPressEvent] Modifier key pressed. Current mode:" << selectionMode();
        if (selectionMode() != QAbstractItemView::ExtendedSelection) {
            qDebug() << "[keyPressEvent] ==> Switching to ExtendedSelection mode.";
            setSelectionMode(QAbstractItemView::ExtendedSelection);
        }
    }
    QTableView::keyPressEvent(event); // Call base class implementation
}

void CustomMarketWatch::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) {
        QTableView::keyReleaseEvent(event);
        return;
    }

    bool isShiftRelease = (event->key() == Qt::Key_Shift);
    bool isCtrlRelease = (event->key() == Qt::Key_Control);

    if (isShiftRelease || isCtrlRelease) {
        qDebug() << "[keyReleaseEvent] Modifier key released. Key:" << event->key();
        
        // Get the current state of ALL keyboard modifiers
        Qt::KeyboardModifiers currentMods = QApplication::keyboardModifiers();
        qDebug() << "[keyReleaseEvent] Keyboard modifiers reported by Qt:" << currentMods;

        // Create a mask of the modifiers we are tracking for selection
        Qt::KeyboardModifiers selectionMods = Qt::ShiftModifier | Qt::ControlModifier;
        
        // IMPORTANT: From the currently pressed modifiers, remove the one that was JUST released.
        if (isShiftRelease) {
            currentMods &= ~Qt::ShiftModifier;
        }
        if (isCtrlRelease) {
            currentMods &= ~Qt::ControlModifier;
        }

        qDebug() << "[keyReleaseEvent] Modifiers AFTER accounting for release:" << currentMods;

        // Now, check if any of our tracked selection modifiers are still active.
        if ((currentMods & selectionMods) == 0) {
             qDebug() << "[keyReleaseEvent] ==> No selection modifiers left. Reverting to SingleSelection mode.";
             setSelectionMode(QAbstractItemView::SingleSelection);
        } else {
             qDebug() << "[keyReleaseEvent] ==> Other selection modifiers still active. Staying in ExtendedSelection mode.";
        }
    }
    
    QTableView::keyReleaseEvent(event); // Call base class implementation
}
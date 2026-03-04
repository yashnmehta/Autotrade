#include "views/GenericProfileDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QInputDialog>
#include <QMessageBox>
#include <QDebug>
#include <QtGlobal>    // Q_OS_MACOS

// ============================================================================
// Constructor
// ============================================================================
GenericProfileDialog::GenericProfileDialog(const QString &windowTitle,
                                           const QList<GenericColumnInfo> &allColumns,
                                           GenericProfileManager *manager,
                                           const GenericTableProfile &currentProfile,
                                           QWidget *parent)
    : QDialog(parent)
    , m_allColumns(allColumns)
    , m_manager(manager)
    , m_profile(currentProfile)
    , m_accepted(false)
{
    setWindowTitle(windowTitle + " — Column Selection");
    setModal(true);

#ifdef Q_OS_MACOS
    resize(600, 460);
#else
    resize(560, 420);
#endif
    setMinimumSize(520, 380);

    // ── Light-theme stylesheet ────────────────────────────────────────────
    setStyleSheet(
        "QDialog {"
        "    background-color: #ffffff;"
        "    color: #1e293b;"
        "    font-size: 12px;"
        "}"

        /* Labels */
        "QLabel {"
        "    color: #475569;"
        "    font-size: 11px;"
        "    font-weight: 600;"
        "    letter-spacing: 0.3px;"
        "}"

        /* ComboBox */
        "QComboBox {"
        "    background-color: #ffffff; color: #1e293b;"
        "    border: 1px solid #e2e8f0; border-radius: 4px;"
        "    padding: 4px 8px; min-height: 24px; font-size: 12px;"
        "}"
        "QComboBox:hover  { border-color: #94a3b8; }"
        "QComboBox:focus  { border-color: #3b82f6; }"
        "QComboBox::drop-down { width: 18px; border: none; }"
        "QComboBox::down-arrow {"
        "    border-left: 4px solid transparent;"
        "    border-right: 4px solid transparent;"
        "    border-top: 5px solid #64748b;"
        "    margin-right: 4px;"
        "}"
        "QComboBox QAbstractItemView {"
        "    background-color: #ffffff; color: #1e293b;"
        "    selection-background-color: #dbeafe; selection-color: #1e40af;"
        "    border: 1px solid #e2e8f0; outline: none;"
        "}"

        /* Lists */
        "QListWidget {"
        "    background-color: #f8fafc; color: #1e293b;"
        "    border: 1px solid #e2e8f0; border-radius: 4px;"
        "    font-size: 12px; outline: none; padding: 2px;"
        "}"
        "QListWidget::item {"
        "    padding: 5px 8px;"
        "    border-radius: 3px;"
        "    margin: 1px 2px;"
        "    color: #1e293b;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #dbeafe; color: #1e40af;"
        "}"
        "QListWidget::item:hover:!selected { background-color: #f1f5f9; }"
        "QListWidget::item:focus { outline: none; }"

        /* Base button style — neutral */
        "QPushButton {"
        "    background-color: #f8fafc; color: #334155;"
        "    border: 1px solid #e2e8f0; border-radius: 4px;"
        "    padding: 5px 12px; font-size: 12px; font-weight: 500;"
        "    min-width: 70px; min-height: 26px;"
        "}"
        "QPushButton:hover   { background-color: #f1f5f9; border-color: #94a3b8; color: #1e293b; }"
        "QPushButton:pressed { background-color: #e2e8f0; }"
        "QPushButton:disabled { background-color: #f8fafc; color: #94a3b8; border-color: #f1f5f9; }"
        "QPushButton:focus   { outline: none; border-color: #3b82f6; }"

        /* Primary / OK button */
        "QPushButton#btn_ok {"
        "    background-color: #2563eb; color: #ffffff;"
        "    border-color: #2563eb; min-width: 70px;"
        "}"
        "QPushButton#btn_ok:hover   { background-color: #1d4ed8; border-color: #1d4ed8; }"
        "QPushButton#btn_ok:pressed { background-color: #1e40af; }"

        /* Save button — blue */
        "QPushButton#btn_save {"
        "    background-color: #2563eb; color: #ffffff; border-color: #2563eb;"
        "}"
        "QPushButton#btn_save:hover { background-color: #1d4ed8; border-color: #1d4ed8; }"

        /* Delete button — red */
        "QPushButton#btn_delete {"
        "    background-color: #ffffff; color: #dc2626; border-color: #fca5a5;"
        "}"
        "QPushButton#btn_delete:hover { background-color: #fef2f2; border-color: #dc2626; }"

        /* Add button — green */
        "QPushButton#btn_add {"
        "    background-color: #16a34a; color: #ffffff; border-color: #16a34a;"
        "}"
        "QPushButton#btn_add:hover { background-color: #15803d; border-color: #15803d; }"

        /* Remove button — red-tinted */
        "QPushButton#btn_remove {"
        "    background-color: #ffffff; color: #dc2626; border-color: #fca5a5;"
        "}"
        "QPushButton#btn_remove:hover { background-color: #fef2f2; border-color: #dc2626; }"
    );

    setupUI();
    setupConnections();
    loadProfile(currentProfile);
}

// ============================================================================
// UI
// ============================================================================
void GenericProfileDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // ── Top: profile combo + Save / Delete / Set Default ─────────────────
    QHBoxLayout *profileLayout = new QHBoxLayout();
    profileLayout->setSpacing(6);

    QLabel *profileLabel = new QLabel("Column Profile:");
    profileLabel->setStyleSheet("QLabel { color: #1e293b; font-size: 12px; font-weight: 600; }");
    profileLayout->addWidget(profileLabel);

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setMinimumWidth(140);
    m_profileCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    profileLayout->addWidget(m_profileCombo);

    m_saveButton       = new QPushButton("Save",        this);
    m_deleteButton     = new QPushButton("Delete",      this);
    m_setDefaultButton = new QPushButton("Set Default", this);

    m_saveButton->setObjectName("btn_save");
    m_deleteButton->setObjectName("btn_delete");
    // m_setDefaultButton uses base style (neutral)

    m_saveButton->setMinimumWidth(60);
    m_deleteButton->setMinimumWidth(60);
    m_setDefaultButton->setMinimumWidth(82);  // prevent clipping

    profileLayout->addWidget(m_saveButton);
    profileLayout->addWidget(m_deleteButton);
    profileLayout->addWidget(m_setDefaultButton);
    mainLayout->addLayout(profileLayout);

    // ── Thin divider ─────────────────────────────────────────────────────
    QFrame *divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("QFrame { background-color: #e2e8f0; max-height: 1px; border: none; }");
    mainLayout->addWidget(divider);

    // ── Middle: dual list ────────────────────────────────────────────────
    QHBoxLayout *listsLayout = new QHBoxLayout();
    listsLayout->setSpacing(6);

    // Available column
    QVBoxLayout *availLayout = new QVBoxLayout();
    availLayout->setSpacing(4);
    QLabel *availLabel = new QLabel("Available Columns");
    availLayout->addWidget(availLabel);
    m_availableList = new QListWidget(this);
    m_availableList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_availableList->setMinimumWidth(150);
    availLayout->addWidget(m_availableList);
    listsLayout->addLayout(availLayout, 5);

    // Add / Remove buttons (center column)
    QVBoxLayout *arrowLayout = new QVBoxLayout();
    arrowLayout->setSpacing(6);
    arrowLayout->addStretch();
    m_addButton    = new QPushButton("Add \u25B6",    this);   // ▶
    m_removeButton = new QPushButton("\u25C0 Remove", this);   // ◀
    m_addButton->setObjectName("btn_add");
    m_removeButton->setObjectName("btn_remove");
    m_addButton->setMinimumWidth(80);
    m_removeButton->setMinimumWidth(80);
    arrowLayout->addWidget(m_addButton);
    arrowLayout->addWidget(m_removeButton);
    arrowLayout->addStretch();
    listsLayout->addLayout(arrowLayout);

    // Selected columns
    QVBoxLayout *selLayout = new QVBoxLayout();
    selLayout->setSpacing(4);
    QLabel *selLabel = new QLabel("Selected Columns");
    selLayout->addWidget(selLabel);
    m_selectedList = new QListWidget(this);
    m_selectedList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_selectedList->setMinimumWidth(150);
    selLayout->addWidget(m_selectedList);
    listsLayout->addLayout(selLayout, 5);

    // Move Up / Down
    QVBoxLayout *moveLayout = new QVBoxLayout();
    moveLayout->setSpacing(6);
    moveLayout->addStretch();
    m_moveUpButton   = new QPushButton("\u25B2 Move Up",   this);  // ▲
    m_moveDownButton = new QPushButton("\u25BC Move Down", this);  // ▼
    m_moveUpButton->setMinimumWidth(90);    // prevent "love Down" clipping
    m_moveDownButton->setMinimumWidth(90);
    moveLayout->addWidget(m_moveUpButton);
    moveLayout->addWidget(m_moveDownButton);
    moveLayout->addStretch();
    listsLayout->addLayout(moveLayout);

    mainLayout->addLayout(listsLayout, 1);

    // ── Bottom: OK / Cancel ──────────────────────────────────────────────
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(6);
    bottomLayout->addStretch();
    m_okButton     = new QPushButton("OK",     this);
    m_cancelButton = new QPushButton("Cancel", this);
    m_okButton->setObjectName("btn_ok");
    m_okButton->setDefault(true);
    m_okButton->setMinimumWidth(72);
    m_cancelButton->setMinimumWidth(72);
    bottomLayout->addWidget(m_okButton);
    bottomLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(bottomLayout);

    refreshProfileList();
}

// ============================================================================
// Connections
// ============================================================================
void GenericProfileDialog::setupConnections()
{
    connect(m_profileCombo, &QComboBox::currentTextChanged, this, &GenericProfileDialog::onProfileChanged);
    connect(m_addButton,      &QPushButton::clicked, this, &GenericProfileDialog::onAddColumn);
    connect(m_removeButton,   &QPushButton::clicked, this, &GenericProfileDialog::onRemoveColumn);
    connect(m_moveUpButton,   &QPushButton::clicked, this, &GenericProfileDialog::onMoveUp);
    connect(m_moveDownButton, &QPushButton::clicked, this, &GenericProfileDialog::onMoveDown);
    connect(m_saveButton,     &QPushButton::clicked, this, &GenericProfileDialog::onSaveProfile);
    connect(m_deleteButton,   &QPushButton::clicked, this, &GenericProfileDialog::onDeleteProfile);
    connect(m_setDefaultButton, &QPushButton::clicked, this, &GenericProfileDialog::onSetAsDefault);
    connect(m_okButton,       &QPushButton::clicked, this, &GenericProfileDialog::onAccepted);
    connect(m_cancelButton,   &QPushButton::clicked, this, &GenericProfileDialog::onRejected);
}

// ============================================================================
// Helpers
// ============================================================================
QString GenericProfileDialog::columnNameById(int id) const
{
    for (const auto &c : m_allColumns) {
        if (c.id == id) return c.name;
    }
    return QString("Column %1").arg(id);
}

void GenericProfileDialog::loadProfile(const GenericTableProfile &profile)
{
    m_profile = profile;
    updateAvailableColumns();
    updateSelectedColumns();
}

void GenericProfileDialog::updateAvailableColumns()
{
    m_availableList->clear();
    for (const auto &info : m_allColumns) {
        if (!m_profile.isColumnVisible(info.id)) {
            auto *item = new QListWidgetItem(info.name);
            item->setData(Qt::UserRole, info.id);
            item->setForeground(QColor("#1e293b"));
            m_availableList->addItem(item);
        }
    }
}

void GenericProfileDialog::updateSelectedColumns()
{
    m_selectedList->clear();
    for (int id : m_profile.columnOrder()) {
        if (m_profile.isColumnVisible(id)) {
            auto *item = new QListWidgetItem(columnNameById(id));
            item->setData(Qt::UserRole, id);
            item->setForeground(QColor("#1e293b"));
            m_selectedList->addItem(item);
        }
    }
}

void GenericProfileDialog::refreshProfileList()
{
    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();

    // Presets first
    for (const QString &name : m_manager->presetNames())
        m_profileCombo->addItem(name);

    // Custom profiles (with separator)
    QStringList custom = m_manager->customProfileNames();
    if (!custom.isEmpty()) {
        m_profileCombo->insertSeparator(m_profileCombo->count());
        for (const QString &name : custom)
            m_profileCombo->addItem(name);
    }

    // Select current
    int idx = m_profileCombo->findText(m_profile.name());
    if (idx >= 0)
        m_profileCombo->setCurrentIndex(idx);

    m_profileCombo->blockSignals(false);
}

// ============================================================================
// Slots
// ============================================================================
void GenericProfileDialog::onProfileChanged(const QString &profileName)
{
    if (!m_manager->hasProfile(profileName)) return;
    loadProfile(m_manager->getProfile(profileName));
}

void GenericProfileDialog::onAddColumn()
{
    auto items = m_availableList->selectedItems();
    if (items.isEmpty()) return;

    QList<int> order = m_profile.columnOrder();
    for (auto *item : items) {
        int id = item->data(Qt::UserRole).toInt();
        m_profile.setColumnVisible(id, true);
        if (!order.contains(id)) order.append(id);
    }
    m_profile.setColumnOrder(order);
    updateAvailableColumns();
    updateSelectedColumns();
}

void GenericProfileDialog::onRemoveColumn()
{
    auto items = m_selectedList->selectedItems();
    if (items.isEmpty()) return;

    if (m_profile.visibleColumnCount() - items.size() < 1) {
        QMessageBox::warning(this, "Cannot Remove",
                             "At least one column must remain visible.");
        return;
    }

    for (auto *item : items) {
        m_profile.setColumnVisible(item->data(Qt::UserRole).toInt(), false);
    }
    updateAvailableColumns();
    updateSelectedColumns();
}

void GenericProfileDialog::onMoveUp()
{
    int row = m_selectedList->currentRow();
    if (row <= 0) return;
    auto *item = m_selectedList->takeItem(row);
    m_selectedList->insertItem(row - 1, item);
    m_selectedList->setCurrentRow(row - 1);
}

void GenericProfileDialog::onMoveDown()
{
    int row = m_selectedList->currentRow();
    if (row < 0 || row >= m_selectedList->count() - 1) return;
    auto *item = m_selectedList->takeItem(row);
    m_selectedList->insertItem(row + 1, item);
    m_selectedList->setCurrentRow(row + 1);
}

void GenericProfileDialog::onSaveProfile()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Save Profile",
                                         "Profile name:", QLineEdit::Normal,
                                         m_profile.name(), &ok);
    if (!ok || name.isEmpty()) return;

    if (m_manager->isPreset(name)) {
        QMessageBox::warning(this, "Invalid Name",
                             "Cannot overwrite a preset profile. Choose a different name.");
        return;
    }

    m_profile.setName(name);

    // Rebuild order from UI (visible in UI order, then hidden in current order)
    QList<int> newOrder;
    for (int i = 0; i < m_selectedList->count(); ++i)
        newOrder.append(m_selectedList->item(i)->data(Qt::UserRole).toInt());
    for (int id : m_profile.columnOrder()) {
        if (!m_profile.isColumnVisible(id))
            newOrder.append(id);
    }
    m_profile.setColumnOrder(newOrder);

    m_manager->saveCustomProfile(m_profile);
    m_manager->saveLastUsedProfile(m_profile);       // always persist as last-used too

    QMessageBox::information(this, "Profile Saved",
                             QString("Profile '%1' has been saved.").arg(name));
    refreshProfileList();
}

void GenericProfileDialog::onDeleteProfile()
{
    QString name = m_profileCombo->currentText();

    if (m_manager->isPreset(name)) {
        QMessageBox::warning(this, "Cannot Delete",
                             "Preset profiles cannot be deleted.");
        return;
    }

    auto reply = QMessageBox::question(this, "Delete Profile",
                                       QString("Delete profile '%1'?").arg(name),
                                       QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        if (m_manager->deleteCustomProfile(name)) {
            refreshProfileList();
            m_profileCombo->setCurrentIndex(0);
        }
    }
}

void GenericProfileDialog::onSetAsDefault()
{
    QString profileName = m_profileCombo->currentText();
    m_manager->saveDefaultProfileName(profileName);
    // Also persist the selected profile as last-used so it survives restart
    if (m_manager->hasProfile(profileName)) {
        m_manager->saveLastUsedProfile(m_manager->getProfile(profileName));
    }
    QMessageBox::information(this, "Default Set",
                             QString("Profile '%1' set as default.").arg(profileName));
}

void GenericProfileDialog::onAccepted()
{
    // Rebuild final column order from UI
    QList<int> newOrder;
    for (int i = 0; i < m_selectedList->count(); ++i)
        newOrder.append(m_selectedList->item(i)->data(Qt::UserRole).toInt());
    for (int id : m_profile.columnOrder()) {
        if (!m_profile.isColumnVisible(id))
            newOrder.append(id);
    }
    m_profile.setColumnOrder(newOrder);

    m_accepted = true;
    accept();
}

void GenericProfileDialog::onRejected()
{
    m_accepted = false;
    reject();
}

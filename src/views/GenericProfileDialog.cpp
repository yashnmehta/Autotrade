#include "views/GenericProfileDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QDebug>
#include <QFile>
#include <QMenu>

GenericProfileDialog::GenericProfileDialog(const QString &windowName, 
                                         const QList<GenericColumnInfo> &allColumns, 
                                         const GenericTableProfile &currentProfile, 
                                         QWidget *parent)
    : QDialog(parent)
    , m_windowName(windowName)
    , m_allColumns(allColumns)
    , m_profile(currentProfile)
    , m_manager("profiles")
    , m_accepted(false)
{
    setWindowTitle(windowName + " - Column Selection");
    setModal(true);
    resize(700, 500);
    
    // Use the same styling as Market Watch's dialog
    setStyleSheet(
        "QDialog { background-color: #1e1e1e; color: #cccccc; }"
        "QLabel { color: #e0e0e0; font-size: 13px; font-weight: 500; }"
        "QComboBox { background-color: #2d2d30; color: #cccccc; border: 1px solid #454545; border-radius: 2px; padding: 6px 10px; }"
        "QListWidget { background-color: #252526; color: #cccccc; border: 1px solid #454545; border-radius: 2px; }"
        "QListWidget::item:selected { background-color: #094771; color: #ffffff; }"
        "QPushButton { background-color: #0e639c; color: #ffffff; border: 1px solid #0e639c; border-radius: 2px; padding: 7px 18px; min-width: 75px; }"
        "QPushButton:hover { background-color: #1177bb; }"
        "QPushButton:disabled { background-color: #2d2d30; color: #656565; }"
    );
    
    setupUI();
    setupConnections();
    loadProfile(currentProfile);
}

void GenericProfileDialog::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    QHBoxLayout *profileLayout = new QHBoxLayout();
    profileLayout->addWidget(new QLabel("Profile:"));
    m_profileCombo = new QComboBox(this);
    m_profileCombo->setMinimumWidth(200);
    profileLayout->addWidget(m_profileCombo);
    
    m_saveButton = new QPushButton("Save", this);
    m_deleteButton = new QPushButton("Delete", this);
    m_setDefaultButton = new QPushButton("Set Default", this);
    
    profileLayout->addWidget(m_saveButton);
    profileLayout->addWidget(m_deleteButton);
    profileLayout->addWidget(m_setDefaultButton);
    profileLayout->addStretch();
    mainLayout->addLayout(profileLayout);
    
    QHBoxLayout *listsLayout = new QHBoxLayout();
    
    QVBoxLayout *availLayout = new QVBoxLayout();
    availLayout->addWidget(new QLabel("Available Columns"));
    m_availableList = new QListWidget(this);
    m_availableList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    availLayout->addWidget(m_availableList);
    listsLayout->addLayout(availLayout);
    
    QVBoxLayout *btnLayout = new QVBoxLayout();
    btnLayout->addStretch();
    m_addButton = new QPushButton("Add >", this);
    m_removeButton = new QPushButton("< Remove", this);
    btnLayout->addWidget(m_addButton);
    btnLayout->addWidget(m_removeButton);
    btnLayout->addStretch();
    listsLayout->addLayout(btnLayout);
    
    QVBoxLayout *selLayout = new QVBoxLayout();
    selLayout->addWidget(new QLabel("Selected Columns"));
    m_selectedList = new QListWidget(this);
    m_selectedList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    selLayout->addWidget(m_selectedList);
    listsLayout->addLayout(selLayout);
    
    QVBoxLayout *moveLayout = new QVBoxLayout();
    moveLayout->addStretch();
    m_moveUpButton = new QPushButton("Move Up", this);
    m_moveDownButton = new QPushButton("Move Down", this);
    moveLayout->addWidget(m_moveUpButton);
    moveLayout->addWidget(m_moveDownButton);
    moveLayout->addStretch();
    listsLayout->addLayout(moveLayout);
    
    mainLayout->addLayout(listsLayout);
    
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    m_okButton = new QPushButton("OK", this);
    m_cancelButton = new QPushButton("Cancel", this);
    bottomLayout->addWidget(m_okButton);
    bottomLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(bottomLayout);
    
    refreshProfileList();
}

void GenericProfileDialog::setupConnections() {
    connect(m_profileCombo, &QComboBox::currentTextChanged, this, &GenericProfileDialog::onProfileChanged);
    connect(m_addButton, &QPushButton::clicked, this, &GenericProfileDialog::onAddColumn);
    connect(m_removeButton, &QPushButton::clicked, this, &GenericProfileDialog::onRemoveColumn);
    connect(m_moveUpButton, &QPushButton::clicked, this, &GenericProfileDialog::onMoveUp);
    connect(m_moveDownButton, &QPushButton::clicked, this, &GenericProfileDialog::onMoveDown);
    connect(m_saveButton, &QPushButton::clicked, this, &GenericProfileDialog::onSaveProfile);
    connect(m_deleteButton, &QPushButton::clicked, this, &GenericProfileDialog::onDeleteProfile);
    connect(m_setDefaultButton, &QPushButton::clicked, this, &GenericProfileDialog::onSetAsDefault);
    connect(m_okButton, &QPushButton::clicked, this, &GenericProfileDialog::onAccepted);
    connect(m_cancelButton, &QPushButton::clicked, this, &GenericProfileDialog::onRejected);
}

void GenericProfileDialog::loadProfile(const GenericTableProfile &profile) {
    m_profile = profile;
    updateAvailableColumns();
    updateSelectedColumns();
}

void GenericProfileDialog::updateAvailableColumns() {
    m_availableList->clear();
    QList<int> visible = m_profile.columnOrder();
    for (const auto &info : m_allColumns) {
        if (!m_profile.isColumnVisible(info.id)) {
            QListWidgetItem *item = new QListWidgetItem(info.name);
            item->setData(Qt::UserRole, info.id);
            m_availableList->addItem(item);
        }
    }
}

void GenericProfileDialog::updateSelectedColumns() {
    m_selectedList->clear();
    for (int id : m_profile.columnOrder()) {
        if (m_profile.isColumnVisible(id)) {
            for (const auto &info : m_allColumns) {
                if (info.id == id) {
                    QListWidgetItem *item = new QListWidgetItem(info.name);
                    item->setData(Qt::UserRole, id);
                    m_selectedList->addItem(item);
                    break;
                }
            }
        }
    }
}

void GenericProfileDialog::refreshProfileList() {
    m_profileCombo->clear();
    m_profileCombo->addItem("Default");
    m_profileCombo->addItems(m_manager.listProfiles(m_windowName));
    
    int idx = m_profileCombo->findText(m_profile.name());
    if (idx >= 0) m_profileCombo->setCurrentIndex(idx);
}

void GenericProfileDialog::onProfileChanged(const QString &profileName) {
    if (profileName == "Default") {
        GenericTableProfile def("Default");
        for (const auto &info : m_allColumns) {
            def.setColumnVisible(info.id, info.visibleByDefault);
            def.setColumnWidth(info.id, info.defaultWidth);
        }
        QList<int> order;
        for (const auto &info : m_allColumns) order.append(info.id);
        def.setColumnOrder(order);
        loadProfile(def);
    } else {
        GenericTableProfile prof;
        if (m_manager.loadProfile(m_windowName, profileName, prof)) {
            loadProfile(prof);
        }
    }
}

void GenericProfileDialog::onAddColumn() {
    auto items = m_availableList->selectedItems();
    QList<int> order = m_profile.columnOrder();
    for (auto item : items) {
        int id = item->data(Qt::UserRole).toInt();
        m_profile.setColumnVisible(id, true);
        if (!order.contains(id)) order.append(id);
    }
    m_profile.setColumnOrder(order);
    updateAvailableColumns();
    updateSelectedColumns();
}

void GenericProfileDialog::onRemoveColumn() {
    auto items = m_selectedList->selectedItems();
    for (auto item : items) {
        m_profile.setColumnVisible(item->data(Qt::UserRole).toInt(), false);
    }
    updateAvailableColumns();
    updateSelectedColumns();
}

void GenericProfileDialog::onMoveUp() {
    int row = m_selectedList->currentRow();
    if (row <= 0) return;
    QList<int> order = m_profile.columnOrder();
    int id = m_selectedList->item(row)->data(Qt::UserRole).toInt();
    int prevId = m_selectedList->item(row-1)->data(Qt::UserRole).toInt();
    int idx = order.indexOf(id);
    int prevIdx = order.indexOf(prevId);
    if (idx != -1 && prevIdx != -1) {
        order.swapItemsAt(idx, prevIdx);
        m_profile.setColumnOrder(order);
        updateSelectedColumns();
        m_selectedList->setCurrentRow(row - 1);
    }
}

void GenericProfileDialog::onMoveDown() {
    int row = m_selectedList->currentRow();
    if (row < 0 || row >= m_selectedList->count() - 1) return;
    QList<int> order = m_profile.columnOrder();
    int id = m_selectedList->item(row)->data(Qt::UserRole).toInt();
    int nextId = m_selectedList->item(row+1)->data(Qt::UserRole).toInt();
    int idx = order.indexOf(id);
    int nextIdx = order.indexOf(nextId);
    if (idx != -1 && nextIdx != -1) {
        order.swapItemsAt(idx, nextIdx);
        m_profile.setColumnOrder(order);
        updateSelectedColumns();
        m_selectedList->setCurrentRow(row + 1);
    }
}

void GenericProfileDialog::onSaveProfile() {
    bool ok;
    QString name = QInputDialog::getText(this, "Save Profile", "Name:", QLineEdit::Normal, m_profile.name(), &ok);
    if (ok && !name.isEmpty()) {
        if (name == "Default") {
            QMessageBox::warning(this, "Error", "Cannot overwrite 'Default' profile");
            return;
        }
        m_profile.setName(name);
        m_manager.saveProfile(m_windowName, m_profile);
        refreshProfileList();
    }
}

void GenericProfileDialog::onDeleteProfile() {
    QString name = m_profileCombo->currentText();
    if (name == "Default") {
        QMessageBox::warning(this, "Error", "Cannot delete 'Default' profile");
        return;
    }
    
    if (QMessageBox::question(this, "Confirm", "Delete profile '" + name + "'?") == QMessageBox::Yes) {
        QString path = "profiles/" + m_windowName + "_" + name + ".json";
        if (QFile::remove(path)) {
            refreshProfileList();
            m_profileCombo->setCurrentIndex(0);
        }
    }
}

void GenericProfileDialog::onSetAsDefault() {
    m_manager.saveDefaultProfile(m_windowName, m_profile.name());
}

void GenericProfileDialog::onAccepted() { m_accepted = true; accept(); }
void GenericProfileDialog::onRejected() { m_accepted = false; reject(); }

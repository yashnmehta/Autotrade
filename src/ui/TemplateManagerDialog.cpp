#include "ui/TemplateManagerDialog.h"
#include "ui/TradingViewChartWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

TemplateManagerDialog::TemplateManagerDialog(TradingViewChartWidget* chartWidget, QWidget* parent)
    : QDialog(parent), m_chartWidget(chartWidget) {
    setupUi();
    refreshTemplateList();
}

void TemplateManagerDialog::setupUi() {
    setWindowTitle("Manage Chart Templates");
    setMinimumSize(500, 400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Template list group
    QGroupBox* listGroup = new QGroupBox("Available Templates", this);
    QVBoxLayout* listLayout = new QVBoxLayout(listGroup);
    
    m_templateList = new QListWidget(this);
    connect(m_templateList, &QListWidget::itemSelectionChanged, this, &TemplateManagerDialog::onTemplateSelected);
    connect(m_templateList, &QListWidget::itemDoubleClicked, this, [this](){ onLoadTemplate(); });
    listLayout->addWidget(m_templateList);

    mainLayout->addWidget(listGroup);

    // Save new template group
    QGroupBox* saveGroup = new QGroupBox("Save Current Chart", this);
    QHBoxLayout* saveLayout = new QHBoxLayout(saveGroup);
    
    QLabel* nameLabel = new QLabel("Template Name:", this);
    m_templateNameEdit = new QLineEdit(this);
    m_templateNameEdit->setPlaceholderText("Enter template name...");
    m_saveButton = new QPushButton("Save", this);
    connect(m_saveButton, &QPushButton::clicked, this, &TemplateManagerDialog::onSaveTemplate);
    
    saveLayout->addWidget(nameLabel);
    saveLayout->addWidget(m_templateNameEdit);
    saveLayout->addWidget(m_saveButton);

    mainLayout->addWidget(saveGroup);

    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_loadButton = new QPushButton("Load Selected", this);
    m_loadButton->setEnabled(false);
    connect(m_loadButton, &QPushButton::clicked, this, &TemplateManagerDialog::onLoadTemplate);
    
    m_deleteButton = new QPushButton("Delete Selected", this);
    m_deleteButton->setEnabled(false);
    connect(m_deleteButton, &QPushButton::clicked, this, &TemplateManagerDialog::onDeleteTemplate);
    
    QPushButton* refreshButton = new QPushButton("Refresh", this);
    connect(refreshButton, &QPushButton::clicked, this, &TemplateManagerDialog::refreshTemplateList);
    
    QPushButton* closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    
    buttonLayout->addWidget(m_loadButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addWidget(refreshButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);

    mainLayout->addLayout(buttonLayout);

    // Status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("QLabel { color: #666; font-style: italic; }");
    mainLayout->addWidget(m_statusLabel);
}

void TemplateManagerDialog::refreshTemplateList() {
    m_templateList->clear();
    
    if (!m_chartWidget) {
        m_statusLabel->setText("Chart widget not available");
        return;
    }

    QStringList templates = m_chartWidget->getTemplateList();
    m_templateList->addItems(templates);
    
    m_statusLabel->setText(QString("%1 template(s) found").arg(templates.count()));
}

void TemplateManagerDialog::onSaveTemplate() {
    QString templateName = m_templateNameEdit->text().trimmed();
    
    if (templateName.isEmpty()) {
        QMessageBox::warning(this, "Invalid Name", "Please enter a template name.");
        return;
    }

    // Check if template already exists
    QStringList existing = m_chartWidget->getTemplateList();
    if (existing.contains(templateName)) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Template Exists",
            QString("Template '%1' already exists. Overwrite?").arg(templateName),
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    if (m_chartWidget->saveTemplate(templateName)) {
        m_statusLabel->setText(QString("Template '%1' saved successfully").arg(templateName));
        m_templateNameEdit->clear();
        refreshTemplateList();
        
        // Select the newly saved template
        QList<QListWidgetItem*> items = m_templateList->findItems(templateName, Qt::MatchExactly);
        if (!items.isEmpty()) {
            m_templateList->setCurrentItem(items.first());
        }
    } else {
        QMessageBox::critical(this, "Save Failed", 
            QString("Failed to save template '%1'").arg(templateName));
    }
}

void TemplateManagerDialog::onLoadTemplate() {
    QListWidgetItem* item = m_templateList->currentItem();
    if (!item) {
        QMessageBox::warning(this, "No Selection", "Please select a template to load.");
        return;
    }

    QString templateName = item->text();
    
    if (m_chartWidget->loadTemplate(templateName)) {
        m_statusLabel->setText(QString("Template '%1' loaded successfully").arg(templateName));
        // Close dialog after successful load
        QTimer::singleShot(1000, this, &QDialog::accept);
    } else {
        QMessageBox::critical(this, "Load Failed",
            QString("Failed to load template '%1'").arg(templateName));
    }
}

void TemplateManagerDialog::onDeleteTemplate() {
    QListWidgetItem* item = m_templateList->currentItem();
    if (!item) {
        QMessageBox::warning(this, "No Selection", "Please select a template to delete.");
        return;
    }

    QString templateName = item->text();
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Deletion",
        QString("Are you sure you want to delete template '%1'?").arg(templateName),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply != QMessageBox::Yes) {
        return;
    }

    if (m_chartWidget->deleteTemplate(templateName)) {
        m_statusLabel->setText(QString("Template '%1' deleted successfully").arg(templateName));
        refreshTemplateList();
    } else {
        QMessageBox::critical(this, "Delete Failed",
            QString("Failed to delete template '%1'").arg(templateName));
    }
}

void TemplateManagerDialog::onTemplateSelected() {
    bool hasSelection = m_templateList->currentItem() != nullptr;
    m_loadButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection);
}

/**
 * @file TemplateManagerDialog.cpp
 * @brief Unified Template Management Dialog implementation
 *
 * Provides a single place to browse, create, edit, clone, delete,
 * and deploy strategy templates.
 */

#include "strategy/builder/TemplateManagerDialog.h"
#include "ui_TemplateManagerDialog.h"
#include "strategy/persistence/StrategyTemplateRepository.h"
#include "strategy/builder/StrategyDeployDialog.h"
#include "strategy/builder/StrategyTemplateBuilderDialog.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QSplitter>
#include <QTableWidget>

// ═════════════════════════════════════════════════════════════════════════════
// Construction
// ═════════════════════════════════════════════════════════════════════════════

TemplateManagerDialog::TemplateManagerDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::TemplateManagerDialog)
{
    ui->setupUi(this);
    initUI();
    loadTemplates();
    updateButtonStates();
}

TemplateManagerDialog::~TemplateManagerDialog()
{
    delete ui;
}

// ═════════════════════════════════════════════════════════════════════════════
// UI Init (post-setupUi configuration)
// ═════════════════════════════════════════════════════════════════════════════

void TemplateManagerDialog::initUI()
{
    // Table column resize modes
    ui->templateTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->templateTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->templateTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->templateTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->templateTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    // Splitter stretch factors
    ui->splitter->setStretchFactor(0, 3);
    ui->splitter->setStretchFactor(1, 2);

    // ── Connections ──
    connect(ui->templateTable, &QTableWidget::itemSelectionChanged,
            this, &TemplateManagerDialog::onSelectionChanged);
    connect(ui->templateTable, &QTableWidget::itemDoubleClicked,
            this, &TemplateManagerDialog::onDoubleClicked);
    connect(ui->searchEdit, &QLineEdit::textChanged,
            this, &TemplateManagerDialog::onSearchChanged);

    connect(ui->createBtn,  &QPushButton::clicked, this, &TemplateManagerDialog::onCreateClicked);
    connect(ui->editBtn,    &QPushButton::clicked, this, &TemplateManagerDialog::onEditClicked);
    connect(ui->cloneBtn,   &QPushButton::clicked, this, &TemplateManagerDialog::onCloneClicked);
    connect(ui->deleteBtn,  &QPushButton::clicked, this, &TemplateManagerDialog::onDeleteClicked);
    connect(ui->deployBtn,  &QPushButton::clicked, this, &TemplateManagerDialog::onDeployClicked);
    connect(ui->closeBtn,   &QPushButton::clicked, this, &QDialog::reject);
}

// ═════════════════════════════════════════════════════════════════════════════
// Data Loading
// ═════════════════════════════════════════════════════════════════════════════

void TemplateManagerDialog::loadTemplates()
{
    StrategyTemplateRepository &repo = StrategyTemplateRepository::instance();
    if (!repo.isOpen()) {
        m_allTemplates.clear();
        ui->templateTable->setRowCount(0);
        return;
    }

    m_allTemplates = repo.loadAllTemplates();
    ui->templateTable->setSortingEnabled(false);
    ui->templateTable->setRowCount(0);

    for (int i = 0; i < m_allTemplates.size(); ++i) {
        const StrategyTemplate &t = m_allTemplates[i];
        ui->templateTable->insertRow(i);

        auto *nameItem = new QTableWidgetItem(t.name);
        nameItem->setToolTip(t.description);
        // Store the template index as data for lookup after sorting
        nameItem->setData(Qt::UserRole, i);
        ui->templateTable->setItem(i, 0, nameItem);

        QString modeStr;
        switch (t.mode) {
        case StrategyMode::IndicatorBased: modeStr = "Indicator"; break;
        case StrategyMode::OptionMultiLeg: modeStr = "Options";   break;
        case StrategyMode::Spread:         modeStr = "Spread";    break;
        }
        ui->templateTable->setItem(i, 1, new QTableWidgetItem(modeStr));
        ui->templateTable->setItem(i, 2, new QTableWidgetItem(
            QString::number(t.symbols.size())));
        ui->templateTable->setItem(i, 3, new QTableWidgetItem(
            QString::number(t.params.size())));

        QString updatedStr = t.updatedAt.isValid()
            ? t.updatedAt.toString("yyyy-MM-dd HH:mm")
            : (t.createdAt.isValid() ? t.createdAt.toString("yyyy-MM-dd HH:mm") : "—");
        ui->templateTable->setItem(i, 4, new QTableWidgetItem(updatedStr));
    }

    ui->templateTable->setSortingEnabled(true);

    // Update count label
    ui->countLabel->setText(QString("%1 template(s)").arg(m_allTemplates.size()));

    if (!m_allTemplates.isEmpty())
        ui->templateTable->selectRow(0);
}

// ═════════════════════════════════════════════════════════════════════════════
// Selection / Detail
// ═════════════════════════════════════════════════════════════════════════════

int TemplateManagerDialog::selectedRow() const
{
    int row = ui->templateTable->currentRow();
    if (row < 0 || row >= ui->templateTable->rowCount())
        return -1;

    // Resolve the original index (table may be sorted)
    auto *item = ui->templateTable->item(row, 0);
    if (!item) return -1;

    int origIdx = item->data(Qt::UserRole).toInt();
    if (origIdx < 0 || origIdx >= m_allTemplates.size())
        return -1;
    return origIdx;
}

void TemplateManagerDialog::onSelectionChanged()
{
    updateDetailPane();
    updateButtonStates();
}

void TemplateManagerDialog::onDoubleClicked()
{
    // Double-click → edit
    onEditClicked();
}

void TemplateManagerDialog::updateButtonStates()
{
    bool hasSel = selectedRow() >= 0;
    ui->editBtn->setEnabled(hasSel);
    ui->cloneBtn->setEnabled(hasSel);
    ui->deleteBtn->setEnabled(hasSel);
    ui->deployBtn->setEnabled(hasSel);
}

void TemplateManagerDialog::updateDetailPane()
{
    int idx = selectedRow();
    if (idx < 0) {
        ui->detailTitle->setText("Select a template");
        ui->detailMeta->setText("—");
        ui->detailDesc->clear();
        ui->detailSymbols->setText("—");
        ui->detailIndicators->setText("—");
        ui->detailParams->setText("—");
        ui->detailRisk->setText("—");
        return;
    }

    const StrategyTemplate &t = m_allTemplates[idx];
    m_selectedTemplate = t;

    // Title
    ui->detailTitle->setText(t.name);

    // Meta
    QStringList meta;
    meta << QString("<b>ID:</b> %1").arg(t.templateId.left(8) + "…");
    meta << QString("<b>Version:</b> %1").arg(t.version);
    QString modeStr;
    switch (t.mode) {
    case StrategyMode::IndicatorBased: modeStr = "Indicator-Based"; break;
    case StrategyMode::OptionMultiLeg: modeStr = "Options Multi-Leg"; break;
    case StrategyMode::Spread:         modeStr = "Spread"; break;
    }
    meta << QString("<b>Mode:</b> %1").arg(modeStr);
    if (t.createdAt.isValid())
        meta << QString("<b>Created:</b> %1").arg(t.createdAt.toString("yyyy-MM-dd HH:mm"));
    if (t.updatedAt.isValid())
        meta << QString("<b>Updated:</b> %1").arg(t.updatedAt.toString("yyyy-MM-dd HH:mm"));
    ui->detailMeta->setText(meta.join("  ·  "));

    // Description
    ui->detailDesc->setPlainText(t.description.isEmpty() ? "(no description)" : t.description);

    // Symbols
    QStringList symLines;
    for (const auto &s : t.symbols) {
        QString roleStr = (s.role == SymbolRole::Reference)
            ? "<span style='color:#2563eb;'>REF</span>"
            : "<span style='color:#16a34a;'>TRADE</span>";
        symLines << QString("• [%1] <b>%2</b> — %3")
            .arg(roleStr, s.label, s.id);
    }
    ui->detailSymbols->setText(symLines.isEmpty() ? "(none)" : symLines.join("<br>"));

    // Indicators
    QStringList indLines;
    for (const auto &ind : t.indicators) {
        QString line = QString("• <b>%1</b> (%2) on %3").arg(ind.id, ind.type, ind.symbolId);
        if (!ind.periodParam.isEmpty()) line += QString(" — P1: %1").arg(ind.periodParam);
        if (!ind.period2Param.isEmpty()) line += QString(", P2: %1").arg(ind.period2Param);
        if (!ind.timeframe.isEmpty()) line += QString("  [TF: %1]").arg(ind.timeframe);
        indLines << line;
    }
    ui->detailIndicators->setText(indLines.isEmpty() ? "(none)" : indLines.join("<br>"));

    // Parameters
    QStringList paramLines;
    for (const auto &p : t.params) {
        QString typeName;
        switch (p.valueType) {
        case ParamValueType::Int:        typeName = "Int";        break;
        case ParamValueType::Double:     typeName = "Double";     break;
        case ParamValueType::Bool:       typeName = "Bool";       break;
        case ParamValueType::String:     typeName = "String";     break;
        case ParamValueType::Expression: typeName = "Expression"; break;
        }
        QString valueStr = p.isExpression()
            ? QString("ƒ(%1)").arg(p.expression)
            : p.defaultValue.toString();
        QString lockStr = p.locked ? " 🔒" : "";
        paramLines << QString("• <b>%1</b> (%2) = %3%4%5")
            .arg(p.label.isEmpty() ? p.name : p.label)
            .arg(typeName)
            .arg(valueStr)
            .arg(lockStr)
            .arg(p.description.isEmpty() ? "" : QString(" — <i>%1</i>").arg(p.description));
    }
    ui->detailParams->setText(paramLines.isEmpty() ? "(none)" : paramLines.join("<br>"));

    // Risk
    const auto &r = t.riskDefaults;
    QStringList riskLines;
    riskLines << QString("SL: %1%%2").arg(r.stopLossPercent, 0, 'f', 2)
                     .arg(r.stopLossLocked ? " 🔒" : "");
    riskLines << QString("Target: %1%%2").arg(r.targetPercent, 0, 'f', 2)
                     .arg(r.targetLocked ? " 🔒" : "");
    if (r.trailingEnabled)
        riskLines << QString("Trailing: trigger %1%, trail %2%").arg(r.trailingTriggerPct, 0, 'f', 2)
                         .arg(r.trailingAmountPct, 0, 'f', 2);
    if (r.timeExitEnabled)
        riskLines << QString("Time exit: %1").arg(r.exitTime);
    riskLines << QString("Max trades/day: %1").arg(r.maxDailyTrades);
    riskLines << QString("Max daily loss: ₹%1").arg(r.maxDailyLossRs, 0, 'f', 0);
    ui->detailRisk->setText(riskLines.join("  ·  "));
}

// ═════════════════════════════════════════════════════════════════════════════
// Search / Filter
// ═════════════════════════════════════════════════════════════════════════════

void TemplateManagerDialog::onSearchChanged(const QString &text)
{
    filterTable(text);
}

void TemplateManagerDialog::filterTable(const QString &text)
{
    QString filter = text.trimmed().toLower();
    for (int row = 0; row < ui->templateTable->rowCount(); ++row) {
        auto *item = ui->templateTable->item(row, 0);
        if (!item) continue;
        bool match = filter.isEmpty() || item->text().toLower().contains(filter);
        ui->templateTable->setRowHidden(row, !match);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Actions
// ═════════════════════════════════════════════════════════════════════════════

void TemplateManagerDialog::onCreateClicked()
{
    StrategyTemplateBuilderDialog dlg(this);

    if (dlg.exec() != QDialog::Accepted)
        return;

    StrategyTemplate tmpl = dlg.buildTemplate();

    StrategyTemplateRepository &repo = StrategyTemplateRepository::instance();
    if (!repo.isOpen()) {
        QMessageBox::critical(this, "Save Failed",
            "Could not open the strategy template database.");
        return;
    }

    if (!repo.saveTemplate(tmpl)) {
        QMessageBox::critical(this, "Save Failed",
            "Template could not be saved to the database.");
        return;
    }

    QMessageBox::information(this, "Template Created",
        QString("Strategy template <b>%1</b> created successfully.")
            .arg(tmpl.name.toHtmlEscaped()));

    loadTemplates();
}

void TemplateManagerDialog::onEditClicked()
{
    int idx = selectedRow();
    if (idx < 0) return;

    StrategyTemplate tmpl = m_allTemplates[idx];

    StrategyTemplateBuilderDialog dlg(this);
    dlg.setWindowTitle(QString("Edit Template: %1").arg(tmpl.name));
    dlg.setTemplate(tmpl);

    if (dlg.exec() != QDialog::Accepted)
        return;

    StrategyTemplate updated = dlg.buildTemplate();

    // Preserve the original template ID so saveTemplate does an UPDATE
    updated.templateId = tmpl.templateId;
    updated.createdAt  = tmpl.createdAt;
    // Bump version
    bool ok;
    double ver = tmpl.version.toDouble(&ok);
    if (ok)
        updated.version = QString::number(ver + 0.1, 'f', 1);

    StrategyTemplateRepository &repo = StrategyTemplateRepository::instance();
    if (!repo.isOpen()) {
        QMessageBox::critical(this, "Save Failed",
            "Could not open the strategy template database.");
        return;
    }

    if (!repo.saveTemplate(updated)) {
        QMessageBox::critical(this, "Save Failed",
            "Template could not be updated in the database.");
        return;
    }

    QMessageBox::information(this, "Template Updated",
        QString("Template <b>%1</b> updated to version <b>%2</b>.")
            .arg(updated.name.toHtmlEscaped(), updated.version));

    loadTemplates();

    // Re-select the edited template
    for (int r = 0; r < ui->templateTable->rowCount(); ++r) {
        auto *item = ui->templateTable->item(r, 0);
        if (item && item->data(Qt::UserRole).toInt() < m_allTemplates.size()) {
            int origIdx = item->data(Qt::UserRole).toInt();
            if (m_allTemplates[origIdx].templateId == updated.templateId) {
                ui->templateTable->selectRow(r);
                break;
            }
        }
    }
}

void TemplateManagerDialog::onCloneClicked()
{
    int idx = selectedRow();
    if (idx < 0) return;

    StrategyTemplate original = m_allTemplates[idx];

    // Create a copy with a new ID
    StrategyTemplate clone = original;
    clone.templateId.clear(); // will be assigned a new UUID on save
    clone.name = original.name + " (Copy)";
    clone.version = "1.0";
    clone.createdAt = QDateTime();
    clone.updatedAt = QDateTime();

    // Let user edit the clone before saving
    StrategyTemplateBuilderDialog dlg(this);
    dlg.setWindowTitle(QString("Clone Template: %1").arg(original.name));
    dlg.setTemplate(clone);

    if (dlg.exec() != QDialog::Accepted)
        return;

    StrategyTemplate result = dlg.buildTemplate();
    // Ensure it gets a new ID (not the original's)
    result.templateId.clear();

    StrategyTemplateRepository &repo = StrategyTemplateRepository::instance();
    if (!repo.isOpen()) {
        QMessageBox::critical(this, "Save Failed",
            "Could not open the strategy template database.");
        return;
    }

    if (!repo.saveTemplate(result)) {
        QMessageBox::critical(this, "Save Failed",
            "Cloned template could not be saved.");
        return;
    }

    QMessageBox::information(this, "Template Cloned",
        QString("Template <b>%1</b> cloned as <b>%2</b>.")
            .arg(original.name.toHtmlEscaped(), result.name.toHtmlEscaped()));

    loadTemplates();
}

void TemplateManagerDialog::onDeleteClicked()
{
    int idx = selectedRow();
    if (idx < 0) return;

    const StrategyTemplate &t = m_allTemplates[idx];

    auto reply = QMessageBox::question(this, "Delete Template",
        QString("Are you sure you want to delete template <b>%1</b>?<br><br>"
                "This action is reversible (soft-delete).")
            .arg(t.name.toHtmlEscaped()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    StrategyTemplateRepository &repo = StrategyTemplateRepository::instance();
    if (!repo.isOpen()) {
        QMessageBox::critical(this, "Delete Failed",
            "Could not open the strategy template database.");
        return;
    }

    if (!repo.deleteTemplate(t.templateId)) {
        QMessageBox::critical(this, "Delete Failed",
            "Template could not be deleted from the database.");
        return;
    }

    loadTemplates();
}

void TemplateManagerDialog::onDeployClicked()
{
    int idx = selectedRow();
    if (idx < 0) return;

    m_selectedTemplate = m_allTemplates[idx];
    m_resultAction = Action::Deploy;
    accept();
}

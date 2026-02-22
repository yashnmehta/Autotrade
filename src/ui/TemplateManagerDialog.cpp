/**
 * @file TemplateManagerDialog.cpp
 * @brief Unified Template Management Dialog implementation
 *
 * Provides a single place to browse, create, edit, clone, delete,
 * and deploy strategy templates.
 */

#include "ui/TemplateManagerDialog.h"
#include "services/StrategyTemplateRepository.h"
#include "ui/StrategyDeployDialog.h"
#include "ui/StrategyTemplateBuilderDialog.h"

#include <QDateTime>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

// ═════════════════════════════════════════════════════════════════════════════
// Construction
// ═════════════════════════════════════════════════════════════════════════════

TemplateManagerDialog::TemplateManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Strategy Template Manager");
    setMinimumSize(1000, 640);
    resize(1100, 720);
    setupUI();
    loadTemplates();
    updateButtonStates();
}

// ═════════════════════════════════════════════════════════════════════════════
// UI Setup
// ═════════════════════════════════════════════════════════════════════════════

void TemplateManagerDialog::setupUI()
{
    // ── Global light stylesheet ──
    setStyleSheet(R"(
        QDialog, QWidget {
            background: #ffffff;
            color: #1e293b;
            font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
        }
        QSplitter::handle { background: #e2e8f0; width: 2px; }

        QTableWidget {
            background: #ffffff;
            border: 1px solid #e2e8f0;
            gridline-color: #f1f5f9;
            color: #1e293b;
            font-size: 12px;
            selection-background-color: #dbeafe;
            selection-color: #1e40af;
            alternate-background-color: #f8fafc;
        }
        QTableWidget::item { padding: 4px 6px; }
        QTableWidget::item:hover { background: #eff6ff; }
        QHeaderView::section {
            background: #f8fafc;
            color: #475569;
            padding: 6px 10px;
            border: none;
            border-bottom: 2px solid #e2e8f0;
            font-weight: 700;
            font-size: 10px;
            text-transform: uppercase;
        }

        QLineEdit {
            background: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            color: #0f172a;
            padding: 6px 10px;
            font-size: 12px;
        }
        QLineEdit:focus { border-color: #3b82f6; background: #ffffff; }

        QLabel { color: #475569; }

        QTextEdit {
            background: #f8fafc;
            border: 1px solid #e2e8f0;
            border-radius: 4px;
            color: #334155;
            font-size: 12px;
            padding: 4px;
        }

        QPushButton {
            background: #f1f5f9;
            color: #334155;
            border: 1px solid #cbd5e1;
            border-radius: 5px;
            padding: 7px 18px;
            font-size: 12px;
            font-weight: 600;
        }
        QPushButton:hover { background: #e2e8f0; color: #0f172a; border-color: #94a3b8; }
        QPushButton:pressed { background: #dbeafe; border-color: #3b82f6; }
        QPushButton:disabled { background: #f8fafc; color: #94a3b8; border-color: #e2e8f0; }

        QGroupBox {
            background: #f8fafc;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            margin-top: 14px;
            padding: 12px 10px 10px 10px;
            color: #2563eb;
            font-weight: 700;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px; top: 0;
            padding: 0 6px;
            color: #2563eb;
            background: #f8fafc;
        }

        QScrollArea { border: none; background: transparent; }
        QScrollBar:vertical { background: #f1f5f9; width: 8px; border-radius: 4px; }
        QScrollBar::handle:vertical { background: #cbd5e1; border-radius: 4px; min-height: 24px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Title bar ──
    auto *titleBar = new QWidget(this);
    titleBar->setStyleSheet(
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #1e40af,stop:1 #2563eb); padding:0;");
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(20, 12, 20, 12);

    auto *titleLbl = new QLabel("📋  Strategy Template Manager", titleBar);
    titleLbl->setStyleSheet("color:#ffffff; font-size:16px; font-weight:700; background:transparent;");
    titleLayout->addWidget(titleLbl);
    titleLayout->addStretch();

    // Search box in title bar
    m_searchEdit = new QLineEdit(titleBar);
    m_searchEdit->setPlaceholderText("🔍 Search templates…");
    m_searchEdit->setFixedWidth(260);
    m_searchEdit->setClearButtonEnabled(true);
    titleLayout->addWidget(m_searchEdit);

    mainLayout->addWidget(titleBar);

    // ── Body: splitter [table | detail pane] ──
    auto *bodyWidget = new QWidget(this);
    auto *bodyLayout = new QHBoxLayout(bodyWidget);
    bodyLayout->setContentsMargins(12, 12, 12, 12);
    bodyLayout->setSpacing(12);

    auto *splitter = new QSplitter(Qt::Horizontal, bodyWidget);

    // ─── Left: Template table ───
    auto *leftWidget = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    auto *listHeader = new QLabel("<b>Templates</b>", leftWidget);
    listHeader->setStyleSheet("color:#1e293b; font-size:13px; background:transparent;");
    leftLayout->addWidget(listHeader);

    m_templateTable = new QTableWidget(0, 5, leftWidget);
    m_templateTable->setHorizontalHeaderLabels({"Name", "Mode", "Symbols", "Params", "Updated"});
    m_templateTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_templateTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_templateTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_templateTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_templateTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_templateTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_templateTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_templateTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_templateTable->setAlternatingRowColors(true);
    m_templateTable->setShowGrid(false);
    m_templateTable->verticalHeader()->setVisible(false);
    m_templateTable->setSortingEnabled(true);
    leftLayout->addWidget(m_templateTable, 1);

    // Action buttons row below the table
    auto *actionRow = new QHBoxLayout;
    actionRow->setSpacing(6);

    m_createBtn = new QPushButton("＋ New Template", leftWidget);
    m_createBtn->setStyleSheet(
        "QPushButton { background:#eff6ff; color:#1d4ed8; border:1px solid #bfdbfe; }"
        "QPushButton:hover { background:#dbeafe; color:#1e40af; border-color:#93c5fd; }");
    m_editBtn   = new QPushButton("✏️ Edit", leftWidget);
    m_cloneBtn  = new QPushButton("📋 Clone", leftWidget);
    m_deleteBtn = new QPushButton("🗑 Delete", leftWidget);
    m_deleteBtn->setStyleSheet(
        "QPushButton { background:#fef2f2; color:#dc2626; border:1px solid #fecaca; }"
        "QPushButton:hover { background:#fee2e2; color:#b91c1c; border-color:#fca5a5; }");

    actionRow->addWidget(m_createBtn);
    actionRow->addWidget(m_editBtn);
    actionRow->addWidget(m_cloneBtn);
    actionRow->addWidget(m_deleteBtn);
    actionRow->addStretch();
    leftLayout->addLayout(actionRow);

    splitter->addWidget(leftWidget);

    // ─── Right: Detail pane (scrollable) ───
    auto *rightWidget = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    m_detailTitle = new QLabel("Select a template", rightWidget);
    m_detailTitle->setStyleSheet("color:#1e293b; font-size:14px; font-weight:700; background:transparent;");
    rightLayout->addWidget(m_detailTitle);

    auto *detailScroll = new QScrollArea(rightWidget);
    detailScroll->setWidgetResizable(true);
    auto *detailContent = new QWidget(detailScroll);
    detailScroll->setWidget(detailContent);
    auto *detailLay = new QVBoxLayout(detailContent);
    detailLay->setContentsMargins(0, 4, 0, 4);
    detailLay->setSpacing(10);

    // Meta info
    m_detailMeta = new QLabel("—", detailContent);
    m_detailMeta->setWordWrap(true);
    m_detailMeta->setStyleSheet("color:#64748b; font-size:11px; background:transparent;");
    detailLay->addWidget(m_detailMeta);

    // Description
    auto *descGroup = new QGroupBox("Description", detailContent);
    auto *descLay = new QVBoxLayout(descGroup);
    m_detailDesc = new QTextEdit(descGroup);
    m_detailDesc->setReadOnly(true);
    m_detailDesc->setMaximumHeight(80);
    descLay->addWidget(m_detailDesc);
    detailLay->addWidget(descGroup);

    // Symbols
    auto *symGroup = new QGroupBox("Symbol Slots", detailContent);
    auto *symLay = new QVBoxLayout(symGroup);
    m_detailSymbols = new QLabel("—", symGroup);
    m_detailSymbols->setWordWrap(true);
    m_detailSymbols->setStyleSheet("color:#334155; font-size:11px; background:transparent;");
    symLay->addWidget(m_detailSymbols);
    detailLay->addWidget(symGroup);

    // Indicators
    auto *indGroup = new QGroupBox("Indicators", detailContent);
    auto *indLay = new QVBoxLayout(indGroup);
    m_detailIndicators = new QLabel("—", indGroup);
    m_detailIndicators->setWordWrap(true);
    m_detailIndicators->setStyleSheet("color:#334155; font-size:11px; background:transparent;");
    indLay->addWidget(m_detailIndicators);
    detailLay->addWidget(indGroup);

    // Parameters
    auto *parGroup = new QGroupBox("Parameters", detailContent);
    auto *parLay = new QVBoxLayout(parGroup);
    m_detailParams = new QLabel("—", parGroup);
    m_detailParams->setWordWrap(true);
    m_detailParams->setStyleSheet("color:#334155; font-size:11px; background:transparent;");
    parLay->addWidget(m_detailParams);
    detailLay->addWidget(parGroup);

    // Risk
    auto *riskGroup = new QGroupBox("Risk Defaults", detailContent);
    auto *riskLay = new QVBoxLayout(riskGroup);
    m_detailRisk = new QLabel("—", riskGroup);
    m_detailRisk->setWordWrap(true);
    m_detailRisk->setStyleSheet("color:#334155; font-size:11px; background:transparent;");
    riskLay->addWidget(m_detailRisk);
    detailLay->addWidget(riskGroup);

    detailLay->addStretch();

    rightLayout->addWidget(detailScroll, 1);

    // Deploy button at the bottom of the detail pane
    m_deployBtn = new QPushButton("🚀  Deploy This Template", rightWidget);
    m_deployBtn->setMinimumHeight(40);
    m_deployBtn->setStyleSheet(
        "QPushButton { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "              stop:0 #16a34a,stop:1 #15803d);"
        "              color:white; font-weight:700; font-size:13px;"
        "              border-radius:6px; border:none; padding:8px 24px; }"
        "QPushButton:hover { background:#22c55e; }"
        "QPushButton:disabled { background:#f0fdf4; color:#86efac; border:1px solid #bbf7d0; }");
    rightLayout->addWidget(m_deployBtn);

    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    bodyLayout->addWidget(splitter);
    mainLayout->addWidget(bodyWidget, 1);

    // ── Bottom bar ──
    auto *bottomBar = new QWidget(this);
    bottomBar->setStyleSheet("background:#f8fafc; border-top:2px solid #e2e8f0;");
    auto *bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(16, 8, 16, 8);

    auto *countLbl = new QLabel("", bottomBar);
    countLbl->setObjectName("countLabel");
    countLbl->setStyleSheet("color:#64748b; font-size:11px; background:transparent;");
    bottomLayout->addWidget(countLbl);
    bottomLayout->addStretch();

    m_closeBtn = new QPushButton("Close", bottomBar);
    bottomLayout->addWidget(m_closeBtn);

    mainLayout->addWidget(bottomBar);

    // ── Connections ──
    connect(m_templateTable, &QTableWidget::itemSelectionChanged,
            this, &TemplateManagerDialog::onSelectionChanged);
    connect(m_templateTable, &QTableWidget::itemDoubleClicked,
            this, &TemplateManagerDialog::onDoubleClicked);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &TemplateManagerDialog::onSearchChanged);

    connect(m_createBtn,  &QPushButton::clicked, this, &TemplateManagerDialog::onCreateClicked);
    connect(m_editBtn,    &QPushButton::clicked, this, &TemplateManagerDialog::onEditClicked);
    connect(m_cloneBtn,   &QPushButton::clicked, this, &TemplateManagerDialog::onCloneClicked);
    connect(m_deleteBtn,  &QPushButton::clicked, this, &TemplateManagerDialog::onDeleteClicked);
    connect(m_deployBtn,  &QPushButton::clicked, this, &TemplateManagerDialog::onDeployClicked);
    connect(m_closeBtn,   &QPushButton::clicked, this, &QDialog::reject);
}

// ═════════════════════════════════════════════════════════════════════════════
// Data Loading
// ═════════════════════════════════════════════════════════════════════════════

void TemplateManagerDialog::loadTemplates()
{
    StrategyTemplateRepository &repo = StrategyTemplateRepository::instance();
    if (!repo.isOpen()) {
        m_allTemplates.clear();
        m_templateTable->setRowCount(0);
        return;
    }

    m_allTemplates = repo.loadAllTemplates();
    m_templateTable->setSortingEnabled(false);
    m_templateTable->setRowCount(0);

    for (int i = 0; i < m_allTemplates.size(); ++i) {
        const StrategyTemplate &t = m_allTemplates[i];
        m_templateTable->insertRow(i);

        auto *nameItem = new QTableWidgetItem(t.name);
        nameItem->setToolTip(t.description);
        // Store the template index as data for lookup after sorting
        nameItem->setData(Qt::UserRole, i);
        m_templateTable->setItem(i, 0, nameItem);

        QString modeStr;
        switch (t.mode) {
        case StrategyMode::IndicatorBased: modeStr = "Indicator"; break;
        case StrategyMode::OptionMultiLeg: modeStr = "Options";   break;
        case StrategyMode::Spread:         modeStr = "Spread";    break;
        }
        m_templateTable->setItem(i, 1, new QTableWidgetItem(modeStr));
        m_templateTable->setItem(i, 2, new QTableWidgetItem(
            QString::number(t.symbols.size())));
        m_templateTable->setItem(i, 3, new QTableWidgetItem(
            QString::number(t.params.size())));

        QString updatedStr = t.updatedAt.isValid()
            ? t.updatedAt.toString("yyyy-MM-dd HH:mm")
            : (t.createdAt.isValid() ? t.createdAt.toString("yyyy-MM-dd HH:mm") : "—");
        m_templateTable->setItem(i, 4, new QTableWidgetItem(updatedStr));
    }

    m_templateTable->setSortingEnabled(true);

    // Update count label
    auto *countLbl = findChild<QLabel*>("countLabel");
    if (countLbl)
        countLbl->setText(QString("%1 template(s)").arg(m_allTemplates.size()));

    if (!m_allTemplates.isEmpty())
        m_templateTable->selectRow(0);
}

// ═════════════════════════════════════════════════════════════════════════════
// Selection / Detail
// ═════════════════════════════════════════════════════════════════════════════

int TemplateManagerDialog::selectedRow() const
{
    int row = m_templateTable->currentRow();
    if (row < 0 || row >= m_templateTable->rowCount())
        return -1;

    // Resolve the original index (table may be sorted)
    auto *item = m_templateTable->item(row, 0);
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
    m_editBtn->setEnabled(hasSel);
    m_cloneBtn->setEnabled(hasSel);
    m_deleteBtn->setEnabled(hasSel);
    m_deployBtn->setEnabled(hasSel);
}

void TemplateManagerDialog::updateDetailPane()
{
    int idx = selectedRow();
    if (idx < 0) {
        m_detailTitle->setText("Select a template");
        m_detailMeta->setText("—");
        m_detailDesc->clear();
        m_detailSymbols->setText("—");
        m_detailIndicators->setText("—");
        m_detailParams->setText("—");
        m_detailRisk->setText("—");
        return;
    }

    const StrategyTemplate &t = m_allTemplates[idx];
    m_selectedTemplate = t;

    // Title
    m_detailTitle->setText(t.name);

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
    m_detailMeta->setText(meta.join("  ·  "));

    // Description
    m_detailDesc->setPlainText(t.description.isEmpty() ? "(no description)" : t.description);

    // Symbols
    QStringList symLines;
    for (const auto &s : t.symbols) {
        QString roleStr = (s.role == SymbolRole::Reference)
            ? "<span style='color:#2563eb;'>REF</span>"
            : "<span style='color:#16a34a;'>TRADE</span>";
        symLines << QString("• [%1] <b>%2</b> — %3")
            .arg(roleStr, s.label, s.id);
    }
    m_detailSymbols->setText(symLines.isEmpty() ? "(none)" : symLines.join("<br>"));

    // Indicators
    QStringList indLines;
    for (const auto &ind : t.indicators) {
        QString line = QString("• <b>%1</b> (%2) on %3").arg(ind.id, ind.type, ind.symbolId);
        if (!ind.periodParam.isEmpty()) line += QString(" — P1: %1").arg(ind.periodParam);
        if (!ind.period2Param.isEmpty()) line += QString(", P2: %1").arg(ind.period2Param);
        if (!ind.timeframe.isEmpty()) line += QString("  [TF: %1]").arg(ind.timeframe);
        indLines << line;
    }
    m_detailIndicators->setText(indLines.isEmpty() ? "(none)" : indLines.join("<br>"));

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
    m_detailParams->setText(paramLines.isEmpty() ? "(none)" : paramLines.join("<br>"));

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
    m_detailRisk->setText(riskLines.join("  ·  "));
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
    for (int row = 0; row < m_templateTable->rowCount(); ++row) {
        auto *item = m_templateTable->item(row, 0);
        if (!item) continue;
        bool match = filter.isEmpty() || item->text().toLower().contains(filter);
        m_templateTable->setRowHidden(row, !match);
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
    for (int r = 0; r < m_templateTable->rowCount(); ++r) {
        auto *item = m_templateTable->item(r, 0);
        if (item && item->data(Qt::UserRole).toInt() < m_allTemplates.size()) {
            int origIdx = item->data(Qt::UserRole).toInt();
            if (m_allTemplates[origIdx].templateId == updated.templateId) {
                m_templateTable->selectRow(r);
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

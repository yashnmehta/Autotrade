/**
 * @file StrategyDeployDialog.cpp
 * @brief Phase 3 – Template Deploy Dialog implementation
 */

#include "strategy/builder/StrategyDeployDialog.h"
#include "repository/RepositoryManager.h"
#include "strategy/persistence/StrategyTemplateRepository.h"
#include "ui/GlobalSearchWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

// ═════════════════════════════════════════════════════════════════════════════
// SymbolBindingRow
// ═════════════════════════════════════════════════════════════════════════════

SymbolBindingRow::SymbolBindingRow(const SymbolDefinition &def, QWidget *parent)
    : QWidget(parent), m_def(def) {
  m_binding.symbolId = def.id;
  setupUI(def);
}

void SymbolBindingRow::setupUI(const SymbolDefinition &def) {
  // Card-style outer frame
  setStyleSheet(
      "SymbolBindingRow { background:#ffffff; border:1px solid #e2e8f0; "
      "border-radius:6px; }"
      "QLabel { color:#475569; font-size:12px; background:transparent; }"
      "QLabel#tokenLabel { color:#2563eb; font-size:11px; "
      "font-family:monospace; }"
      "QLabel#resolvedLabel { color:#16a34a; font-size:11px; font-weight:600; }"
      "QLineEdit { background:#f8fafc; border:1px solid #cbd5e1; "
      "border-radius:4px;"
      "            color:#0f172a; padding:5px 8px; font-size:12px; }"
      "QLineEdit:focus { border-color:#3b82f6; background:#ffffff; }"
      "QPushButton { background:#f1f5f9; color:#475569; border:1px solid #cbd5e1;"
      "              border-radius:4px; padding:5px 12px; font-size:12px; }"
      "QPushButton:hover { background:#e2e8f0; color:#0f172a; }"
      "QPushButton#clearBtn { background:#fef2f2; color:#dc2626; "
      "border-color:#fecaca; }"
      "QPushButton#clearBtn:hover { background:#fee2e2; }"
      "QSpinBox { background:#f8fafc; border:1px solid #cbd5e1; "
      "border-radius:4px;"
      "           color:#0f172a; padding:4px 6px; font-size:12px; }");

  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(10, 8, 10, 8);
  outer->setSpacing(6);

  // ── Top row: badge / label / search edit / buttons ──
  auto *topRow = new QHBoxLayout;
  topRow->setSpacing(8);

  // Role badge
  bool isRef = (def.role == SymbolRole::Reference);
  QString roleText = isRef ? "REF" : "TRADE";
  QString roleColor = isRef ? "#2563eb" : "#16a34a";
  auto *roleLbl = new QLabel(roleText, this);
  roleLbl->setFixedSize(52, 24);
  roleLbl->setAlignment(Qt::AlignCenter);
  roleLbl->setStyleSheet(
      QString("background:%1; color:white; border-radius:3px; font-weight:700;"
              "font-size:11px; padding:2px 4px;")
          .arg(roleColor));

  // Slot label
  auto *slotLbl = new QLabel(def.label, this);
  slotLbl->setFixedWidth(150);
  slotLbl->setStyleSheet("color:#1e293b; font-size:12px; font-weight:600; "
                         "background:transparent;");

  // Inline search edit (type here to search)
  m_nameEdit = new QLineEdit(this);
  m_nameEdit->setPlaceholderText("Type to search instrument (min. 2 chars)…");
  m_nameEdit->setMinimumWidth(220);
  m_nameEdit->setToolTip(
      "Type a symbol name and press Enter or click 🔍 Search");

  m_searchBtn = new QPushButton("🔍 Search", this);
  m_searchBtn->setFixedWidth(95);
  m_searchBtn->setToolTip("Open full search dialog");

  m_clearBtn = new QPushButton("✕", this);
  m_clearBtn->setObjectName("clearBtn");
  m_clearBtn->setFixedWidth(28);
  m_clearBtn->setEnabled(false);
  m_clearBtn->setToolTip("Clear selection");

  // Qty spin
  m_qtySpinBox = new QSpinBox(this);
  m_qtySpinBox->setRange(1, 9999);
  m_qtySpinBox->setValue(1);
  m_qtySpinBox->setPrefix("Qty: ");
  m_qtySpinBox->setFixedWidth(110);

  topRow->addWidget(roleLbl);
  topRow->addWidget(slotLbl);
  topRow->addWidget(m_nameEdit, 1);
  topRow->addWidget(m_searchBtn);
  topRow->addWidget(m_clearBtn);
  topRow->addWidget(m_qtySpinBox);
  outer->addLayout(topRow);

  // ── Bottom row: resolved token info ──
  m_tokenLabel = new QLabel("—", this);
  m_tokenLabel->setObjectName("tokenLabel");
  outer->addWidget(m_tokenLabel);

  // ── Inline search results popup (QTableWidget hidden until typing) ──
  m_inlineResults = new QTableWidget(0, 5, this);
  m_inlineResults->setObjectName("inlineResults");
  m_inlineResults->setHorizontalHeaderLabels(
      {"Name", "Exchange", "Expiry", "Strike", "Type"});
  m_inlineResults->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::Stretch);
  m_inlineResults->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  m_inlineResults->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);
  m_inlineResults->horizontalHeader()->setSectionResizeMode(
      3, QHeaderView::ResizeToContents);
  m_inlineResults->horizontalHeader()->setSectionResizeMode(
      4, QHeaderView::ResizeToContents);
  m_inlineResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_inlineResults->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_inlineResults->setSelectionMode(QAbstractItemView::SingleSelection);
  m_inlineResults->verticalHeader()->setVisible(false);
  m_inlineResults->setMaximumHeight(160);
  m_inlineResults->setStyleSheet(
      "QTableWidget { background:#ffffff; border:1px solid #e2e8f0; "
      "border-radius:4px;"
      "               color:#0f172a; font-size:11px; gridline-color:#f1f5f9; }"
      "QTableWidget::item:selected { background:#dbeafe; color:#1e40af; }"
      "QTableWidget::item:hover { background:#f8fafc; }"
      "QHeaderView::section { background:#f8fafc; color:#475569; padding:3px "
      "6px;"
      "                       border:none; border-bottom:2px solid #e2e8f0; "
      "font-size:11px; font-weight:600; }");
  m_inlineResults->hide();
  outer->addWidget(m_inlineResults);

  // ── Connections ──
  connect(m_nameEdit, &QLineEdit::textChanged, this,
          &SymbolBindingRow::onInlineSearch);
  connect(m_nameEdit, &QLineEdit::returnPressed, this,
          &SymbolBindingRow::onInlineEnter);
  connect(m_searchBtn, &QPushButton::clicked, this,
          &SymbolBindingRow::onSearchClicked);
  connect(m_clearBtn, &QPushButton::clicked, this,
          &SymbolBindingRow::onClearClicked);
  connect(m_inlineResults, &QTableWidget::cellDoubleClicked, this,
          [this](int row, int) { pickInlineRow(row); });
  // Also allow Enter key in the results table
  connect(m_inlineResults, &QTableWidget::itemSelectionChanged, this, [this]() {
    int row = m_inlineResults->currentRow();
    if (row >= 0 && row < m_inlineContracts.size())
      m_inlineResults->setToolTip(m_inlineContracts[row].name);
  });
}

void SymbolBindingRow::onSearchClicked() {
  // Open a full-screen GlobalSearchWidget dialog (dark themed)
  QDialog dlg(this);
  dlg.setWindowTitle(QString("Search: %1").arg(m_def.label));
  dlg.resize(820, 520);
  dlg.setStyleSheet(
      "QDialog { background:#ffffff; color:#0f172a; }"
      "QLabel  { color:#475569; }"
      "QPushButton { background:#f1f5f9; color:#334155; border:1px solid "
      "#cbd5e1;"
      "              border-radius:4px; padding:5px 14px; font-size:12px; }"
      "QPushButton:hover { background:#e2e8f0; color:#0f172a; }");
  auto *vl = new QVBoxLayout(&dlg);
  vl->setContentsMargins(12, 12, 12, 12);
  auto *sw = new GlobalSearchWidget(&dlg);
  vl->addWidget(sw);

  auto *btnBox = new QHBoxLayout;
  auto *cancelBtn = new QPushButton("Cancel", &dlg);
  btnBox->addStretch();
  btnBox->addWidget(cancelBtn);
  vl->addLayout(btnBox);
  connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

  connect(sw, &GlobalSearchWidget::scripSelected, &dlg,
          [&dlg, this](const ContractData &cd) {
            applyContract(cd);
            m_inlineResults->hide();
            dlg.accept();
          });

  dlg.exec();
}

void SymbolBindingRow::onClearClicked() {
  m_resolved = false;
  m_binding = SymbolBinding{};
  m_binding.symbolId = m_def.id;

  m_nameEdit->clear();
  m_tokenLabel->setText("—");
  m_clearBtn->setEnabled(false);
  m_inlineResults->hide();
  m_inlineContracts.clear();

  emit bindingCleared(m_def.id);
}

void SymbolBindingRow::onInlineSearch(const QString &text) {
  // If already resolved and user is editing, clear the resolved state
  if (m_resolved) {
    m_resolved = false;
    m_binding = SymbolBinding{};
    m_binding.symbolId = m_def.id;
    m_clearBtn->setEnabled(false);
    m_tokenLabel->setText("—");
    emit bindingCleared(m_def.id);
  }

  if (text.length() < 2) {
    m_inlineResults->hide();
    m_inlineContracts.clear();
    return;
  }

  auto *repo = RepositoryManager::getInstance();
  if (!repo) {
    m_inlineResults->hide();
    return;
  }

  m_inlineContracts = repo->searchScripsGlobal(text, "", "", "", 50);

  if (m_inlineContracts.isEmpty()) {
    m_inlineResults->hide();
    return;
  }

  m_inlineResults->setRowCount(0);
  m_inlineResults->setRowCount(m_inlineContracts.size());
  for (int i = 0; i < m_inlineContracts.size(); ++i) {
    const auto &c = m_inlineContracts[i];
    m_inlineResults->setItem(
        i, 0,
        new QTableWidgetItem(c.displayName.isEmpty() ? c.name : c.displayName));
    QString exch = (c.exchangeInstrumentID >= 11000000) ? "BSE" : "NSE";
    m_inlineResults->setItem(i, 1, new QTableWidgetItem(exch));
    m_inlineResults->setItem(i, 2, new QTableWidgetItem(c.expiryDate));
    m_inlineResults->setItem(
        i, 3,
        new QTableWidgetItem(
            c.strikePrice > 0 ? QString::number(c.strikePrice, 'f', 0) : "-"));
    m_inlineResults->setItem(i, 4, new QTableWidgetItem(c.optionType));
  }
  m_inlineResults->show();
  m_inlineResults->resizeRowsToContents();
  if (!m_inlineResults->selectedItems().isEmpty())
    m_inlineResults->clearSelection();
}

void SymbolBindingRow::onInlineEnter() {
  int row = m_inlineResults->currentRow();
  if (row < 0 && m_inlineResults->rowCount() > 0)
    row = 0;
  if (row >= 0 && row < m_inlineContracts.size())
    pickInlineRow(row);
  else if (!m_inlineResults->isHidden())
    ; // do nothing — let user pick
  else
    onSearchClicked(); // no inline results yet, open full dialog
}

void SymbolBindingRow::pickInlineRow(int row) {
  if (row < 0 || row >= m_inlineContracts.size())
    return;
  applyContract(m_inlineContracts[row]);
  m_inlineResults->hide();
  m_inlineContracts.clear();
}

void SymbolBindingRow::applyContract(const ContractData &c) {
  m_resolved = true;
  m_binding.symbolId = m_def.id;
  m_binding.instrumentName = c.name;
  m_binding.token = c.exchangeInstrumentID;
  // Derive segment from series / instrument type
  // NSE FO → 2, NSE CM → 1, BSE FO → 12, BSE CM → 11
  QString series = c.series.toUpper();
  if (series.startsWith("EQ") || series == "BE" || series == "N")
    m_binding.segment = 1; // NSECM equity
  else
    m_binding.segment = 2; // default to NSEFO
  m_binding.lotSize = c.lotSize > 0 ? c.lotSize : 1;
  m_binding.quantity = m_qtySpinBox->value();
  m_binding.expiryDate = c.expiryDate;

  // Block signals so setting text doesn't re-trigger inline search
  m_nameEdit->blockSignals(true);
  m_nameEdit->setText(c.displayName.isEmpty() ? c.name : c.displayName);
  m_nameEdit->blockSignals(false);

  QString tokenInfo =
      QString("✔  Token: %1   Lot: %2")
          .arg(c.exchangeInstrumentID)
          .arg(c.lotSize > 0 ? QString::number(c.lotSize) : "1");
  if (!c.expiryDate.isEmpty())
    tokenInfo += QString("   Exp: %1").arg(c.expiryDate);
  if (c.strikePrice > 0)
    tokenInfo += QString("   Strike: %1 %2")
                     .arg(c.strikePrice, 0, 'f', 0)
                     .arg(c.optionType);
  m_tokenLabel->setText(tokenInfo);
  m_tokenLabel->setStyleSheet(
      "color:#16a34a; font-size:11px; font-family:monospace;");
  m_clearBtn->setEnabled(true);

  emit bindingResolved(m_def.id);
}

// ═════════════════════════════════════════════════════════════════════════════
// StrategyDeployDialog
// ═════════════════════════════════════════════════════════════════════════════

StrategyDeployDialog::StrategyDeployDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle("Deploy Strategy Template");
  setMinimumSize(860, 640);
  resize(960, 700);

  // Global light stylesheet
  setStyleSheet(R"(
        QDialog, QWidget         { background:#ffffff; color:#1e293b; }
        QTabWidget::pane         { border:1px solid #e2e8f0; background:#ffffff; }
        QTabBar::tab             { background:#f8fafc; color:#64748b; padding:8px 18px;
                                   font-size:12px; border:1px solid #e2e8f0;
                                   border-bottom:none; border-radius:4px 4px 0 0; }
        QTabBar::tab:selected    { background:#ffffff; color:#1e293b; border-color:#3b82f6;
                                   border-bottom-color:#ffffff; }
        QTabBar::tab:disabled    { color:#94a3b8; background:#f1f5f9; }
        QTableWidget             { background:#ffffff; border:1px solid #e2e8f0;
                                   gridline-color:#f1f5f9; color:#1e293b; }
        QTableWidget::item:selected { background:#dbeafe; color:#1e40af; }
        QTableWidget::item:hover    { background:#f8fafc; }
        QHeaderView::section     { background:#f8fafc; color:#475569; padding:4px 8px;
                                   border:none; border-bottom:2px solid #e2e8f0;
                                   font-size:11px; font-weight:600; }
        QLabel                   { color:#475569; }
        QLineEdit                { background:#ffffff; border:1px solid #cbd5e1;
                                   border-radius:4px; color:#0f172a; padding:5px 8px; }
        QLineEdit:focus          { border-color:#3b82f6; }
        QComboBox                { background:#ffffff; border:1px solid #cbd5e1;
                                   border-radius:4px; color:#0f172a; padding:4px 8px; }
        QComboBox::drop-down     { border:none; }
        QComboBox QAbstractItemView { background:#ffffff; color:#0f172a;
                                      border:1px solid #e2e8f0;
                                      selection-background-color:#dbeafe;
                                      selection-color:#1e40af; }
        QSpinBox, QDoubleSpinBox { background:#ffffff; border:1px solid #cbd5e1;
                                   border-radius:4px; color:#0f172a; padding:4px 6px; }
        QCheckBox                { color:#475569; }
        QCheckBox::indicator     { width:14px; height:14px; background:#ffffff;
                                   border:1px solid #cbd5e1; border-radius:3px; }
        QCheckBox::indicator:checked { background:#3b82f6; border-color:#2563eb; }
        QGroupBox                { background:#f8fafc; border:1px solid #e2e8f0;
                                   border-radius:5px; margin-top:14px; padding:8px;
                                   color:#2563eb; font-weight:700; }
        QGroupBox::title         { subcontrol-origin:margin; subcontrol-position:top left;
                                   left:10px; top:0; padding:0 5px; color:#2563eb;
                                   background:#f8fafc; }
        QScrollArea              { border:none; background:#ffffff; }
        QScrollBar:vertical      { background:#f1f5f9; width:8px; border-radius:4px; }
        QScrollBar::handle:vertical { background:#cbd5e1; border-radius:4px; }
        QPushButton              { background:#f1f5f9; color:#334155; border:1px solid #cbd5e1;
                                   border-radius:4px; padding:5px 14px; font-size:12px; }
        QPushButton:hover        { background:#e2e8f0; color:#0f172a; }
        QPushButton:disabled     { background:#f8fafc; color:#94a3b8; border-color:#e2e8f0; }
        QFrame[frameShape="4"]   { color:#e2e8f0; }
    )");

  // ── Outer layout ─────────────────────────────────────────────────────────
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(0);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  // Title bar
  auto *titleBar = new QWidget(this);
  titleBar->setStyleSheet(
      "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
      "stop:0 #1e40af,stop:1 #2563eb); padding:8px;");
  auto *titleLayout = new QHBoxLayout(titleBar);
  titleLayout->setContentsMargins(16, 10, 16, 10);
  auto *titleLbl = new QLabel("🚀  Deploy Strategy Template", titleBar);
  titleLbl->setStyleSheet(
      "color:#ffffff; font-size:15px; font-weight:700; background:transparent; "
      "letter-spacing:0.5px;");
  titleLayout->addWidget(titleLbl);
  mainLayout->addWidget(titleBar);

  // Tab widget (wizard pages)
  m_tabs = new QTabWidget(this);
  m_tabs->setTabsClosable(false);
  m_tabs->setMovable(false);
  m_tabs->addTab(buildPickTemplatePage(), "1 · Pick Template");
  m_tabs->addTab(buildBindSymbolsPage(), "2 · Bind Symbols");
  m_tabs->addTab(buildParametersPage(), "3 · Parameters");
  m_tabs->addTab(buildRiskPage(), "4 · Risk & Limits");
  // Disable forward tabs – user must go through steps in order
  for (int i = 1; i < m_tabs->count(); ++i)
    m_tabs->setTabEnabled(i, false);
  m_tabs->tabBar()->setStyleSheet(
      "QTabBar::tab { padding: 8px 20px; font-size:12px; font-weight:600; }"
      "QTabBar::tab:disabled { color:#94a3b8; background:#f1f5f9; }");
  mainLayout->addWidget(m_tabs, 1);

  // ── Instance name row (always visible below tabs) ─────────────────────
  auto *instanceFrame = new QFrame(this);
  instanceFrame->setFrameShape(QFrame::StyledPanel);
  instanceFrame->setStyleSheet(
      "background:#f8fafc; border-top:1px solid #e2e8f0; border-bottom:1px solid #e2e8f0;");
  auto *instLayout = new QHBoxLayout(instanceFrame);
  instLayout->setContentsMargins(12, 8, 12, 8);

  auto mkLbl = [&](const QString &t) {
    auto *l = new QLabel(t, instanceFrame);
    l->setStyleSheet("color:#475569; font-size:11px; font-weight:600; background:transparent;");
    return l;
  };

  instLayout->addWidget(mkLbl("Instance Name:"));
  m_instanceNameEdit = new QLineEdit(instanceFrame);
  m_instanceNameEdit->setPlaceholderText("My Strategy 1");
  m_instanceNameEdit->setFixedWidth(200);
  instLayout->addWidget(m_instanceNameEdit);

  instLayout->addWidget(mkLbl("Description:"));
  m_instanceDescEdit = new QLineEdit(instanceFrame);
  m_instanceDescEdit->setPlaceholderText("Optional description");
  m_instanceDescEdit->setFixedWidth(250);
  instLayout->addWidget(m_instanceDescEdit);

  instLayout->addWidget(mkLbl("Account:"));
  m_accountCombo = new QComboBox(instanceFrame);
  m_accountCombo->addItems({"Default", "Account1", "Account2"});
  m_accountCombo->setEditable(true);
  m_accountCombo->setFixedWidth(140);
  instLayout->addWidget(m_accountCombo);
  instLayout->addStretch();

  mainLayout->addWidget(instanceFrame);

  // ── Navigation buttons ────────────────────────────────────────────────
  auto *navBar = new QWidget(this);
  navBar->setStyleSheet(
      "background:#f1f5f9; border-top:2px solid #e2e8f0;");
  auto *navLayout = new QHBoxLayout(navBar);
  navLayout->setContentsMargins(16, 10, 16, 10);

  m_cancelBtn = new QPushButton("Cancel", navBar);
  m_backBtn = new QPushButton("◀ Back", navBar);
  m_nextBtn = new QPushButton("Next ▶", navBar);
  m_deployBtn = new QPushButton("🚀 Deploy", navBar);

  m_backBtn->setEnabled(false);
  m_deployBtn->setEnabled(false);
  m_deployBtn->setStyleSheet(
      "QPushButton { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
      "stop:0 #16a34a,stop:1 #15803d); color:white; font-weight:700;"
      "padding:6px 20px; border-radius:4px; border:none; }"
      "QPushButton:hover { background:#22c55e; }"
      "QPushButton:disabled { background:#f1f5f9; color:#94a3b8; border:1px solid #e2e8f0; "
      "}");
  m_nextBtn->setStyleSheet(
      "QPushButton { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
      "stop:0 #2563eb,stop:1 #1d4ed8); color:white;"
      "padding:6px 20px; border-radius:4px; border:none; font-weight:700; }"
      "QPushButton:hover { background:#3b82f6; }");

  navLayout->addWidget(m_cancelBtn);
  navLayout->addStretch();
  navLayout->addWidget(m_backBtn);
  navLayout->addWidget(m_nextBtn);
  navLayout->addWidget(m_deployBtn);
  mainLayout->addWidget(navBar);

  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_backBtn, &QPushButton::clicked, this,
          &StrategyDeployDialog::onBackClicked);
  connect(m_nextBtn, &QPushButton::clicked, this,
          &StrategyDeployDialog::onNextClicked);
  connect(m_deployBtn, &QPushButton::clicked, this,
          &StrategyDeployDialog::onDeployClicked);

  loadTemplates();
}

// ─────────────────────────────────────────────────────────────────────────────
// Page 0 – Pick Template
// ─────────────────────────────────────────────────────────────────────────────
QWidget *StrategyDeployDialog::buildPickTemplatePage() {
  auto *page = new QWidget(this);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(12, 12, 12, 12);

  auto *splitter = new QSplitter(Qt::Horizontal, page);

  // Left: template list
  auto *leftWidget = new QWidget(splitter);
  auto *leftLayout = new QVBoxLayout(leftWidget);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->addWidget(new QLabel("<b>Available Templates</b>"));

  m_templateTable = new QTableWidget(0, 4, leftWidget);
  m_templateTable->setHorizontalHeaderLabels(
      {"Name", "Mode", "Symbols", "Params"});
  m_templateTable->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::Stretch);
  m_templateTable->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  m_templateTable->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);
  m_templateTable->horizontalHeader()->setSectionResizeMode(
      3, QHeaderView::ResizeToContents);
  m_templateTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_templateTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_templateTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_templateTable->setAlternatingRowColors(true);
  m_templateTable->setShowGrid(false);
  m_templateTable->verticalHeader()->setVisible(false);

  leftLayout->addWidget(m_templateTable, 1);
  splitter->addWidget(leftWidget);

  // Right: description panel
  auto *rightWidget = new QWidget(splitter);
  auto *rightLayout = new QVBoxLayout(rightWidget);
  rightLayout->setContentsMargins(8, 0, 0, 0);
  rightLayout->addWidget(new QLabel("<b>Template Details</b>"));

  m_templateMeta = new QLabel("Select a template to see details", rightWidget);
  m_templateMeta->setStyleSheet("color:#64748b; font-size:11px;");
  m_templateMeta->setWordWrap(true);
  rightLayout->addWidget(m_templateMeta);

  m_templateDesc = new QLabel("", rightWidget);
  m_templateDesc->setWordWrap(true);
  m_templateDesc->setStyleSheet("color:#334155; font-size:12px; padding:6px; "
                                "background:#f8fafc; border-radius:4px; border:1px solid #e2e8f0;");
  m_templateDesc->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  rightLayout->addWidget(m_templateDesc, 1);

  splitter->addWidget(rightWidget);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);

  layout->addWidget(splitter);

  connect(m_templateTable, &QTableWidget::itemSelectionChanged, this,
          &StrategyDeployDialog::onTemplateSelectionChanged);
  connect(m_templateTable, &QTableWidget::itemDoubleClicked, this,
          &StrategyDeployDialog::onTemplateDoubleClicked);

  return page;
}

// ─────────────────────────────────────────────────────────────────────────────
// Page 1 – Bind Symbols
// ─────────────────────────────────────────────────────────────────────────────
QWidget *StrategyDeployDialog::buildBindSymbolsPage() {
  m_symbolPage = new QWidget(this);
  // Content is populated in populateSymbolPage() after a template is chosen
  auto *layout = new QVBoxLayout(m_symbolPage);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->addWidget(new QLabel("Select a template first (step 1)."));
  return m_symbolPage;
}

// ─────────────────────────────────────────────────────────────────────────────
// Page 2 – Parameters
// ─────────────────────────────────────────────────────────────────────────────
QWidget *StrategyDeployDialog::buildParametersPage() {
  m_paramsPage = new QWidget(this);
  auto *layout = new QVBoxLayout(m_paramsPage);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->addWidget(new QLabel("Select a template first (step 1)."));
  return m_paramsPage;
}

// ─────────────────────────────────────────────────────────────────────────────
// Page 3 – Risk
// ─────────────────────────────────────────────────────────────────────────────
QWidget *StrategyDeployDialog::buildRiskPage() {
  m_riskPage = new QWidget(this);
  auto *scroll = new QScrollArea(m_riskPage);
  scroll->setWidgetResizable(true);
  auto *content = new QWidget;
  scroll->setWidget(content);
  auto *mainLay = new QVBoxLayout(m_riskPage);
  mainLay->setContentsMargins(0, 0, 0, 0);
  mainLay->addWidget(scroll);

  // All groups stacked vertically inside the scroll content
  auto *vl = new QVBoxLayout(content);
  vl->setContentsMargins(20, 16, 20, 16);
  vl->setSpacing(12);

  // ── Stop-loss ──
  auto *slGroup = new QGroupBox("Stop Loss", content);
  auto *slLay = new QFormLayout(slGroup);

  m_slPctSpin = new QDoubleSpinBox(slGroup);
  m_slPctSpin->setRange(0.0, 100.0);
  m_slPctSpin->setSingleStep(0.1);
  m_slPctSpin->setDecimals(2);
  m_slPctSpin->setSuffix(" %");
  m_slPctSpin->setValue(1.0);

  m_slLockedCheck =
      new QCheckBox("Locked (cannot be changed by user)", slGroup);

  slLay->addRow("Stop-loss %:", m_slPctSpin);
  slLay->addRow("", m_slLockedCheck);

  // ── Target ──
  auto *tgtGroup = new QGroupBox("Target", content);
  auto *tgtLay = new QFormLayout(tgtGroup);

  m_tgtPctSpin = new QDoubleSpinBox(tgtGroup);
  m_tgtPctSpin->setRange(0.0, 1000.0);
  m_tgtPctSpin->setSingleStep(0.1);
  m_tgtPctSpin->setDecimals(2);
  m_tgtPctSpin->setSuffix(" %");
  m_tgtPctSpin->setValue(2.0);

  m_tgtLockedCheck = new QCheckBox("Locked", tgtGroup);
  tgtLay->addRow("Target %:", m_tgtPctSpin);
  tgtLay->addRow("", m_tgtLockedCheck);

  // ── Trailing stop ──
  auto *trailGroup = new QGroupBox("Trailing Stop", content);
  auto *trailLay = new QFormLayout(trailGroup);

  m_trailingCheck = new QCheckBox("Enable trailing stop", trailGroup);

  m_trailTriggerSpin = new QDoubleSpinBox(trailGroup);
  m_trailTriggerSpin->setRange(0.0, 100.0);
  m_trailTriggerSpin->setSingleStep(0.1);
  m_trailTriggerSpin->setDecimals(2);
  m_trailTriggerSpin->setSuffix(" % profit to activate");
  m_trailTriggerSpin->setValue(1.0);
  m_trailTriggerSpin->setEnabled(false);

  m_trailAmountSpin = new QDoubleSpinBox(trailGroup);
  m_trailAmountSpin->setRange(0.0, 100.0);
  m_trailAmountSpin->setSingleStep(0.1);
  m_trailAmountSpin->setDecimals(2);
  m_trailAmountSpin->setSuffix(" % trail");
  m_trailAmountSpin->setValue(0.5);
  m_trailAmountSpin->setEnabled(false);

  connect(m_trailingCheck, &QCheckBox::toggled, m_trailTriggerSpin,
          &QWidget::setEnabled);
  connect(m_trailingCheck, &QCheckBox::toggled, m_trailAmountSpin,
          &QWidget::setEnabled);

  trailLay->addRow("", m_trailingCheck);
  trailLay->addRow("Trigger:", m_trailTriggerSpin);
  trailLay->addRow("Trail amount:", m_trailAmountSpin);

  // ── Time exit ──
  auto *timeGroup = new QGroupBox("Time-Based Exit", content);
  auto *timeLay = new QFormLayout(timeGroup);

  m_timeExitCheck = new QCheckBox("Exit at time", timeGroup);
  m_timeExitEdit = new QLineEdit("15:15", timeGroup);
  m_timeExitEdit->setInputMask("99:99");
  m_timeExitEdit->setFixedWidth(80);
  m_timeExitEdit->setEnabled(false);

  connect(m_timeExitCheck, &QCheckBox::toggled, m_timeExitEdit,
          &QWidget::setEnabled);

  timeLay->addRow("", m_timeExitCheck);
  timeLay->addRow("Exit time:", m_timeExitEdit);

  // ── Daily limits ──
  auto *limitsGroup = new QGroupBox("Daily Limits", content);
  auto *limitsLay = new QFormLayout(limitsGroup);

  m_maxTradesSpin = new QSpinBox(limitsGroup);
  m_maxTradesSpin->setRange(1, 200);
  m_maxTradesSpin->setValue(10);

  m_maxDailyLossSpin = new QDoubleSpinBox(limitsGroup);
  m_maxDailyLossSpin->setRange(0, 1000000);
  m_maxDailyLossSpin->setSingleStep(500);
  m_maxDailyLossSpin->setDecimals(0);
  m_maxDailyLossSpin->setPrefix("₹ ");
  m_maxDailyLossSpin->setValue(5000);

  limitsLay->addRow("Max trades / day:", m_maxTradesSpin);
  limitsLay->addRow("Max daily loss:", m_maxDailyLossSpin);

  vl->addWidget(slGroup);
  vl->addWidget(tgtGroup);
  vl->addWidget(trailGroup);
  vl->addWidget(timeGroup);
  vl->addWidget(limitsGroup);
  vl->addStretch();

  return m_riskPage;
}

// ─────────────────────────────────────────────────────────────────────────────
// Data loading
// ─────────────────────────────────────────────────────────────────────────────
void StrategyDeployDialog::loadTemplates() {
  StrategyTemplateRepository &repo = StrategyTemplateRepository::instance();
  if (!repo.isOpen()) {
    m_templateTable->setRowCount(0);
    return;
  }

  m_allTemplates = repo.loadAllTemplates();

  m_templateTable->setRowCount(0);
  for (int i = 0; i < m_allTemplates.size(); ++i) {
    const StrategyTemplate &t = m_allTemplates[i];
    m_templateTable->insertRow(i);

    auto *nameItem = new QTableWidgetItem(t.name);
    nameItem->setToolTip(t.description);
    m_templateTable->setItem(i, 0, nameItem);

    QString modeStr;
    switch (t.mode) {
    case StrategyMode::IndicatorBased:
      modeStr = "Indicator";
      break;
    case StrategyMode::OptionMultiLeg:
      modeStr = "Options";
      break;
    case StrategyMode::Spread:
      modeStr = "Spread";
      break;
    }
    m_templateTable->setItem(i, 1, new QTableWidgetItem(modeStr));
    m_templateTable->setItem(
        i, 2, new QTableWidgetItem(QString::number(t.symbols.size())));
    m_templateTable->setItem(
        i, 3, new QTableWidgetItem(QString::number(t.params.size())));
  }

  if (!m_allTemplates.isEmpty())
    m_templateTable->selectRow(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Template selection
// ─────────────────────────────────────────────────────────────────────────────
void StrategyDeployDialog::onTemplateSelectionChanged() {
  int row = m_templateTable->currentRow();
  if (row < 0 || row >= m_allTemplates.size()) {
    m_templateDesc->clear();
    m_templateMeta->setText("—");
    return;
  }

  const StrategyTemplate &t = m_allTemplates[row];
  m_templateDesc->setText(t.description.isEmpty() ? "<i>(no description)</i>"
                                                  : t.description);

  QStringList lines;
  lines << QString("<b>Version:</b> %1").arg(t.version);
  lines << QString("<b>Symbols:</b> %1 slot(s)").arg(t.symbols.size());
  for (const auto &s : t.symbols) {
    QString roleStr = (s.role == SymbolRole::Reference) ? "REF" : "TRADE";
    lines << QString("  • [%1] %2").arg(roleStr, s.label);
  }
  if (!t.params.isEmpty()) {
    lines << QString("<b>Parameters:</b> %1").arg(t.params.size());
    for (const auto &p : t.params)
      lines << QString("  • %1 (default: %2)")
                   .arg(p.label)
                   .arg(p.defaultValue.toString());
  }
  lines << QString("<b>Indicators:</b> %1").arg(t.indicators.size());
  m_templateMeta->setText(lines.join("<br>"));
}

void StrategyDeployDialog::onTemplateDoubleClicked() {
  onNextClicked(); // treat as pressing "Next"
}

// ─────────────────────────────────────────────────────────────────────────────
// Navigation
// ─────────────────────────────────────────────────────────────────────────────
void StrategyDeployDialog::onNextClicked() {
  if (!validateCurrentPage())
    return;

  int nextPage = m_currentPage + 1;

  if (m_currentPage == 0) {
    // Template chosen → populate symbol page
    int row = m_templateTable->currentRow();
    m_template = m_allTemplates[row];
    populateSymbolPage();
    populateParamsPage();
    populateRiskPage();

    // Set instance name default
    if (m_instanceNameEdit->text().isEmpty())
      m_instanceNameEdit->setText(m_template.name + " #1");
  }

  goToPage(nextPage);
}

void StrategyDeployDialog::onBackClicked() { goToPage(m_currentPage - 1); }

void StrategyDeployDialog::goToPage(int index) {
  if (index < 0 || index >= m_tabs->count())
    return;

  m_currentPage = index;
  m_tabs->setTabEnabled(index, true);
  m_tabs->setCurrentIndex(index);

  m_backBtn->setEnabled(index > 0);
  m_nextBtn->setVisible(index < m_tabs->count() - 1);
  m_deployBtn->setVisible(index == m_tabs->count() - 1);
  m_deployBtn->setEnabled(index == m_tabs->count() - 1);
}

bool StrategyDeployDialog::validateCurrentPage() {
  switch (m_currentPage) {
  case 0:
    if (m_templateTable->currentRow() < 0) {
      QMessageBox::warning(this, "Select Template",
                           "Please select a strategy template to continue.");
      return false;
    }
    return true;

  case 1: {
    // Check all TRADE symbol slots are resolved
    // (REF slots are optional – indicators will just skip)
    QStringList missing;
    for (auto *row : m_bindingRows) {
      if (!row->isResolved()) {
        // Find the definition to check role
        for (const auto &sym : m_template.symbols) {
          if (sym.id == row->binding().symbolId &&
              sym.role == SymbolRole::Trade) {
            missing << sym.label;
          }
        }
      }
    }
    if (!missing.isEmpty()) {
      QMessageBox::warning(
          this, "Missing Symbols",
          "Please bind all TRADE symbols before proceeding:\n• " +
              missing.join("\n• "));
      return false;
    }
    return true;
  }

  case 2:
    return true; // params always valid (have defaults)

  case 3:
    if (m_instanceNameEdit->text().trimmed().isEmpty()) {
      QMessageBox::warning(this, "Instance Name",
                           "Please provide a name for this strategy instance.");
      return false;
    }
    return true;

  default:
    return true;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Populate symbol binding page dynamically from template
// ─────────────────────────────────────────────────────────────────────────────
void StrategyDeployDialog::populateSymbolPage() {
  // Clear existing
  QLayout *oldLayout = m_symbolPage->layout();
  if (oldLayout) {
    QLayoutItem *item;
    while ((item = oldLayout->takeAt(0)) != nullptr) {
      if (item->widget())
        item->widget()->deleteLater();
      delete item;
    }
    delete oldLayout;
  }
  m_bindingRows.clear();

  auto *layout = new QVBoxLayout(m_symbolPage);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  layout->addWidget(
      new QLabel(QString("<b>Bind instruments for template: <span "
                         "style='color:#64b5f6'>%1</span></b>")
                     .arg(m_template.name)));

  auto *sep = new QFrame(m_symbolPage);
  sep->setFrameShape(QFrame::HLine);
  sep->setStyleSheet("color:#e2e8f0;");
  layout->addWidget(sep);

  auto *infoLbl = new QLabel("For each symbol slot, click <b>Search</b> to "
                             "pick a real instrument from the master file.<br>"
                             "TRADE slots require a valid instrument. REF "
                             "slots are optional but recommended.",
                             m_symbolPage);
  infoLbl->setWordWrap(true);
  infoLbl->setStyleSheet("color:#64748b; font-size:11px; padding:4px 6px; "
                         "background:#f0f9ff; border-radius:4px; border:1px solid #bae6fd;");
  layout->addWidget(infoLbl);

  for (const auto &sym : m_template.symbols) {
    auto *row = new SymbolBindingRow(sym, m_symbolPage);
    connect(row, &SymbolBindingRow::bindingResolved, this,
            &StrategyDeployDialog::onBindingResolved);
    connect(row, &SymbolBindingRow::bindingCleared, this,
            &StrategyDeployDialog::onBindingCleared);
    m_bindingRows.append(row);
    layout->addWidget(row);
  }

  layout->addStretch();
}

// ─────────────────────────────────────────────────────────────────────────────
// Populate parameters page dynamically from template
// ─────────────────────────────────────────────────────────────────────────────
void StrategyDeployDialog::populateParamsPage() {
  QLayout *oldLayout = m_paramsPage->layout();
  if (oldLayout) {
    QLayoutItem *item;
    while ((item = oldLayout->takeAt(0)) != nullptr) {
      if (item->widget())
        item->widget()->deleteLater();
      delete item;
    }
    delete oldLayout;
  }
  m_paramEditors.clear();

  auto *scroll = new QScrollArea(m_paramsPage);
  scroll->setWidgetResizable(true);
  auto *content = new QWidget(scroll);
  scroll->setWidget(content);
  auto *outerLay = new QVBoxLayout(m_paramsPage);
  outerLay->setContentsMargins(0, 0, 0, 0);
  outerLay->addWidget(scroll);

  auto *formLay = new QFormLayout(content);
  formLay->setContentsMargins(20, 16, 20, 16);
  formLay->setRowWrapPolicy(QFormLayout::WrapLongRows);
  formLay->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  formLay->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

  if (m_template.params.isEmpty()) {
    formLay->addRow(new QLabel(
        "<i>This template has no configurable parameters.</i>", content));
    return;
  }

  for (const TemplateParam &p : m_template.params) {
    QWidget *editor = nullptr;
    QString labelText = p.label.isEmpty() ? p.name : p.label;

    switch (p.valueType) {
    case ParamValueType::Int: {
      auto *spin = new QSpinBox(content);
      int lo = p.minValue.isValid() ? p.minValue.toInt() : 1;
      int hi = p.maxValue.isValid() ? p.maxValue.toInt() : 9999;
      spin->setRange(lo, hi);
      spin->setValue(p.defaultValue.toInt());
      spin->setFixedWidth(120);
      editor = spin;
      break;
    }
    case ParamValueType::Double: {
      auto *spin = new QDoubleSpinBox(content);
      double lo = p.minValue.isValid() ? p.minValue.toDouble() : 0.0;
      double hi = p.maxValue.isValid() ? p.maxValue.toDouble() : 1e9;
      spin->setRange(lo, hi);
      spin->setDecimals(4);
      spin->setSingleStep(0.01);
      spin->setValue(p.defaultValue.toDouble());
      spin->setFixedWidth(140);
      editor = spin;
      break;
    }
    case ParamValueType::Bool: {
      auto *cb = new QCheckBox(content);
      cb->setChecked(p.defaultValue.toBool());
      editor = cb;
      break;
    }
    case ParamValueType::String: {
      auto *le = new QLineEdit(content);
      le->setText(p.defaultValue.toString());
      le->setFixedWidth(220);
      editor = le;
      break;
    }
    case ParamValueType::Expression: {
      // Show formula with trigger info — user can override with a fixed value
      auto *le = new QLineEdit(content);
      le->setText(p.expression.isEmpty() ? p.defaultValue.toString()
                                         : p.expression);
      le->setPlaceholderText("Formula (e.g. ATR(REF_1,14)*2.5) or fixed number...");
      le->setFixedWidth(300);

      // Build trigger label string
      QString triggerStr;
      switch (p.trigger) {
      case ParamTrigger::EveryTick:     triggerStr = "⚡ Every Tick"; break;
      case ParamTrigger::OnCandleClose: triggerStr = "🕯 On Candle Close";
          if (!p.triggerTimeframe.isEmpty()) triggerStr += " (" + p.triggerTimeframe + ")";
          break;
      case ParamTrigger::OnEntry:       triggerStr = "📥 On Entry"; break;
      case ParamTrigger::OnExit:        triggerStr = "📤 On Exit"; break;
      case ParamTrigger::OnceAtStart:   triggerStr = "🔒 Once at Start"; break;
      case ParamTrigger::OnSchedule:
          triggerStr = QString("⏲ Every %1s").arg(p.scheduleIntervalSec); break;
      case ParamTrigger::Manual:        triggerStr = "✋ Manual"; break;
      }

      auto *trigLabel = new QLabel(triggerStr, content);
      trigLabel->setStyleSheet(
          "color:#475569; font-size:10px; background:#f1f5f9; "
          "padding:2px 6px; border-radius:3px; border:1px solid #e2e8f0;");
      trigLabel->setToolTip(
          "Recalculation trigger — set in the template builder.\n"
          "To override: type a plain number to freeze the value.");

      auto *formulaRow = new QWidget(content);
      auto *formulaLay = new QHBoxLayout(formulaRow);
      formulaLay->setContentsMargins(0, 0, 0, 0);
      formulaLay->setSpacing(6);
      formulaLay->addWidget(le);
      formulaLay->addWidget(trigLabel);
      formulaLay->addStretch();

      editor = le; // the QLineEdit is the editor for value extraction
      // We need to use formulaRow as the row widget below
      // Adjust: skip the standard rowWidget creation — use formulaRow directly
      if (!p.description.isEmpty())
        editor->setToolTip(p.description);

      QString rangeHintExpr;
      formLay->addRow(labelText + ":", formulaRow);
      m_paramEditors.insert(p.name, editor);
      continue; // skip the standard row creation below
    }
    }

    if (!editor)
      continue;

    if (!p.description.isEmpty())
      editor->setToolTip(p.description);

    auto *rowWidget = new QWidget(content);
    auto *rowLay = new QHBoxLayout(rowWidget);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->addWidget(editor);
    if (!p.description.isEmpty()) {
      auto *hint = new QLabel("ⓘ", rowWidget);
      hint->setToolTip(p.description);
      hint->setStyleSheet("color:#2563eb; font-size:14px; cursor:help;");
      rowLay->addWidget(hint);
    }
    rowLay->addStretch();

    QString rangeHint;
    if (p.minValue.isValid() || p.maxValue.isValid()) {
      if (p.minValue.isValid() && p.maxValue.isValid())
        rangeHint = QString(" [%1 – %2]")
                        .arg(p.minValue.toString())
                        .arg(p.maxValue.toString());
      else if (p.minValue.isValid())
        rangeHint = QString(" [min: %1]").arg(p.minValue.toString());
      else
        rangeHint = QString(" [max: %1]").arg(p.maxValue.toString());
    }

    formLay->addRow(labelText + rangeHint + ":", rowWidget);
    m_paramEditors.insert(p.name, editor);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Populate risk page defaults from template
// ─────────────────────────────────────────────────────────────────────────────
void StrategyDeployDialog::populateRiskPage() {
  const RiskDefaults &r = m_template.riskDefaults;

  m_slPctSpin->setValue(r.stopLossPercent);
  m_slLockedCheck->setChecked(r.stopLossLocked);
  if (r.stopLossLocked)
    m_slPctSpin->setEnabled(false);

  m_tgtPctSpin->setValue(r.targetPercent);
  m_tgtLockedCheck->setChecked(r.targetLocked);
  if (r.targetLocked)
    m_tgtPctSpin->setEnabled(false);

  m_trailingCheck->setChecked(r.trailingEnabled);
  m_trailTriggerSpin->setValue(r.trailingTriggerPct);
  m_trailTriggerSpin->setEnabled(r.trailingEnabled);
  m_trailAmountSpin->setValue(r.trailingAmountPct);
  m_trailAmountSpin->setEnabled(r.trailingEnabled);

  m_timeExitCheck->setChecked(r.timeExitEnabled);
  m_timeExitEdit->setText(r.exitTime);
  m_timeExitEdit->setEnabled(r.timeExitEnabled);

  m_maxTradesSpin->setValue(r.maxDailyTrades);
  m_maxDailyLossSpin->setValue(r.maxDailyLossRs);
}

// ─────────────────────────────────────────────────────────────────────────────
// Binding status feedback
// ─────────────────────────────────────────────────────────────────────────────
void StrategyDeployDialog::onBindingResolved(
    const QString &) { /* no-op for now */
}
void StrategyDeployDialog::onBindingCleared(
    const QString &) { /* no-op for now */
}

// ─────────────────────────────────────────────────────────────────────────────
// Deploy
// ─────────────────────────────────────────────────────────────────────────────
void StrategyDeployDialog::onDeployClicked() {
  if (!validateCurrentPage())
    return;

  QString name = m_instanceNameEdit->text().trimmed();
  if (name.isEmpty()) {
    QMessageBox::warning(this, "Instance Name",
                         "Please provide a name for this strategy instance.");
    return;
  }

  accept();
}

// ─────────────────────────────────────────────────────────────────────────────
// Outputs
// ─────────────────────────────────────────────────────────────────────────────
QVector<SymbolBinding> StrategyDeployDialog::symbolBindings() const {
  QVector<SymbolBinding> result;
  for (auto *row : m_bindingRows) {
    SymbolBinding b = row->binding();
    // Override qty from spin (in case user changed it after binding)
    result.append(b);
  }
  return result;
}

QVariantMap StrategyDeployDialog::paramValues() const {
  QVariantMap values;
  for (const TemplateParam &p : m_template.params) {
    auto it = m_paramEditors.find(p.name);
    if (it == m_paramEditors.end()) {
      values[p.name] = p.defaultValue;
      continue;
    }
    QWidget *editor = it.value();
    switch (p.valueType) {
    case ParamValueType::Int:
      values[p.name] = qobject_cast<QSpinBox *>(editor)->value();
      break;
    case ParamValueType::Double:
      values[p.name] = qobject_cast<QDoubleSpinBox *>(editor)->value();
      break;
    case ParamValueType::Bool:
      values[p.name] = qobject_cast<QCheckBox *>(editor)->isChecked();
      break;
    case ParamValueType::String:
      values[p.name] = qobject_cast<QLineEdit *>(editor)->text();
      break;
    case ParamValueType::Expression:
      values[p.name] = qobject_cast<QLineEdit *>(editor)->text();
      break;
    }
  }
  return values;
}

RiskDefaults StrategyDeployDialog::riskOverride() const {
  RiskDefaults r;
  r.stopLossPercent = m_slPctSpin->value();
  r.stopLossLocked = m_slLockedCheck->isChecked();
  r.targetPercent = m_tgtPctSpin->value();
  r.targetLocked = m_tgtLockedCheck->isChecked();
  r.trailingEnabled = m_trailingCheck->isChecked();
  r.trailingTriggerPct = m_trailTriggerSpin->value();
  r.trailingAmountPct = m_trailAmountSpin->value();
  r.timeExitEnabled = m_timeExitCheck->isChecked();
  r.exitTime = m_timeExitEdit->text();
  r.maxDailyTrades = m_maxTradesSpin->value();
  r.maxDailyLossRs = m_maxDailyLossSpin->value();
  return r;
}

StrategyInstance StrategyDeployDialog::buildInstance() const {
  StrategyInstance inst;
  inst.instanceName = m_instanceNameEdit->text().trimmed();
  inst.description = m_instanceDescEdit->text().trimmed();
  inst.strategyType = m_template.name;
  inst.account = m_accountCombo->currentText().trimmed();
  inst.state = StrategyState::Created;
  inst.createdAt = QDateTime::currentDateTime();
  inst.lastUpdated = inst.createdAt;

  // Carry params
  inst.parameters = paramValues();

  // Encode templateId so the runtime engine can load it
  inst.parameters["__templateId__"] = m_template.templateId;
  inst.parameters["__templateName__"] = m_template.name;

  // Risk
  RiskDefaults r = riskOverride();
  inst.stopLoss = r.stopLossPercent;
  inst.target = r.targetPercent;

  // Symbol bindings serialised to parameters map for persistence
  QVariantList bindingsJson;
  for (const SymbolBinding &b : symbolBindings()) {
    QVariantMap bmap;
    bmap["symbolId"] = b.symbolId;
    bmap["instrumentName"] = b.instrumentName;
    bmap["token"] = (qlonglong)b.token;
    bmap["segment"] = b.segment;
    bmap["lotSize"] = b.lotSize;
    bmap["quantity"] = b.quantity;
    bmap["expiryDate"] = b.expiryDate;
    bindingsJson.append(bmap);
  }
  inst.parameters["__symbolBindings__"] = bindingsJson;

  // Use the primary TRADE symbol as the inst.symbol for display
  for (const SymbolBinding &b : symbolBindings()) {
    for (const SymbolDefinition &def : m_template.symbols) {
      if (def.id == b.symbolId && def.role == SymbolRole::Trade) {
        inst.symbol = b.instrumentName;
        inst.quantity = b.quantity;
        break;
      }
    }
    if (!inst.symbol.isEmpty())
      break;
  }
  if (inst.symbol.isEmpty() && !m_bindingRows.isEmpty())
    inst.symbol = m_bindingRows.first()->binding().instrumentName;

  return inst;
}

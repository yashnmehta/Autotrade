#include "strategy/builder/SymbolBindingWidget.h"
#include "repository/RepositoryManager.h"
#include "ui/GlobalSearchWidget.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

SymbolBindingWidget::SymbolBindingWidget(const SymbolDefinition &def,
                                         QWidget *parent)
    : QWidget(parent), m_def(def) {
  m_binding.symbolId = def.id;
  setupUI(def);
}

void SymbolBindingWidget::setupUI(const SymbolDefinition &def) {
  // Card-style outer frame (Light Theme)
  setStyleSheet(
      "SymbolBindingWidget { background:#ffffff; border:1px solid #e2e8f0; "
      "border-radius:6px; }"
      "QLabel { color:#475569; font-size:12px; background:transparent; }"
      "QLabel#tokenLabel { color:#0e7490; font-size:11px; "
      "font-family:monospace; }"
      "QLabel#resolvedLabel { color:#16a34a; font-size:11px; font-weight:600; }"
      "QLineEdit { background:#f8fafc; border:1px solid #cbd5e1; "
      "border-radius:4px;"
      "            color:#0f172a; padding:5px 8px; font-size:12px; }"
      "QLineEdit:focus { border-color:#3b82f6; background:#ffffff; }"
      "QPushButton { background:#f1f5f9; color:#475569; border:1px solid "
      "#cbd5e1;"
      "              border-radius:4px; padding:5px 12px; font-size:12px; }"
      "QPushButton:hover { background:#e2e8f0; color:#1e293b; }"
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
  QString roleColor = isRef ? "#2563eb" : "#16a34a"; // Blue-600 / Green-600
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
  slotLbl->setStyleSheet("color:#0f172a; font-size:12px; font-weight:600; "
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
      "QTableWidget::item:hover { background:#f1f5f9; }"
      "QHeaderView::section { background:#f8fafc; color:#64748b; padding:3px "
      "6px;"
      "                       border:none; border-bottom:1px solid #e2e8f0; "
      "font-size:11px; }");
  m_inlineResults->hide();
  outer->addWidget(m_inlineResults);

  // ── Connections ──
  connect(m_nameEdit, &QLineEdit::textChanged, this,
          &SymbolBindingWidget::onInlineSearch);
  connect(m_nameEdit, &QLineEdit::returnPressed, this,
          &SymbolBindingWidget::onInlineEnter);
  connect(m_searchBtn, &QPushButton::clicked, this,
          &SymbolBindingWidget::onSearchClicked);
  connect(m_clearBtn, &QPushButton::clicked, this,
          &SymbolBindingWidget::onClearClicked);
  connect(m_inlineResults, &QTableWidget::cellDoubleClicked, this,
          [this](int row, int) { pickInlineRow(row); });
  // Also allow Enter key in the results table
  connect(m_inlineResults, &QTableWidget::itemSelectionChanged, this, [this]() {
    int row = m_inlineResults->currentRow();
    if (row >= 0 && row < m_inlineContracts.size())
      m_inlineResults->setToolTip(m_inlineContracts[row].name);
  });
}

void SymbolBindingWidget::onSearchClicked() {
  // Open a full-screen GlobalSearchWidget dialog (dark themed)
  QDialog dlg(this);
  dlg.setWindowTitle(QString("Search: %1").arg(m_def.label));
  dlg.resize(820, 520);
  dlg.setStyleSheet(
      "QDialog { background:#ffffff; color:#0f172a; }"
      "QLabel  { color:#475569; }"
      "QPushButton { background:#f1f5f9; color:#475569; border:1px solid "
      "#cbd5e1;"
      "              border-radius:4px; padding:5px 14px; font-size:12px; }"
      "QPushButton:hover { background:#e2e8f0; color:#1e293b; }");
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

void SymbolBindingWidget::onClearClicked() {
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

void SymbolBindingWidget::onInlineSearch(const QString &text) {
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

void SymbolBindingWidget::onInlineEnter() {
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

void SymbolBindingWidget::pickInlineRow(int row) {
  if (row < 0 || row >= m_inlineContracts.size())
    return;
  applyContract(m_inlineContracts[row]);
  m_inlineResults->hide();
  m_inlineContracts.clear();
}

void SymbolBindingWidget::applyContract(const ContractData &c) {
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

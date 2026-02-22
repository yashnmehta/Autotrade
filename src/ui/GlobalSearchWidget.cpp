#include "ui/GlobalSearchWidget.h"
#include "api/XTSTypes.h"
#include "repository/RepositoryManager.h"
#include <QDebug>
#include <QHeaderView>
#include <QLabel>
#include <QShortcut>
#include <QVBoxLayout>

GlobalSearchWidget::GlobalSearchWidget(QWidget *parent) : QWidget(parent) {
  setupUI();

  // Initial population
  m_exchangeCombo->addItems({"All Exchanges", "NSE", "BSE"});
  m_segmentCombo->addItems({"All Segments", "Cash", "F&O"});
  m_expiryCombo->addItem("All Expiries");
}

GlobalSearchWidget::~GlobalSearchWidget() = default;

void GlobalSearchWidget::setupUI() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(10, 10, 10, 10);
  mainLayout->setSpacing(10);

  // Title / Search Bar
  auto *searchLayout = new QHBoxLayout();
  m_searchEdit = new QLineEdit(this);
  m_searchEdit->setPlaceholderText(
      "Search scripts (e.g., NIFTY 17FEB 26000 CE)");
  m_searchEdit->setMinimumHeight(35);
  m_searchEdit->setStyleSheet(
      "font-size: 14px; padding: 5px; background:#ffffff; border:1px solid #cbd5e1;"
      "border-radius:4px; color:#0f172a;");
  searchLayout->addWidget(m_searchEdit);
  mainLayout->addLayout(searchLayout);

  // Filters
  auto *filterLayout = new QHBoxLayout();

  m_exchangeCombo = new QComboBox(this);
  m_segmentCombo = new QComboBox(this);
  m_expiryCombo = new QComboBox(this);

  filterLayout->addWidget(new QLabel("Exchange:", this));
  filterLayout->addWidget(m_exchangeCombo);
  filterLayout->addWidget(new QLabel("Segment:", this));
  filterLayout->addWidget(m_segmentCombo);
  filterLayout->addWidget(new QLabel("Expiry:", this));
  filterLayout->addWidget(m_expiryCombo);
  filterLayout->addStretch();

  mainLayout->addLayout(filterLayout);

  // Results Table
  m_resultsTable = new QTableWidget(this);
  m_resultsTable->setColumnCount(6);
  m_resultsTable->setHorizontalHeaderLabels(
      {"Symbol", "Exchange", "Segment", "Expiry", "Strike", "Type"});
  m_resultsTable->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_resultsTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_resultsTable->setAlternatingRowColors(true);
  mainLayout->addWidget(m_resultsTable);

  // Styling (Light Theme)
  setStyleSheet(
      "QWidget { background-color: #ffffff; color: #1e293b; }"
      "QLineEdit { background-color: #ffffff; border: 1px solid #cbd5e1; "
      "border-radius: 4px; color: #0f172a; padding: 5px 8px; }"
      "QLineEdit:focus { border-color: #3b82f6; }"
      "QComboBox { background-color: #ffffff; border: 1px solid #cbd5e1; "
      "border-radius: 4px; padding: 4px 8px; color: #0f172a; }"
      "QComboBox::drop-down { border: none; }"
      "QComboBox QAbstractItemView { background: #ffffff; color: #0f172a; "
      "border: 1px solid #e2e8f0; selection-background-color: #dbeafe; "
      "selection-color: #1e40af; }"
      "QTableWidget { background-color: #ffffff; border: 1px solid #e2e8f0; "
      "gridline-color: #f1f5f9; color: #1e293b; }"
      "QTableWidget::item:selected { background: #dbeafe; color: #1e40af; }"
      "QTableWidget::item:hover { background: #f8fafc; }"
      "QHeaderView::section { background-color: #f8fafc; color: #475569; "
      "padding: 5px; border: none; border-bottom: 2px solid #e2e8f0; "
      "font-weight: 600; }"
      "QLabel { color: #475569; }");

  // Connections
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &GlobalSearchWidget::onSearchTextChanged);
  connect(m_searchEdit, &QLineEdit::returnPressed, this,
          &GlobalSearchWidget::onReturnPressed);
  connect(m_exchangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &GlobalSearchWidget::onFilterChanged);
  connect(m_segmentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &GlobalSearchWidget::onFilterChanged);
  connect(m_expiryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &GlobalSearchWidget::onFilterChanged);
  connect(m_resultsTable, &QTableWidget::cellDoubleClicked, this,
          &GlobalSearchWidget::onResultDoubleClicked);

  m_searchEdit->setFocus();
}

void GlobalSearchWidget::onSearchTextChanged(const QString &text) {
  // Debounce or simple timer could be added if results are too slow
  updateResults();
}

void GlobalSearchWidget::onFilterChanged() { updateResults(); }

void GlobalSearchWidget::updateResults() {
  QString query = m_searchEdit->text();
  if (query.length() < 2) {
    m_resultsTable->setRowCount(0);
    return;
  }

  QString ex = m_exchangeCombo->currentIndex() == 0
                   ? ""
                   : m_exchangeCombo->currentText();
  QString seg =
      m_segmentCombo->currentIndex() == 0 ? "" : m_segmentCombo->currentText();
  QString exp =
      m_expiryCombo->currentIndex() == 0 ? "" : m_expiryCombo->currentText();

  // Normalize segment names for RepositoryManager
  if (seg == "F&O")
    seg = "FO";

  auto *repo = RepositoryManager::getInstance();
  m_currentResults = repo->searchScripsGlobal(query, ex, seg, exp, 100);

  m_resultsTable->setRowCount(0);
  m_resultsTable->setRowCount(m_currentResults.size());

  for (int i = 0; i < m_currentResults.size(); ++i) {
    const auto &contract = m_currentResults[i];

    m_resultsTable->setItem(i, 0, new QTableWidgetItem(contract.name));

    // Identify exchange from token
    QString exchangeStr =
        (contract.exchangeInstrumentID >= 11000000) ? "BSE" : "NSE";
    m_resultsTable->setItem(i, 1, new QTableWidgetItem(exchangeStr));

    // Identify segment from strike/series
    QString segmentStr =
        (contract.strikePrice > 0 || contract.instrumentType == 1) ? "F&O"
                                                                   : "Cash";
    m_resultsTable->setItem(i, 2, new QTableWidgetItem(segmentStr));

    m_resultsTable->setItem(i, 3, new QTableWidgetItem(contract.expiryDate));
    m_resultsTable->setItem(
        i, 4,
        new QTableWidgetItem(contract.strikePrice > 0
                                 ? QString::number(contract.strikePrice, 'f', 2)
                                 : "-"));
    m_resultsTable->setItem(i, 5, new QTableWidgetItem(contract.optionType));
  }
}

void GlobalSearchWidget::onResultDoubleClicked(int row, int column) {
  if (row >= 0 && row < m_currentResults.size()) {
    emit scripSelected(m_currentResults[row]);
  }
}

void GlobalSearchWidget::onReturnPressed() {
  if (m_resultsTable->rowCount() > 0) {
    onResultDoubleClicked(
        m_resultsTable->currentRow() >= 0 ? m_resultsTable->currentRow() : 0,
        0);
  }
}

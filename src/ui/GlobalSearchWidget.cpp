#include "ui/GlobalSearchWidget.h"
#include "ui_GlobalSearchWidget.h"
#include "api/xts/XTSTypes.h"
#include "repository/RepositoryManager.h"
#include <QComboBox>
#include <QDebug>
#include <QHeaderView>
#include <QLineEdit>
#include <QTableWidget>

GlobalSearchWidget::GlobalSearchWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::GlobalSearchWidget) {
  ui->setupUi(this);

  // Table header stretch
  ui->resultsTable->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);

  // Connections
  connect(ui->searchEdit, &QLineEdit::textChanged, this,
          &GlobalSearchWidget::onSearchTextChanged);
  connect(ui->searchEdit, &QLineEdit::returnPressed, this,
          &GlobalSearchWidget::onReturnPressed);
  connect(ui->exchangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &GlobalSearchWidget::onFilterChanged);
  connect(ui->segmentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &GlobalSearchWidget::onFilterChanged);
  connect(ui->expiryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &GlobalSearchWidget::onFilterChanged);
  connect(ui->resultsTable, &QTableWidget::cellDoubleClicked, this,
          &GlobalSearchWidget::onResultDoubleClicked);

  // Initial population
  ui->exchangeCombo->addItems({"All Exchanges", "NSE", "BSE"});
  ui->segmentCombo->addItems({"All Segments", "Cash", "F&O"});
  ui->expiryCombo->addItem("All Expiries");

  ui->searchEdit->setFocus();
}

GlobalSearchWidget::~GlobalSearchWidget() { delete ui; }

void GlobalSearchWidget::onSearchTextChanged(const QString &text) {
  // Debounce or simple timer could be added if results are too slow
  updateResults();
}

void GlobalSearchWidget::onFilterChanged() { updateResults(); }

void GlobalSearchWidget::updateResults() {
  QString query = ui->searchEdit->text();
  if (query.length() < 2) {
    ui->resultsTable->setRowCount(0);
    return;
  }

  QString ex = ui->exchangeCombo->currentIndex() == 0
                   ? ""
                   : ui->exchangeCombo->currentText();
  QString seg =
      ui->segmentCombo->currentIndex() == 0 ? "" : ui->segmentCombo->currentText();
  QString exp =
      ui->expiryCombo->currentIndex() == 0 ? "" : ui->expiryCombo->currentText();

  // Normalize segment names for RepositoryManager
  if (seg == "F&O")
    seg = "FO";

  auto *repo = RepositoryManager::getInstance();
  m_currentResults = repo->searchScripsGlobal(query, ex, seg, exp, 100);

  ui->resultsTable->setRowCount(0);
  ui->resultsTable->setRowCount(m_currentResults.size());

  for (int i = 0; i < m_currentResults.size(); ++i) {
    const auto &contract = m_currentResults[i];

    ui->resultsTable->setItem(i, 0, new QTableWidgetItem(contract.name));

    // Identify exchange from token
    QString exchangeStr =
        (contract.exchangeInstrumentID >= 11000000) ? "BSE" : "NSE";
    ui->resultsTable->setItem(i, 1, new QTableWidgetItem(exchangeStr));

    // Identify segment from strike/series
    QString segmentStr =
        (contract.strikePrice > 0 || contract.instrumentType == 1) ? "F&O"
                                                                   : "Cash";
    ui->resultsTable->setItem(i, 2, new QTableWidgetItem(segmentStr));

    ui->resultsTable->setItem(i, 3, new QTableWidgetItem(contract.expiryDate));
    ui->resultsTable->setItem(
        i, 4,
        new QTableWidgetItem(contract.strikePrice > 0
                                 ? QString::number(contract.strikePrice, 'f', 2)
                                 : "-"));
    ui->resultsTable->setItem(i, 5, new QTableWidgetItem(contract.optionType));
  }
}

void GlobalSearchWidget::onResultDoubleClicked(int row, int column) {
  if (row >= 0 && row < m_currentResults.size()) {
    emit scripSelected(m_currentResults[row]);
  }
}

void GlobalSearchWidget::onReturnPressed() {
  if (ui->resultsTable->rowCount() > 0) {
    onResultDoubleClicked(
        ui->resultsTable->currentRow() >= 0 ? ui->resultsTable->currentRow() : 0,
        0);
  }
}

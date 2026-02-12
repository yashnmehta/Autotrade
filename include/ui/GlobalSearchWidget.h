#ifndef GLOBAL_SEARCH_WIDGET_H
#define GLOBAL_SEARCH_WIDGET_H

#include "repository/ContractData.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>


class GlobalSearchWidget : public QWidget {
  Q_OBJECT
public:
  explicit GlobalSearchWidget(QWidget *parent = nullptr);
  ~GlobalSearchWidget();

signals:
  void scripSelected(const ContractData &contract);

private slots:
  void onSearchTextChanged(const QString &text);
  void onFilterChanged();
  void onResultDoubleClicked(int row, int column);
  void onReturnPressed();

private:
  void setupUI();
  void updateResults();
  void populateExpiries(const QString &symbol);

  QLineEdit *m_searchEdit;
  QComboBox *m_exchangeCombo;
  QComboBox *m_segmentCombo;
  QComboBox *m_expiryCombo;
  QTableWidget *m_resultsTable;

  QVector<ContractData> m_currentResults;
};

#endif // GLOBAL_SEARCH_WIDGET_H

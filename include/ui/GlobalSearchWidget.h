#ifndef GLOBAL_SEARCH_WIDGET_H
#define GLOBAL_SEARCH_WIDGET_H

#include "repository/ContractData.h"
#include <QWidget>

class QComboBox;
class QLineEdit;
class QTableWidget;

namespace Ui { class GlobalSearchWidget; }

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
  void updateResults();
  void populateExpiries(const QString &symbol);

  Ui::GlobalSearchWidget *ui;
  QVector<ContractData> m_currentResults;
};

#endif // GLOBAL_SEARCH_WIDGET_H

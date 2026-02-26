#include "ui/StrategyManagerWindow.h"
#include "models/StrategyFilterProxyModel.h"
#include "models/StrategyTableModel.h"
#include "services/StrategyService.h"
#include "services/StrategyTemplateRepository.h"
#include "ui/CreateStrategyDialog.h"
#include "ui/ModifyParametersDialog.h"
#include "ui/StrategyDeployDialog.h"
#include "ui/StrategyTemplateBuilderDialog.h"
#include "ui/TemplateManagerDialog.h"
#include "ui_StrategyManagerWindow.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>

class StrategyStatusDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (index.column() == StrategyTableModel::COL_STATUS) {
      painter->save();
      painter->setRenderHint(QPainter::Antialiasing);

      QString status = index.data().toString();
      QColor bgColor = QColor("#64748b");
      QColor textColor = Qt::white;

      if (status == "Running")
        bgColor = QColor("#16a34a");
      else if (status == "Paused")
        bgColor = QColor("#ea580c");
      else if (status == "Stopped")
        bgColor = QColor("#dc2626");
      else if (status == "Created")
        bgColor = QColor("#2563eb");

      QRect rect = option.rect.adjusted(4, 4, -4, -4);
      painter->setBrush(bgColor);
      painter->setPen(Qt::NoPen);
      painter->drawRoundedRect(rect, 4, 4);

      painter->setPen(textColor);
      painter->drawText(rect, Qt::AlignCenter, status);
      painter->restore();
    } else {
      QStyledItemDelegate::paint(painter, option, index);
    }
  }
};

StrategyManagerWindow::StrategyManagerWindow(QWidget *parent)
    : QWidget(parent), ui(new Ui::StrategyManagerWindow),
      m_statusGroup(new QButtonGroup(this)),
      m_model(new StrategyTableModel(this)),
      m_proxy(new StrategyFilterProxyModel(this)) {
  ui->setupUi(this);
  setupUI();
  setupModels();
  setupConnections();

  StrategyService::instance().initialize();
  QVector<StrategyInstance> existing = StrategyService::instance().instances();
  m_model->setInstances(existing);
  refreshStrategyTypes();
  updateSummary();
}

StrategyManagerWindow::~StrategyManagerWindow() { delete ui; }

void StrategyManagerWindow::setupUI() {
  // Map buttons to ButtonGroup for filtering
  m_statusGroup->setExclusive(true);
  m_statusGroup->addButton(ui->filterAllBtn, 0);
  m_statusGroup->addButton(ui->filterRunningBtn, 1);
  m_statusGroup->addButton(ui->filterPausedBtn, 2);
  m_statusGroup->addButton(ui->filterStoppedBtn, 3);

  // Table Setup
  ui->tableView->setShowGrid(false);
  ui->tableView->setAlternatingRowColors(true);
  ui->tableView->setItemDelegateForColumn(StrategyTableModel::COL_STATUS,
                                          new StrategyStatusDelegate(this));
  ui->tableView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Interactive);
  ui->tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft |
                                                         Qt::AlignVCenter);
}

void StrategyManagerWindow::setupModels() {
  m_proxy->setSourceModel(m_model);
  ui->tableView->setModel(m_proxy);
}

void StrategyManagerWindow::setupConnections() {
  connect(ui->createButton, &QPushButton::clicked, this,
          &StrategyManagerWindow::onCreateClicked);
  // Both "Build Custom" and "Deploy Template" now open the unified
  // Template Manager dialog which supports create, edit, clone, delete,
  // and deploy — all in one place.
  connect(ui->buildCustomButton, &QPushButton::clicked, this,
          &StrategyManagerWindow::onManageTemplatesClicked);
  connect(ui->deployTemplateButton, &QPushButton::clicked, this,
          &StrategyManagerWindow::onManageTemplatesClicked);
  connect(ui->startButton, &QPushButton::clicked, this,
          &StrategyManagerWindow::onStartClicked);
  connect(ui->pauseButton, &QPushButton::clicked, this,
          &StrategyManagerWindow::onPauseClicked);
  connect(ui->resumeButton, &QPushButton::clicked, this,
          &StrategyManagerWindow::onResumeClicked);
  connect(ui->modifyButton, &QPushButton::clicked, this,
          &StrategyManagerWindow::onModifyClicked);
  connect(ui->stopButton, &QPushButton::clicked, this,
          &StrategyManagerWindow::onStopClicked);
  connect(ui->deleteButton, &QPushButton::clicked, this,
          &StrategyManagerWindow::onDeleteClicked);

  connect(ui->strategyFilterCombo, &QComboBox::currentTextChanged, this,
          &StrategyManagerWindow::onStrategyFilterChanged);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  connect(m_statusGroup, QOverload<int>::of(&QButtonGroup::idClicked), this,
          &StrategyManagerWindow::onStatusFilterClicked);
#else
  connect(
      m_statusGroup,
      static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
      this, &StrategyManagerWindow::onStatusFilterClicked);
#endif

  StrategyService &service = StrategyService::instance();
  connect(&service, &StrategyService::instanceAdded, this,
          &StrategyManagerWindow::onInstanceAdded);
  connect(&service, &StrategyService::instanceUpdated, this,
          &StrategyManagerWindow::onInstanceUpdated);
  connect(&service, &StrategyService::instanceRemoved, this,
          &StrategyManagerWindow::onInstanceRemoved);

  connect(m_model, &StrategyTableModel::instanceEdited, this,
          &StrategyManagerWindow::onInstanceEdited);
}

void StrategyManagerWindow::refreshStrategyTypes() {
  QSet<QString> types;
  types.insert("All");

  for (int row = 0; row < m_model->rowCount(); ++row) {
    StrategyInstance instance = m_model->instanceAt(row);
    if (!instance.strategyType.isEmpty()) {
      types.insert(instance.strategyType);
    }
  }

  QString current = ui->strategyFilterCombo->currentText();
  ui->strategyFilterCombo->clear();
  ui->strategyFilterCombo->addItems(types.values());

  int index = ui->strategyFilterCombo->findText(current);
  if (index >= 0) {
    ui->strategyFilterCombo->setCurrentIndex(index);
  }
}

StrategyInstance StrategyManagerWindow::selectedInstance(bool *ok) const {
  QModelIndex proxyIndex = ui->tableView->currentIndex();
  if (!proxyIndex.isValid()) {
    if (ok) {
      *ok = false;
    }
    return StrategyInstance();
  }

  QModelIndex sourceIndex = m_proxy->mapToSource(proxyIndex);
  StrategyInstance instance = m_model->instanceAt(sourceIndex.row());
  if (ok) {
    *ok = instance.instanceId != 0;
  }
  return instance;
}

void StrategyManagerWindow::onCreateClicked() {
  CreateStrategyDialog dialog(this);
  dialog.setStrategyTypes({"JodiATM", "Momentum", "TSpecial", "VixMonkey",
                           "RangeBreakout", "Custom"});

  // Calculate next Sr No
  int nextId = 1;
  const auto &instances = m_model->allInstances();
  for (const auto &inst : instances) {
    if (inst.instanceId >= nextId)
      nextId = inst.instanceId + 1;
  }
  dialog.setNextSrNo(nextId);

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  QVariantMap params = dialog.parameters();

  StrategyService::instance().createInstance(
      dialog.instanceName(), dialog.description(), dialog.strategyType(),
      dialog.symbol(), dialog.account(), dialog.segment(), dialog.stopLoss(),
      dialog.target(), dialog.entryPrice(), dialog.quantity(), params);
}

void StrategyManagerWindow::onBuildCustomClicked() {
    StrategyTemplateBuilderDialog dlg(this);

    if (dlg.exec() != QDialog::Accepted)
        return;

    StrategyTemplate tmpl = dlg.buildTemplate();

    // Persist the template
    StrategyTemplateRepository &repo = StrategyTemplateRepository::instance();
    if (!repo.isOpen()) {
        QMessageBox::critical(this, "Save Failed",
            "Could not open the strategy template database.\n"
            "Check application write permissions.");
        return;
    }

    if (!repo.saveTemplate(tmpl)) {
        QMessageBox::critical(this, "Save Failed",
            "Template could not be saved to the database.");
        return;
    }

    QMessageBox::information(this, "Template Saved",
        QString("Strategy template <b>%1</b> saved successfully.<br><br>"
                "You can now deploy it via <i>Deploy Template</i>.")
            .arg(tmpl.name.toHtmlEscaped()));
}

void StrategyManagerWindow::onDeployTemplateClicked()
{
    StrategyDeployDialog dlg(this);

    if (dlg.exec() != QDialog::Accepted)
        return;

    StrategyInstance inst = dlg.buildInstance();

    // Give it the next available ID
    int nextId = 1;
    for (const auto &existing : m_model->allInstances())
        if (existing.instanceId >= nextId) nextId = existing.instanceId + 1;
    // StrategyService assigns the real ID on create – we just fill visible fields

    QVariantMap params = inst.parameters;

    StrategyService::instance().createInstance(
        inst.instanceName, inst.description,
        inst.strategyType,
        inst.symbol,
        inst.account,
        inst.segment,
        inst.stopLoss,
        inst.target,
        inst.entryPrice,
        inst.quantity,
        params);

    QMessageBox::information(this, "Strategy Deployed",
        QString("Strategy <b>%1</b> has been created from template <b>%2</b>.<br>"
                "Use <i>Start</i> to activate it.")
            .arg(inst.instanceName.toHtmlEscaped())
            .arg(inst.strategyType.toHtmlEscaped()));
}

void StrategyManagerWindow::onManageTemplatesClicked()
{
    TemplateManagerDialog mgr(this);

    if (mgr.exec() != QDialog::Accepted)
        return;

    if (mgr.resultAction() == TemplateManagerDialog::Action::Deploy) {
        // User chose to deploy a template — open the deploy dialog
        // pre-seeded with the selected template
        StrategyDeployDialog dlg(this);

        if (dlg.exec() != QDialog::Accepted)
            return;

        StrategyInstance inst = dlg.buildInstance();

        QVariantMap params = inst.parameters;

        StrategyService::instance().createInstance(
            inst.instanceName, inst.description,
            inst.strategyType,
            inst.symbol,
            inst.account,
            inst.segment,
            inst.stopLoss,
            inst.target,
            inst.entryPrice,
            inst.quantity,
            params);

        QMessageBox::information(this, "Strategy Deployed",
            QString("Strategy <b>%1</b> has been created from template <b>%2</b>.<br>"
                    "Use <i>Start</i> to activate it.")
                .arg(inst.instanceName.toHtmlEscaped())
                .arg(inst.strategyType.toHtmlEscaped()));
    }
}

void StrategyManagerWindow::onLoadFromJsonClicked() {
  // Open file dialog to select JSON strategy file
  QString fileName = QFileDialog::getOpenFileName(
      this,
      "Load Strategy from JSON",
      QDir::currentPath() + "/test_strategies",
      "JSON Files (*.json);;All Files (*.*)");

  if (fileName.isEmpty()) {
    return; // User cancelled
  }

  // Read the JSON file
  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::critical(this, "Error",
                          "Failed to open file: " + file.errorString());
    return;
  }

  QByteArray jsonData = file.readAll();
  file.close();

  // Parse JSON document
  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
  if (parseError.error != QJsonParseError::NoError) {
    QMessageBox::critical(this, "JSON Parse Error",
                          "Failed to parse JSON: " + parseError.errorString());
    return;
  }

  if (!doc.isObject()) {
    QMessageBox::critical(this, "Invalid JSON",
                          "Expected JSON object at root level");
    return;
  }

  // Parse strategy definition using StrategyParser
  QString errorMsg;
  StrategyDefinition def = StrategyParser::parseJSON(doc.object(), errorMsg);
  if (!errorMsg.isEmpty()) {
    QMessageBox::critical(this, "Strategy Parse Error",
                          "Failed to parse strategy:\n" + errorMsg);
    return;
  }

  // Convert parsed definition back to JSON for storage
  QJsonDocument defDoc(StrategyParser::toJSON(def));
  QVariantMap params;
  params["definition"] = QString::fromUtf8(defDoc.toJson(QJsonDocument::Compact));
  params["productType"] = "MIS"; // Default product type

  // Create the strategy instance
  StrategyService::instance().createInstance(
      def.name.isEmpty() ? "Loaded Strategy" : def.name,  // name
      QString(),                                          // description (empty)
      "Custom",                                           // strategyType
      def.symbol.isEmpty() ? "NIFTY" : def.symbol,       // symbol
      "Test Account",                                     // account
      1,                                                  // segment (NSE F&O)
      0.0,                                                // stopLoss
      0.0,                                                // target
      0.0,                                                // entryPrice
      50,                                                 // quantity
      params);                                            // parameters

  QMessageBox::information(this, "Success",
                           "Strategy loaded successfully from JSON:\n" +
                           QFileInfo(fileName).fileName());
}

void StrategyManagerWindow::onStartClicked() {
  bool ok = false;
  StrategyInstance instance = selectedInstance(&ok);
  if (!ok) {
    return;
  }

  StrategyService::instance().startStrategy(instance.instanceId);
}

void StrategyManagerWindow::onPauseClicked() {
  bool ok = false;
  StrategyInstance instance = selectedInstance(&ok);
  if (!ok) {
    return;
  }

  StrategyService::instance().pauseStrategy(instance.instanceId);
}

void StrategyManagerWindow::onResumeClicked() {
  bool ok = false;
  StrategyInstance instance = selectedInstance(&ok);
  if (!ok) {
    return;
  }

  StrategyService::instance().resumeStrategy(instance.instanceId);
}

void StrategyManagerWindow::onModifyClicked() {
  bool ok = false;
  StrategyInstance instance = selectedInstance(&ok);
  if (!ok) {
    return;
  }

  ModifyParametersDialog dialog(this);
  dialog.setInitialValues(instance.stopLoss, instance.target,
                          instance.parameters);

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  StrategyService::instance().modifyParameters(
      instance.instanceId, dialog.parameters(), dialog.stopLoss(),
      dialog.target());
}

void StrategyManagerWindow::onStopClicked() {
  bool ok = false;
  StrategyInstance instance = selectedInstance(&ok);
  if (!ok) {
    return;
  }

  StrategyService::instance().stopStrategy(instance.instanceId);
}

void StrategyManagerWindow::onDeleteClicked() {
  bool ok = false;
  StrategyInstance instance = selectedInstance(&ok);
  if (!ok) {
    return;
  }

  if (QMessageBox::question(this, "Delete",
                            "Delete selected strategy instance?") !=
      QMessageBox::Yes) {
    return;
  }

  StrategyService::instance().deleteStrategy(instance.instanceId);
}

void StrategyManagerWindow::onStrategyFilterChanged(const QString &value) {
  m_proxy->setStrategyTypeFilter(value);
}

void StrategyManagerWindow::onStatusFilterClicked(int id) {
  switch (id) {
  case 0:
    m_proxy->clearStatusFilter();
    break;
  case 1:
    m_proxy->setStatusFilter(StrategyState::Running);
    break;
  case 2:
    m_proxy->setStatusFilter(StrategyState::Paused);
    break;
  case 3:
    m_proxy->setStatusFilter(StrategyState::Stopped);
    break;
  default:
    m_proxy->clearStatusFilter();
    break;
  }
}

void StrategyManagerWindow::onInstanceAdded(const StrategyInstance &instance) {
  m_model->addInstance(instance);
  refreshStrategyTypes();
  updateSummary();
}

void StrategyManagerWindow::onInstanceUpdated(
    const StrategyInstance &instance) {
  m_model->updateInstance(instance);
  updateSummary();
}

void StrategyManagerWindow::onInstanceRemoved(qint64 instanceId) {
  m_model->removeInstance(instanceId);
  refreshStrategyTypes();
  updateSummary();
}

void StrategyManagerWindow::onInstanceEdited(const StrategyInstance &instance) {
  StrategyService::instance().modifyParameters(
      instance.instanceId, instance.parameters, instance.stopLoss,
      instance.target);
  updateSummary();
}

void StrategyManagerWindow::updateSummary() {
  int total = m_model->rowCount();
  int running = 0;
  double totalMtm = 0.0;
  for (int i = 0; i < total; ++i) {
    StrategyInstance inst = m_model->instanceAt(i);
    if (inst.state == StrategyState::Running)
      running++;
    totalMtm += inst.mtm;
  }

  ui->SummaryLabel->setText(QString("Total: %1 | Running: %2 | Total P&L: %3")
                                .arg(total)
                                .arg(running)
                                .arg(totalMtm, 0, 'f', 2));

  if (totalMtm > 0)
    ui->SummaryLabel->setStyleSheet("color: #16a34a;");
  else if (totalMtm < 0)
    ui->SummaryLabel->setStyleSheet("color: #dc2626;");
  else
    ui->SummaryLabel->setStyleSheet("color: #64748b;");
}

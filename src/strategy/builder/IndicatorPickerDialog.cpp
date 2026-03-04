#include "strategy/builder/IndicatorPickerDialog.h"
#include "ui_IndicatorPickerDialog.h"
#include "strategy/builder/IndicatorCatalog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
IndicatorPickerDialog::IndicatorPickerDialog(const QStringList &symbolIds,
                                             int               existingCount,
                                             QWidget          *parent)
    : QDialog(parent)
    , ui(new Ui::IndicatorPickerDialog)
    , m_existingCount(existingCount)
    , m_symbolIds(symbolIds.isEmpty() ? QStringList{"REF_1"} : symbolIds)
{
    ui->setupUi(this);

    // Populate combos
    ui->symCombo->addItems(m_symbolIds);
    ui->tfCombo->addItems({"1", "3", "5", "10", "15", "30", "60", "D", "W"});
    ui->tfCombo->setCurrentText("D");

    // Splitter initial sizes
    ui->splitter->setSizes({300, 460});

    // Customize OK button
    if (auto *ok = ui->buttonBox->button(QDialogButtonBox::Ok)) {
        ok->setObjectName("okBtn");
        ok->setText("Add Indicator");
    }
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // ── Wire signals ──
    connect(ui->filterEdit, &QLineEdit::textChanged, this, &IndicatorPickerDialog::onFilterChanged);
    connect(ui->tree, &QTreeWidget::currentItemChanged,
            this, &IndicatorPickerDialog::onItemChanged);
    connect(ui->tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem *item, int) {
        if (item && item->parent()) accept();
    });

    // ── Initial tree population ──
    buildTree();
}

IndicatorPickerDialog::~IndicatorPickerDialog()
{
    delete ui;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildTree
// ─────────────────────────────────────────────────────────────────────────────
void IndicatorPickerDialog::buildTree(const QString &filter)
{
    ui->tree->clear();
    const QString lf = filter.trimmed().toLower();

    IndicatorCatalog &cat = IndicatorCatalog::instance();
    for (const QString &grp : cat.groups()) {
        QTreeWidgetItem *groupItem = nullptr;

        for (const IndicatorMeta &m : cat.forGroup(grp)) {
            bool match = lf.isEmpty()
                || m.type.toLower().contains(lf)
                || m.label.toLower().contains(lf)
                || m.description.toLower().contains(lf);
            if (!match) continue;

            if (!groupItem) {
                groupItem = new QTreeWidgetItem(ui->tree, QStringList{grp});
                QFont f = groupItem->font(0);
                f.setBold(true);
                groupItem->setFont(0, f);
                groupItem->setForeground(0, QColor("#2563eb"));
                groupItem->setExpanded(!lf.isEmpty() || grp == "Overlap Studies"
                                                     || grp == "Momentum Indicators");
            }

            auto *item = new QTreeWidgetItem(groupItem);
            item->setText(0, QString("%1  —  %2").arg(m.type, m.label));
            item->setForeground(0, QColor("#334155"));
            item->setData(0, Qt::UserRole, m.type);
            item->setToolTip(0, m.description);
        }
    }

    // Auto-select first leaf if filtering
    if (!lf.isEmpty()) {
      for (int i = 0; i < ui->tree->topLevelItemCount(); ++i) {
          QTreeWidgetItem *g = ui->tree->topLevelItem(i);
          if (g && g->childCount() > 0) {
              ui->tree->setCurrentItem(g->child(0));
                break;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// onFilterChanged
// ─────────────────────────────────────────────────────────────────────────────
void IndicatorPickerDialog::onFilterChanged(const QString &text)
{
    buildTree(text);
}

// ─────────────────────────────────────────────────────────────────────────────
// onItemChanged
// ─────────────────────────────────────────────────────────────────────────────
void IndicatorPickerDialog::onItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *)
{
    if (!current || !current->parent()) {
        // Group header selected — clear detail
      ui->descLabel->setText("Select an indicator on the left to see its description.");
      ui->paramLabel->setVisible(false);
        return;
    }

    QString type = current->data(0, Qt::UserRole).toString();
    IndicatorMeta meta;
    if (!IndicatorCatalog::instance().find(type, meta)) return;

    m_selected = meta;
    updateDescription(meta);
}

// ─────────────────────────────────────────────────────────────────────────────
// updateDescription
// ─────────────────────────────────────────────────────────────────────────────
void IndicatorPickerDialog::updateDescription(const IndicatorMeta &m)
{
    // Update group badge
    if (auto *badge = findChild<QLabel*>("groupLabel")) {
        badge->setText("📂  " + m.group);
    }

    // Description
    QString desc = QString("<b>%1</b> (%2)<br/><br/>%3")
        .arg(m.label, m.type, m.description.isEmpty() ? "—" : m.description);

    if (!m.outputs.isEmpty())
        desc += QString("<br/><span style='color:#2563eb;'>Outputs: %1</span>")
                    .arg(m.outputs.join(", "));
    if (!m.inputs.isEmpty())
        desc += QString("<br/><span style='color:#16a34a;'>Inputs: %1</span>")
                    .arg(m.inputs.join(", "));

    ui->descLabel->setText(desc);

    // Params
    if (m.paramMeta.isEmpty()) {
        ui->paramLabel->setVisible(false);
    } else {
        QStringList lines;
        for (const auto &pm : m.paramMeta) {
            QString range = (pm.type == "int")
                ? QString("[%1 … %2]  default: %3")
                      .arg((int)pm.minVal).arg((int)pm.maxVal).arg((int)pm.defVal)
                : QString("[%1 … %2]  default: %3")
                      .arg(pm.minVal, 0, 'g').arg(pm.maxVal, 0, 'g').arg(pm.defVal, 0, 'g');
            lines << QString("<b>%1</b>: %2  %3").arg(pm.key, pm.label, range);
        }
        ui->paramLabel->setText(lines.join("<br/>"));
        ui->paramLabel->setVisible(true);
    }

    // Build suggested id: TYPE_N+1 where N = how many indicators are already defined
    int n = m_existingCount + 1;
    m_suggestedId = QString("%1_%2").arg(m.type).arg(n);
    m_symbolId    = ui->symCombo->currentText();

    // Output selector — show only for multi-output indicators
    if (ui->outputCombo) {
        ui->outputCombo->clear();
        bool multiOut = m.outputs.size() > 1;
        ui->outputCombo->setEnabled(multiOut);
        if (multiOut) {
            ui->outputCombo->addItems(m.outputs);
        } else if (!m.outputs.isEmpty()) {
            ui->outputCombo->addItem(m.outputs.first());
        } else {
            ui->outputCombo->addItem("—");
        }
        // Show/hide the label next to it
        if (auto *lbl = findChild<QLabel*>("outLabel"))
            lbl->setVisible(multiOut);
        ui->outputCombo->setVisible(true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public helpers
// ─────────────────────────────────────────────────────────────────────────────
void IndicatorPickerDialog::setInitialFilter(const QString &text)
{
    if (ui->filterEdit) ui->filterEdit->setText(text);
}

// ─────────────────────────────────────────────────────────────────────────────
// accept override — capture final state
// ─────────────────────────────────────────────────────────────────────────────
void IndicatorPickerDialog::accept()
{
    if (m_selected.type.isEmpty()) return; // nothing chosen

    m_symbolId    = ui->symCombo->currentText();
    m_timeframe   = ui->tfCombo ? ui->tfCombo->currentText() : "D";
    int n         = m_existingCount + 1;
    m_suggestedId = QString("%1_%2").arg(m_selected.type).arg(n);

    // Capture output selector
    if (ui->outputCombo && ui->outputCombo->isEnabled())
        m_outputSel = ui->outputCombo->currentText();
    else if (!m_selected.outputs.isEmpty())
        m_outputSel = m_selected.outputs.first();
    else
        m_outputSel.clear();

    QDialog::accept();
}

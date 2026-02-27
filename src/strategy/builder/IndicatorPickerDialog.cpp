#include "strategy/builder/IndicatorPickerDialog.h"
#include "strategy/builder/IndicatorCatalog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

// ─────────────────────────────────────────────────────────────────────────────
// Light-theme stylesheet shared across the dialog
// ─────────────────────────────────────────────────────────────────────────────
static const char *kPickerStyle = R"(
    QDialog { background:#ffffff; color:#1e293b; }
    QSplitter::handle { background:#e2e8f0; }
    QLineEdit {
        background:#ffffff; border:1px solid #cbd5e1; border-radius:4px;
        color:#1e293b; padding:4px 8px; font-size:12px;
    }
    QLineEdit:focus { border-color:#3b82f6; }
    QTreeWidget {
        background:#ffffff; border:1px solid #e2e8f0; border-radius:4px;
        color:#1e293b; font-size:12px; outline:none;
    }
    QTreeWidget::item { padding:3px 6px; }
    QTreeWidget::item:selected { background:#dbeafe; color:#1e40af; }
    QTreeWidget::item:hover    { background:#f1f5f9; }
    QTreeWidget::branch { background:#ffffff; }
    QLabel { color:#475569; font-size:12px; }
    QLabel#titleLabel  { color:#1e293b; font-size:14px; font-weight:700; }
    QLabel#groupLabel  { color:#2563eb; font-size:11px; font-weight:700;
                          background:#eff6ff; padding:3px 8px; border-radius:3px; }
    QLabel#descLabel   { color:#334155; font-size:12px; background:#f8fafc;
                          border:1px solid #e2e8f0; border-radius:4px;
                          padding:8px 10px; }
    QLabel#paramLabel  { color:#475569; font-size:11px; background:#f8fafc;
                          border:1px solid #e2e8f0; border-radius:4px;
                          padding:6px 10px; }
    QComboBox {
        background:#ffffff; border:1px solid #cbd5e1; border-radius:4px;
        color:#1e293b; padding:4px 8px; font-size:12px; min-width:100px;
    }
    QComboBox:hover { border-color:#94a3b8; }
    QComboBox:focus { border-color:#3b82f6; }
    QComboBox::drop-down { border:none; width:18px; }
    QComboBox QAbstractItemView { background:#ffffff; color:#1e293b;
        border:1px solid #e2e8f0; selection-background-color:#dbeafe;
        selection-color:#1e40af; }
    QPushButton {
        background:#f1f5f9; color:#475569; border:1px solid #cbd5e1;
        border-radius:4px; padding:6px 18px; font-size:12px;
    }
    QPushButton:hover { background:#e2e8f0; color:#1e293b; }
    QPushButton#okBtn {
        background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #2563eb,stop:1 #1d4ed8);
        color:#fff; border:none; font-weight:700;
    }
    QPushButton#okBtn:hover { background:#1e40af; }
    QScrollArea { border:none; background:transparent; }
)";

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
IndicatorPickerDialog::IndicatorPickerDialog(const QStringList &symbolIds,
                                             int               existingCount,
                                             QWidget          *parent)
    : QDialog(parent)
    , m_existingCount(existingCount)
    , m_symbolIds(symbolIds.isEmpty() ? QStringList{"REF_1"} : symbolIds)
{
    setWindowTitle("Add Indicator");
    setMinimumSize(780, 520);
    setStyleSheet(kPickerStyle);

    auto *mainLay = new QVBoxLayout(this);
    mainLay->setSpacing(8);
    mainLay->setContentsMargins(12, 10, 12, 10);

    // ── Title ──
    auto *title = new QLabel("Select Indicator", this);
    title->setObjectName("titleLabel");
    mainLay->addWidget(title);

    // ── Search ──
    auto *searchRow = new QHBoxLayout;
    searchRow->addWidget(new QLabel("🔍 Search:", this));
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Type to filter  (e.g. RSI, Bollinger, MACD)…");
    m_filterEdit->setClearButtonEnabled(true);
    searchRow->addWidget(m_filterEdit, 1);
    mainLay->addLayout(searchRow);

    // ── Splitter: tree on left | detail panel on right ──
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(4);
    mainLay->addWidget(splitter, 1);

    // Left — tree
    m_tree = new QTreeWidget(splitter);
    m_tree->setColumnCount(1);
    m_tree->setHeaderHidden(true);
    m_tree->setMinimumWidth(280);

    // Right — detail panel
    auto *detailWidget = new QWidget(splitter);
    auto *detailLay    = new QVBoxLayout(detailWidget);
    detailLay->setContentsMargins(8, 0, 0, 0);
    detailLay->setSpacing(8);

    auto *groupBadge = new QLabel("—", detailWidget);
    groupBadge->setObjectName("groupLabel");
    groupBadge->setFixedHeight(22);

    m_descLabel = new QLabel("Select an indicator on the left to see its description.", detailWidget);
    m_descLabel->setObjectName("descLabel");
    m_descLabel->setWordWrap(true);
    m_descLabel->setMinimumHeight(60);

    m_paramLabel = new QLabel("", detailWidget);
    m_paramLabel->setObjectName("paramLabel");
    m_paramLabel->setWordWrap(true);
    m_paramLabel->setVisible(false);

    // Symbol + Timeframe row
    auto *symRow = new QHBoxLayout;
    symRow->addWidget(new QLabel("Apply to symbol:", detailWidget));
    m_symCombo = new QComboBox(detailWidget);
    m_symCombo->addItems(m_symbolIds);
    symRow->addWidget(m_symCombo);

    symRow->addSpacing(12);
    symRow->addWidget(new QLabel("Timeframe:", detailWidget));
    m_tfCombo = new QComboBox(detailWidget);
    m_tfCombo->addItems({"1", "3", "5", "10", "15", "30", "60", "D", "W"});
    m_tfCombo->setCurrentText("D");
    m_tfCombo->setToolTip("Candle interval to compute this indicator on");
    symRow->addWidget(m_tfCombo);
    symRow->addStretch();

    // Output selector — shown only when indicator has >1 output
    auto *outRow = new QHBoxLayout;
    auto *outLabel = new QLabel("Use output series:", detailWidget);
    m_outputCombo = new QComboBox(detailWidget);
    m_outputCombo->setToolTip("Pick which output series to use in conditions\n"
                               "(only relevant for multi-output indicators)");
    outRow->addWidget(outLabel);
    outRow->addWidget(m_outputCombo);
    outRow->addStretch();
    // Store references so updateDescription can show/hide
    outLabel->setObjectName("outLabel");

    detailLay->addWidget(groupBadge);
    detailLay->addWidget(m_descLabel);
    detailLay->addWidget(m_paramLabel);
    detailLay->addLayout(symRow);
    detailLay->addLayout(outRow);
    detailLay->addStretch();

    // Store groupBadge so we can update it from onItemChanged
    groupBadge->setProperty("role", "groupBadge");

    splitter->setSizes({300, 460});

    // ── Buttons ──
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (auto *ok = buttons->button(QDialogButtonBox::Ok)) {
        ok->setObjectName("okBtn");
        ok->setText("Add Indicator");
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLay->addWidget(buttons);

    // ── Wire signals ──
    connect(m_filterEdit, &QLineEdit::textChanged, this, &IndicatorPickerDialog::onFilterChanged);
    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, &IndicatorPickerDialog::onItemChanged);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem *item, int) {
        if (item && item->parent()) accept();
    });

    // ── Initial tree population ──
    buildTree();
}

// ─────────────────────────────────────────────────────────────────────────────
// buildTree
// ─────────────────────────────────────────────────────────────────────────────
void IndicatorPickerDialog::buildTree(const QString &filter)
{
    m_tree->clear();
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
                groupItem = new QTreeWidgetItem(m_tree, QStringList{grp});
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
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *g = m_tree->topLevelItem(i);
            if (g && g->childCount() > 0) {
                m_tree->setCurrentItem(g->child(0));
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
        m_descLabel->setText("Select an indicator on the left to see its description.");
        m_paramLabel->setVisible(false);
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

    m_descLabel->setText(desc);

    // Params
    if (m.paramMeta.isEmpty()) {
        m_paramLabel->setVisible(false);
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
        m_paramLabel->setText(lines.join("<br/>"));
        m_paramLabel->setVisible(true);
    }

    // Build suggested id: TYPE_N+1 where N = how many indicators are already defined
    int n = m_existingCount + 1;
    m_suggestedId = QString("%1_%2").arg(m.type).arg(n);
    m_symbolId    = m_symCombo->currentText();

    // Output selector — show only for multi-output indicators
    if (m_outputCombo) {
        m_outputCombo->clear();
        bool multiOut = m.outputs.size() > 1;
        m_outputCombo->setEnabled(multiOut);
        if (multiOut) {
            m_outputCombo->addItems(m.outputs);
        } else if (!m.outputs.isEmpty()) {
            m_outputCombo->addItem(m.outputs.first());
        } else {
            m_outputCombo->addItem("—");
        }
        // Show/hide the label next to it
        if (auto *lbl = findChild<QLabel*>("outLabel"))
            lbl->setVisible(multiOut);
        m_outputCombo->setVisible(true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public helpers
// ─────────────────────────────────────────────────────────────────────────────
void IndicatorPickerDialog::setInitialFilter(const QString &text)
{
    if (m_filterEdit) m_filterEdit->setText(text);
}

// ─────────────────────────────────────────────────────────────────────────────
// accept override — capture final state
// ─────────────────────────────────────────────────────────────────────────────
void IndicatorPickerDialog::accept()
{
    if (m_selected.type.isEmpty()) return; // nothing chosen

    m_symbolId    = m_symCombo->currentText();
    m_timeframe   = m_tfCombo ? m_tfCombo->currentText() : "D";
    int n         = m_existingCount + 1;
    m_suggestedId = QString("%1_%2").arg(m_selected.type).arg(n);

    // Capture output selector
    if (m_outputCombo && m_outputCombo->isEnabled())
        m_outputSel = m_outputCombo->currentText();
    else if (!m_selected.outputs.isEmpty())
        m_outputSel = m_selected.outputs.first();
    else
        m_outputSel.clear();

    QDialog::accept();
}

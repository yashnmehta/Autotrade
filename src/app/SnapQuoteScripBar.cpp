#include "app/SnapQuoteScripBar.h"
#include "core/widgets/CustomScripComboBox.h"
#include <QApplication>
#include <QDebug>
#include <QLineEdit>

SnapQuoteScripBar::SnapQuoteScripBar(QWidget *parent)
    : ScripBar(parent, ScripBar::DisplayMode)
{
    // SnapQuoteScripBar always starts in DisplayMode.
    // activateSearchMode() is inherited and will fire on first user interaction.
}

void SnapQuoteScripBar::focusDefault()
{
    // Focus the symbol combo so the user can type a symbol immediately.
    if (!m_symbolCombo) return;

    if (auto *le = m_symbolCombo->lineEdit()) {
        le->setFocus(Qt::OtherFocusReason);
        if (!le->text().isEmpty())
            le->selectAll();
    } else {
        m_symbolCombo->setFocus(Qt::OtherFocusReason);
    }
    qDebug() << "[SnapQuoteScripBar] Default focus → symbolCombo";
}

bool SnapQuoteScripBar::focusNextPrevChild(bool next)
{
    // Build an ordered list of all visible, enabled combos inside this bar.
    // Tab/Shift-Tab cycle within these widgets only.
    QList<QWidget *> chain;
    const QList<CustomScripComboBox *> combos = {
        m_exchangeCombo,
        m_segmentCombo,
        m_instrumentCombo,
        m_symbolCombo,
        m_expiryCombo,
        m_strikeCombo,
        m_optionTypeCombo
    };
    for (auto *c : combos) {
        if (c && c->isVisibleTo(this) && c->isEnabled())
            chain.append(c);
    }

    if (chain.isEmpty())
        return true; // Absorb Tab silently

    QWidget *current = QApplication::focusWidget();
    // focusWidget() might be the embedded QLineEdit inside a combo, so
    // also check parents.
    int idx = -1;
    for (int i = 0; i < chain.size(); ++i) {
        QWidget *w = chain[i];
        if (w == current || w->isAncestorOf(current)) {
            idx = i;
            break;
        }
    }

    if (next) {
        idx = (idx < 0 || idx >= chain.size() - 1) ? 0 : idx + 1;
    } else {
        idx = (idx <= 0) ? chain.size() - 1 : idx - 1;
    }

    QWidget *target = chain[idx];
    if (auto *combo = qobject_cast<CustomScripComboBox *>(target)) {
        if (auto *le = combo->lineEdit()) {
            le->setFocus(Qt::TabFocusReason);
            le->selectAll();
            return true;
        }
    }
    target->setFocus(Qt::TabFocusReason);
    return true; // Always consumed — focus never escapes this bar
}

#ifndef SNAPQUOTESCRIPBAR_H
#define SNAPQUOTESCRIPBAR_H

#include "app/ScripBar.h"

/**
 * @class SnapQuoteScripBar
 * @brief SnapQuote-specific ScripBar variant.
 *
 * Inherits all symbol-search / display logic from ScripBar.
 * Differences from the main-app ScripBar:
 *  - Default focus lands on m_symbolCombo (not m_exchangeCombo)
 *  - Tab / Shift-Tab cycle only through the combos inside this bar
 *    (focus never escapes to other widgets in SnapQuoteWindow)
 *  - Escape closes the popup but does not bubble further
 *
 * Add any future SnapQuote-only tweaks here without touching ScripBar.
 */
class SnapQuoteScripBar : public ScripBar
{
    Q_OBJECT

public:
    explicit SnapQuoteScripBar(QWidget *parent = nullptr);
    ~SnapQuoteScripBar() override = default;

    /**
     * @brief Set keyboard focus to the symbol combo and select all text.
     * Called by SnapQuoteWindow::showEvent() so the user can type immediately.
     */
    void focusDefault();

protected:
    /**
     * @brief Trap Tab / Shift-Tab so focus cycles only through the visible,
     *        enabled combos in this bar — never escapes to other MDI widgets.
     */
    bool focusNextPrevChild(bool next) override;
};

#endif // SNAPQUOTESCRIPBAR_H

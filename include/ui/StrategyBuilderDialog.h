#ifndef STRATEGY_BUILDER_DIALOG_H
#define STRATEGY_BUILDER_DIALOG_H

#include "strategy/StrategyDefinition.h"
#include <QDialog>
#include <QJsonObject>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTabWidget;
class QTextEdit;
class QTimeEdit;
class QVBoxLayout;

/**
 * @brief Form-based dialog for creating Custom Strategies.
 *
 * Provides a no-code interface with:
 *   - Strategy info (name, symbol, segment, timeframe, product)
 *   - Indicator configuration (add/remove with type, period)
 *   - Entry/exit condition builder (indicator/price comparisons, AND/OR)
 *   - Risk management settings (SL, target, trailing, time exit)
 *   - Live JSON preview panel
 *   - Validation before deployment
 *
 * The generated JSON is stored in StrategyInstance::parameters["definition"]
 * and executed by CustomStrategy at runtime.
 */
class StrategyBuilderDialog : public QDialog {
  Q_OBJECT

public:
  explicit StrategyBuilderDialog(QWidget *parent = nullptr);
  ~StrategyBuilderDialog() override;

  /// Get the generated strategy definition JSON string.
  QString definitionJSON() const;

  /// Get common fields for StrategyService::createInstance().
  QString strategyName() const;
  QString symbol() const;
  QString account() const;
  int segment() const;
  double stopLoss() const;
  double target() const;
  int quantity() const;
  QString productType() const;

  // ── UI: Indicator rows ──
  struct IndicatorRow {
    QWidget *container;
    QLineEdit *idEdit;
    QComboBox *typeCombo;
    QSpinBox *periodSpin;
    QSpinBox *period2Spin;  // For MACD signal period, BB std dev
  };

  // ── UI: Condition rows ──
  struct ConditionRow {
    QWidget *container;
    QComboBox *typeCombo;     // Indicator, Price, PriceVsIndicator  OR  CombinedPremium, LegPremium, etc.
    QComboBox *indicatorCombo;// Indicator mode: indicator ID | Options mode: leg ID
    QComboBox *operatorCombo; // >, <, >=, <=, ==, !=
    QLineEdit *valueEdit;     // Numeric value or indicator ID
  };

  // ── UI: Option Leg rows ──
  struct LegRow {
    QWidget *container;
    QLineEdit *legIdEdit;
    QLineEdit *symbolEdit;      // Per-leg symbol override (optional)
    QComboBox *sideCombo;       // BUY, SELL
    QComboBox *optionTypeCombo; // CE, PE, FUT
    QComboBox *strikeModCombo;  // ATM Relative, Premium Based, Fixed
    QSpinBox *atmOffsetSpin;
    QDoubleSpinBox *premiumSpin;
    QSpinBox *fixedStrikeSpin;
    QStackedWidget *strikeParamStack;
    QComboBox *expiryCombo;
    QSpinBox *qtySpin;
  };

  // ── UI: Multi-Symbol rows ──
  struct SymbolRow {
    QWidget *container;
    QLineEdit *symbolIdEdit;    // Unique ID: "SYM_1", "SYM_2"
    QLineEdit *symbolEdit;      // Actual symbol: NIFTY, RELIANCE
    QComboBox *segmentCombo;    // NSE CM/FO, BSE CM/FO
    QSpinBox *qtySpin;
    QDoubleSpinBox *weightSpin; // For weighted strategies
  };

private slots:
  void addIndicator();
  void removeIndicator();
  void addLeg();
  void removeLeg();
  void addSymbol();
  void removeSymbol();
  void addEntryCondition();
  void removeEntryCondition();
  void addExitCondition();
  void removeExitCondition();

  void onModeChanged(int index);
  void updateJSONPreview();
  void onValidateClicked();

protected:
  void accept() override;

private:
  void setupUI();
  QWidget *createInfoSection();
  QWidget *createIndicatorSection();
  QWidget *createLegsSection();
  QWidget *createSymbolsSection();
  QWidget *createConditionsSection(const QString &title, bool isEntry);
  QWidget *createRiskSection();
  QWidget *createPreviewSection();

  QJsonObject buildJSON() const;
  QJsonObject buildConditionGroupJSON(bool isEntry) const;
  QString generateValidationErrors() const;
  QStringList conditionTypesForMode() const;
  void refreshConditionCombos();

  // ── Strategy info ──
  QComboBox *m_modeCombo = nullptr;    // Indicator-Based / Options Strategy
  QLineEdit *m_nameEdit = nullptr;
  QLineEdit *m_symbolEdit = nullptr;
  QLineEdit *m_accountEdit = nullptr;
  QComboBox *m_segmentCombo = nullptr;
  QComboBox *m_timeframeCombo = nullptr;
  QComboBox *m_productCombo = nullptr;

  // ── Mode-dependent sections ──
  QWidget *m_indicatorSection = nullptr;
  QWidget *m_legsSection = nullptr;
  QWidget *m_symbolsSection = nullptr;

  // ── Indicators ──
  QVBoxLayout *m_indicatorLayout = nullptr;
  QVector<IndicatorRow> m_indicators;

  // ── Option Legs ──
  QVBoxLayout *m_legsLayout = nullptr;
  QVector<LegRow> m_legs;
  QSpinBox *m_atmRecalcPeriodSpin = nullptr; // ATM recalc interval (sec)

  // ── Multi-Symbol ──
  QVBoxLayout *m_symbolsLayout = nullptr;
  QVector<SymbolRow> m_symbols;

  // ── Entry conditions ──
  QVBoxLayout *m_entryLayout = nullptr;
  QComboBox *m_entryLogicCombo = nullptr;  // AND / OR
  QVector<ConditionRow> m_entryConditions;

  // ── Exit conditions ──
  QVBoxLayout *m_exitLayout = nullptr;
  QComboBox *m_exitLogicCombo = nullptr;
  QVector<ConditionRow> m_exitConditions;

  // ── Risk management ──
  QDoubleSpinBox *m_slSpin = nullptr;
  QDoubleSpinBox *m_targetSpin = nullptr;
  QSpinBox *m_positionSizeSpin = nullptr;
  QSpinBox *m_maxPositionsSpin = nullptr;
  QSpinBox *m_maxDailyTradesSpin = nullptr;
  QDoubleSpinBox *m_maxDailyLossSpin = nullptr;
  QCheckBox *m_trailingCheck = nullptr;
  QDoubleSpinBox *m_trailingTriggerSpin = nullptr;
  QDoubleSpinBox *m_trailingAmountSpin = nullptr;
  QCheckBox *m_timeExitCheck = nullptr;
  QTimeEdit *m_exitTimeEdit = nullptr;

  // ── Preview ──
  QTextEdit *m_jsonPreview = nullptr;
  QLabel *m_validationLabel = nullptr;
};

#endif // STRATEGY_BUILDER_DIALOG_H

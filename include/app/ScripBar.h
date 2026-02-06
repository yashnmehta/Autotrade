#ifndef SCRIPBAR_H
#define SCRIPBAR_H

#include "core/widgets/CustomScripComboBox.h"
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QVector>
#include <QWidget>

class XTSMarketDataClient;

struct InstrumentData {
  int64_t exchangeInstrumentID;
  QString name;
  QString symbol;
  QString series;
  QString instrumentType;
  QString expiryDate;
  double strikePrice;
  QString optionType;
  int exchangeSegment;
  QString scripCode; // BSE scrip code (BSE only)
};

class ScripBar : public QWidget {
  Q_OBJECT

public:
  // ⚡ DisplayMode: Skip expensive populateSymbols(), just display token's data
  // via O(1) lookup SearchMode: Full population for user interaction (default)
  enum ScripBarMode {
    SearchMode, // Full symbol dropdown population (200-400ms)
    DisplayMode // Direct O(1) display, no population (<1ms)
  };

  explicit ScripBar(QWidget *parent = nullptr, ScripBarMode mode = SearchMode);
  ~ScripBar();

  // Set XTS client for instrument search
  void setXTSClient(XTSMarketDataClient *client);

  // ⚡ Set mode: DisplayMode skips populateSymbols() for instant
  // setScripDetails()
  void setScripBarMode(ScripBarMode mode);
  ScripBarMode scripBarMode() const { return m_mode; }

  // Get currently selected instrument data
  InstrumentData getCurrentInstrument() const;

  // Refresh symbols when repository loads
  void refreshSymbols();

  // Focus on the input field
  void focusInput();

  // Set the scrip details programmatically
  // ⚡ In DisplayMode: O(1) token lookup, no cache rebuild
  // In SearchMode: Full population (original behavior)
  void setScripDetails(const InstrumentData &data);

signals:
  void addToWatchRequested(const InstrumentData &instrument);
  void scripBarEscapePressed();

private slots:
  void onExchangeChanged(const QString &text);
  void onSegmentChanged(const QString &text);
  void onInstrumentChanged(const QString &text = QString());
  void onSymbolChanged(const QString &text);
  void onBseScripCodeChanged(const QString &text); // BSE scrip code search
  void onExpiryChanged(const QString &text = QString());
  void onStrikeChanged(const QString &text = QString());
  void onOptionTypeChanged(const QString &text = QString());
  void onAddToWatchClicked();

private:
  void setupUI();
  void populateExchanges();
  void populateSegments(const QString &exchange);
  void populateInstruments(const QString &segment);
  void populateSymbols(const QString &instrument);
  void populateBseScripCodes(); // Populate BSE scrip codes for search
  void populateExpiries(const QString &symbol);
  void populateStrikes(const QString &expiry);
  void populateOptionTypes(const QString &strike);
  void updateBseScripCodeVisibility();
  void updateTokenDisplay();

  QString getCurrentExchange() const;
  QString getCurrentSegment() const;
  // ✅ Removed: getCurrentExchangeSegmentCode() - use
  // RepositoryManager::getExchangeSegmentID() ✅ Removed:
  // mapInstrumentToSeries() - use RepositoryManager::mapInstrumentToSeries()

  // Async search from XTS (future implementation)
  void searchInstrumentsAsync(const QString &searchText);
  void onInstrumentsReceived(const QVector<InstrumentData> &instruments);

  QHBoxLayout *m_layout;
  XTSMarketDataClient *m_xtsClient;

  // Cache for instrument data
  QVector<InstrumentData> m_instrumentCache;
  QVector<InstrumentData> m_filteredInstruments;

  CustomScripComboBox *m_exchangeCombo;
  CustomScripComboBox *m_segmentCombo;
  CustomScripComboBox *m_instrumentCombo;
  CustomScripComboBox
      *m_bseScripCodeCombo; // BSE scrip code search (BSE + E only)
  CustomScripComboBox *m_symbolCombo;
  CustomScripComboBox *m_expiryCombo;
  CustomScripComboBox *m_strikeCombo;
  CustomScripComboBox *m_optionTypeCombo;
  QLineEdit *m_tokenEdit; // Shows selected token

  // Current selection state
  QString m_currentExchange;
  QString m_currentSegment;
  ScripBarMode m_mode =
      SearchMode; // ⚡ Default to SearchMode for backward compatibility
  InstrumentData m_displayData; // ⚡ Cached data for DisplayMode

  QPushButton *m_addToWatchButton;

  // ⚡ DisplayMode helper: Display single contract via O(1) lookup, no cache
  // rebuild
  void displaySingleContract(const InstrumentData &data);
};

#endif // SCRIPBAR_H
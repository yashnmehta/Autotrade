#ifndef SCRIPBAR_H
#define SCRIPBAR_H

#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMap>
#include <QVector>
#include "core/widgets/CustomScripComboBox.h"

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
    QString scripCode;  // BSE scrip code (BSE only)
};

class ScripBar : public QWidget
{
    Q_OBJECT

public:
    explicit ScripBar(QWidget *parent = nullptr);
    ~ScripBar();

    // Set XTS client for instrument search
    void setXTSClient(XTSMarketDataClient *client);

signals:
    void addToWatchRequested(const QString &exchange, const QString &segment,
                             const QString &instrument, const QString &symbol,
                             const QString &expiry, const QString &strike,
                             const QString &optionType);

private slots:
    void onExchangeChanged(const QString &text);
    void onSegmentChanged(const QString &text);
    void onInstrumentChanged(int index);
    void onSymbolChanged(const QString &text);
    void onExpiryChanged(int index);
    void onStrikeChanged(int index);
    void onOptionTypeChanged(int index);
    void onAddToWatchClicked();

private:
    void setupUI();
    void populateExchanges();
    void populateSegments(const QString &exchange);
    void populateInstruments(const QString &segment);
    void populateSymbols(const QString &instrument);
    void populateExpiries(const QString &symbol);
    void populateStrikes(const QString &expiry);
    void populateOptionTypes(const QString &strike);
    void setupShortcuts();
    void updateBseScripCodeVisibility();
    void updateTokenDisplay();
    
    QString getCurrentExchange() const;
    QString getCurrentSegment() const;
    int getCurrentExchangeSegmentCode() const;
    
    // Async search from XTS (future implementation)
    void searchInstrumentsAsync(const QString &searchText);
    void onInstrumentsReceived(const QVector<InstrumentData> &instruments);

    QHBoxLayout *m_layout;
    XTSMarketDataClient *m_xtsClient;
    
    // Cache for instrument data
    QVector<InstrumentData> m_instrumentCache;
    QVector<InstrumentData> m_filteredInstruments;
    
    // Custom combo boxes (using existing CustomScripComboBox)
    CustomScripComboBox *m_exchangeCombo;
    CustomScripComboBox *m_segmentCombo;
    CustomScripComboBox *m_instrumentCombo;
    CustomScripComboBox *m_bseScripCodeCombo;  // Only visible for BSE + E segment
    CustomScripComboBox *m_symbolCombo;
    CustomScripComboBox *m_expiryCombo;
    CustomScripComboBox *m_strikeCombo;
    CustomScripComboBox *m_optionTypeCombo;
    CustomScripComboBox *m_tokenCombo;  // Shows selected token

    // Current selection state
    QString m_currentExchange;
    QString m_currentSegment;

    QPushButton *m_addToWatchButton;
};

#endif // SCRIPBAR_H
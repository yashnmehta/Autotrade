#ifndef SCRIPBAR_H
#define SCRIPBAR_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMenu>
#include "ui/CustomScripComboBox.h"

class ScripBar : public QWidget
{
    Q_OBJECT

public:
    explicit ScripBar(QWidget *parent = nullptr);
    ~ScripBar();

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
    void setupShortcuts();
    void populateExchanges();
    void populateSegments(const QString &exchange);
    void populateInstruments(const QString &segment);
    void populateSymbols(const QString &instrument);
    void populateExpiries(const QString &symbol);
    void populateStrikes(const QString &expiry);
    void populateOptionTypes(const QString &strike);

    QHBoxLayout *m_layout;

    CustomScripComboBox *m_exchangeCombo;
    CustomScripComboBox *m_segmentCombo;
    CustomScripComboBox *m_instrumentCombo;
    CustomScripComboBox *m_symbolCombo;
    CustomScripComboBox *m_expiryCombo;
    CustomScripComboBox *m_strikeCombo;
    CustomScripComboBox *m_optionTypeCombo;

    // compact toolbuttons to save horizontal space
    QToolButton *m_exchangeBtn;
    QToolButton *m_segmentBtn;
    QMenu *m_exchangeMenu;
    QMenu *m_segmentMenu;

    QString m_currentExchange;
    QString m_currentSegment;

    QPushButton *m_addToWatchButton;
};

#endif // SCRIPBAR_H
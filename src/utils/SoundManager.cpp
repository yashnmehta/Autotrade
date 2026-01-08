#include "utils/SoundManager.h"
#include "utils/PreferencesManager.h"
#include <QApplication>

SoundManager::SoundManager()
    : m_prefsManager(&PreferencesManager::instance())
{
}

SoundManager& SoundManager::instance()
{
    static SoundManager instance;
    return instance;
}

QString SoundManager::eventTypeToString(EventType eventType)
{
    switch (eventType) {
        case EventType::OrderError: return "Order Error";
        case EventType::OrderConfirmation: return "Order Confirmation";
        case EventType::TradeConfirmation: return "Trade Confirmation";
        case EventType::StopLossTriggers: return "stop loss triggers";
        case EventType::FrozenOrders: return "frozen orders";
        case EventType::SurveillanceAlert: return "servellance alert";
        case EventType::NSEEQLogon: return "NSE EQ logon";
        case EventType::NSEEQLogoff: return "NSE EQ Logoff";
        case EventType::NSEDervLogon: return "NSE derv logon";
        case EventType::NSEDervLogoff: return "NSE derv logoff";
        case EventType::BSEEQLogon: return "BSE EQ logon";
        case EventType::BSEEQLogoff: return "BSE EQ Logoff";
        case EventType::BSEDervLogon: return "BSE derv logon";
        case EventType::BSEDervLogoff: return "BSE derv logoff";
        case EventType::MCXCommLogon: return "MCX comm log onn";
        case EventType::MCXCommLogoff: return "MCX comm log off";
        case EventType::NSECDSCurrLogon: return "NSECDS Curr Logon";
        case EventType::NSECDSCurrLogoff: return "NSECDS Curr Logoff";
        case EventType::BSEOTSEQLogon: return "BSE OTS EQ Log on";
        case EventType::BSEOTSEQLogoff: return "BSE OTS EQ Log off";
        case EventType::BSECDSCurrLogon: return "BSECDS Curr Logon";
        case EventType::BSECDSCurrLogoff: return "BSECDS Curr Logoff";
        case EventType::BuyWindowOpen: return "By windowe Open";
        default: return "";
    }
}

SoundManager::EventType SoundManager::stringToEventType(const QString &eventString)
{
    if (eventString == "Order Error") return EventType::OrderError;
    if (eventString == "Order Confirmation") return EventType::OrderConfirmation;
    if (eventString == "Trade Confirmation") return EventType::TradeConfirmation;
    if (eventString == "stop loss triggers") return EventType::StopLossTriggers;
    if (eventString == "frozen orders") return EventType::FrozenOrders;
    if (eventString == "servellance alert") return EventType::SurveillanceAlert;
    if (eventString == "NSE EQ logon") return EventType::NSEEQLogon;
    if (eventString == "NSE EQ Logoff") return EventType::NSEEQLogoff;
    if (eventString == "NSE derv logon") return EventType::NSEDervLogon;
    if (eventString == "NSE derv logoff") return EventType::NSEDervLogoff;
    if (eventString == "BSE EQ logon") return EventType::BSEEQLogon;
    if (eventString == "BSE EQ Logoff") return EventType::BSEEQLogoff;
    if (eventString == "BSE derv logon") return EventType::BSEDervLogon;
    if (eventString == "BSE derv logoff") return EventType::BSEDervLogoff;
    if (eventString == "MCX comm log onn") return EventType::MCXCommLogon;
    if (eventString == "MCX comm log off") return EventType::MCXCommLogoff;
    if (eventString == "NSECDS Curr Logon") return EventType::NSECDSCurrLogon;
    if (eventString == "NSECDS Curr Logoff") return EventType::NSECDSCurrLogoff;
    if (eventString == "BSE OTS EQ Log on") return EventType::BSEOTSEQLogon;
    if (eventString == "BSE OTS EQ Log off") return EventType::BSEOTSEQLogoff;
    if (eventString == "BSECDS Curr Logon") return EventType::BSECDSCurrLogon;
    if (eventString == "BSECDS Curr Logoff") return EventType::BSECDSCurrLogoff;
    if (eventString == "By windowe Open") return EventType::BuyWindowOpen;
    
    return EventType::OrderError; // Default
}

void SoundManager::playBeepForEvent(EventType eventType)
{
    if (isBeepEnabledForEvent(eventType)) {
        playSystemBeep();
    }
}

void SoundManager::playSoundForEvent(EventType eventType)
{
    QString eventString = eventTypeToString(eventType);
    QString key = QString("General/Event/%1/PlaySound").arg(eventString);
    bool playSoundEnabled = m_prefsManager->value(key, false).toBool();
    
    if (playSoundEnabled) {
        QString soundPathKey = QString("General/Event/%1/SoundPath").arg(eventString);
        QString soundPath = m_prefsManager->value(soundPathKey, "").toString();
        
        if (!soundPath.isEmpty()) {
            playCustomSound(soundPath);
        }
    }
}

bool SoundManager::isBeepEnabledForEvent(EventType eventType) const
{
    QString eventString = eventTypeToString(eventType);
    QString key = QString("General/Event/%1/Beep").arg(eventString);
    return m_prefsManager->value(key, false).toBool();
}

bool SoundManager::isFlashMessageEnabledForEvent(EventType eventType) const
{
    QString eventString = eventTypeToString(eventType);
    QString key = QString("General/Event/%1/FlashMessage").arg(eventString);
    return m_prefsManager->value(key, false).toBool();
}

bool SoundManager::isStaticMessageEnabledForEvent(EventType eventType) const
{
    QString eventString = eventTypeToString(eventType);
    QString key = QString("General/Event/%1/StaticMessage").arg(eventString);
    return m_prefsManager->value(key, false).toBool();
}

void SoundManager::playSystemBeep()
{
    // Play system beep
    QApplication::beep();
}

void SoundManager::playCustomSound(const QString &filePath)
{
    // Note: QSound is not available in Qt5 base
    // For custom sound files, you need to add QtMultimedia module
    // For now, fall back to system beep
    Q_UNUSED(filePath);
    playSystemBeep();
}

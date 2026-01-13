#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <QObject>
#include <QString>

class PreferencesManager;

/**
 * @brief Manages system beep and sound notifications
 * 
 * This class provides functionality to:
 * - Play system beep sounds based on preferences
 * - Play custom sound files for events
 * - Check if beep is enabled for specific events
 */
class SoundManager : public QObject
{
    Q_OBJECT

public:
    // Singleton instance
    static SoundManager& instance();
    
    /**
     * @brief Event types for which beep/sound can be configured
     */
    enum class EventType {
        OrderError,
        OrderConfirmation,
        TradeConfirmation,
        StopLossTriggers,
        FrozenOrders,
        SurveillanceAlert,
        NSEEQLogon,
        NSEEQLogoff,
        NSEDervLogon,
        NSEDervLogoff,
        BSEEQLogon,
        BSEEQLogoff,
        BSEDervLogon,
        BSEDervLogoff,
        MCXCommLogon,
        MCXCommLogoff,
        NSECDSCurrLogon,
        NSECDSCurrLogoff,
        BSEOTSEQLogon,
        BSEOTSEQLogoff,
        BSECDSCurrLogon,
        BSECDSCurrLogoff,
        BuyWindowOpen  // NEW: Buy window open event
    };
    
    /**
     * @brief Convert EventType to string (matches UI combo box values)
     */
    static QString eventTypeToString(EventType eventType);
    
    /**
     * @brief Convert string to EventType
     */
    static EventType stringToEventType(const QString &eventString);
    
    /**
     * @brief Play beep for a specific event if enabled in preferences
     * @param eventType The event type to check and play beep for
     */
    void playBeepForEvent(EventType eventType);
    
    /**
     * @brief Play custom sound file for a specific event if enabled in preferences
     * @param eventType The event type to check and play sound for
     */
    void playSoundForEvent(EventType eventType);
    
    /**
     * @brief Check if beep is enabled for a specific event
     * @param eventType The event type to check
     * @return true if beep is enabled, false otherwise
     */
    bool isBeepEnabledForEvent(EventType eventType) const;
    
    /**
     * @brief Check if flash message is enabled for a specific event
     * @param eventType The event type to check
     * @return true if flash message is enabled, false otherwise
     */
    bool isFlashMessageEnabledForEvent(EventType eventType) const;
    
    /**
     * @brief Check if static message box is enabled for a specific event
     * @param eventType The event type to check
     * @return true if static message box is enabled, false otherwise
     */
    bool isStaticMessageEnabledForEvent(EventType eventType) const;
    
    /**
     * @brief Play system beep sound
     */
    void playSystemBeep();
    
    /**
     * @brief Play a custom sound file
     * @param filePath Path to the sound file
     */
    void playCustomSound(const QString &filePath);

signals:
    /**
     * @brief Emitted when a beep should trigger a flash message
     */
    void flashMessageRequested(const QString &message);
    
    /**
     * @brief Emitted when a static message box should be shown
     */
    void staticMessageRequested(const QString &message);

private:
    SoundManager();
    ~SoundManager() = default;
    
    // Prevent copying
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;
    
    PreferencesManager *m_prefsManager;
};

#endif // SOUNDMANAGER_H

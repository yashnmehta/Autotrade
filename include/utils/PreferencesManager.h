#ifndef PREFERENCESMANAGER_H
#define PREFERENCESMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariant>

/**
 * @brief Manages user preferences for trading windows
 * 
 * Stores and retrieves default values for:
 * - Order types (Market, Limit, SL, SL-M)
 * - Product types (Intraday, Delivery, etc.)
 * - Validity (Day, IOC)
 * - Default quantities
 * - Price offsets for limit orders
 * - UI preferences
 */
class PreferencesManager : public QObject
{
    Q_OBJECT
    
public:
    // Singleton instance
    static PreferencesManager& instance();
    
    // Trading preferences
    QString getDefaultOrderType() const;
    void setDefaultOrderType(const QString &type);
    
    QString getDefaultProduct() const;
    void setDefaultProduct(const QString &product);
    
    QString getDefaultValidity() const;
    void setDefaultValidity(const QString &validity);
    
    int getDefaultQuantity(const QString &segment) const;
    void setDefaultQuantity(const QString &segment, int qty);
    
    // Price offset preferences (for limit orders)
    double getBuyPriceOffset() const;  // e.g., +0.05 from ask
    void setBuyPriceOffset(double offset);
    
    double getSellPriceOffset() const;  // e.g., -0.05 from bid
    void setSellPriceOffset(double offset);
    
    // Auto-fill preferences
    bool getAutoFillQuantity() const;
    void setAutoFillQuantity(bool enabled);
    
    bool getAutoFillPrice() const;
    void setAutoFillPrice(bool enabled);
    
    bool getAutoCalculatePrice() const;  // Auto-calculate limit price from market price
    void setAutoCalculatePrice(bool enabled);
    
    // UI preferences
    bool getConfirmOrders() const;
    void setConfirmOrders(bool enabled);
    
    bool getShowOrderConfirmation() const;
    void setShowOrderConfirmation(bool enabled);
    
    // PriceCache mode preference
    bool getUseLegacyPriceCache() const;
    void setUseLegacyPriceCache(bool useLegacy);
    
    // ============================================================================
    // NEW: Order Window Focus Field (default: Quantity)
    // ============================================================================
    enum class FocusField { Quantity, Price, Scrip };
    FocusField getOrderWindowFocusField() const;
    
    void setOrderWindowFocusField(FocusField field);
    QString focusFieldToString(FocusField field) const;
    FocusField stringToFocusField(const QString& str) const;
    
    // ============================================================================
    // NEW: Default Workspace (loads on login)
    // ============================================================================
    QString getDefaultWorkspace() const;
    void setDefaultWorkspace(const QString& workspaceName);
    
    // ============================================================================
    // NEW: Order Book Default Filter (default: Pending)
    // ============================================================================
    QString getOrderBookDefaultFilter() const;  // "All", "Pending", "Executed", "Cancelled"
    void setOrderBookDefaultFilter(const QString& filter);
    
    // ============================================================================
    // NEW: Position Book Default View (default: Net wise)
    // ============================================================================
    QString getPositionBookDefaultView() const;  // "Net", "DayWise"
    void setPositionBookDefaultView(const QString& view);
    
    // Window-specific preferences
    QVariant getWindowPreference(const QString &window, const QString &key, 
                                 const QVariant &defaultValue = QVariant()) const;
    void setWindowPreference(const QString &window, const QString &key, const QVariant &value);
    
    // Generic settings access
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);
    void clear();
    
    // Quick presets
    struct TradingPreset {
        QString orderType;
        QString product;
        QString validity;
        int quantity;
    };
    
    TradingPreset getPreset(const QString &name) const;
    void savePreset(const QString &name, const TradingPreset &preset);
    QStringList getPresetNames() const;
    
signals:
    void preferencesChanged(const QString &key);
    
private:
    PreferencesManager();
    ~PreferencesManager() = default;
    
    // Prevent copying
    PreferencesManager(const PreferencesManager&) = delete;
    PreferencesManager& operator=(const PreferencesManager&) = delete;
    
    QSettings m_settings;
    
    // Default values
    static const QString DEFAULT_ORDER_TYPE;
    static const QString DEFAULT_PRODUCT;
    static const QString DEFAULT_VALIDITY;
    static const int DEFAULT_QUANTITY;
};

#endif // PREFERENCESMANAGER_H

#include "utils/PreferencesManager.h"
#include <QDebug>

// Default values
const QString PreferencesManager::DEFAULT_ORDER_TYPE = "LIMIT";
const QString PreferencesManager::DEFAULT_PRODUCT = "MIS";  // Intraday
const QString PreferencesManager::DEFAULT_VALIDITY = "DAY";
const int PreferencesManager::DEFAULT_QUANTITY = 1;

PreferencesManager& PreferencesManager::instance()
{
    static PreferencesManager instance;
    return instance;
}

PreferencesManager::PreferencesManager()
    : m_settings("TradingCompany", "TradingTerminal")
{
    qDebug() << "[PreferencesManager] Initialized with config:" << m_settings.fileName();
}

// ============================================================================
// Trading Preferences
// ============================================================================

QString PreferencesManager::getDefaultOrderType() const
{
    return m_settings.value("trading/order_type", DEFAULT_ORDER_TYPE).toString();
}

void PreferencesManager::setDefaultOrderType(const QString &type)
{
    m_settings.setValue("trading/order_type", type);
    emit preferencesChanged("trading/order_type");
}

QString PreferencesManager::getDefaultProduct() const
{
    return m_settings.value("trading/product", DEFAULT_PRODUCT).toString();
}

void PreferencesManager::setDefaultProduct(const QString &product)
{
    m_settings.setValue("trading/product", product);
    emit preferencesChanged("trading/product");
}

QString PreferencesManager::getDefaultValidity() const
{
    return m_settings.value("trading/validity", DEFAULT_VALIDITY).toString();
}

void PreferencesManager::setDefaultValidity(const QString &validity)
{
    m_settings.setValue("trading/validity", validity);
    emit preferencesChanged("trading/validity");
}

int PreferencesManager::getDefaultQuantity(const QString &segment) const
{
    QString key = QString("trading/quantity_%1").arg(segment.toLower());
    return m_settings.value(key, DEFAULT_QUANTITY).toInt();
}

void PreferencesManager::setDefaultQuantity(const QString &segment, int qty)
{
    QString key = QString("trading/quantity_%1").arg(segment.toLower());
    m_settings.setValue(key, qty);
    emit preferencesChanged(key);
}

// ============================================================================
// Price Offset Preferences
// ============================================================================

double PreferencesManager::getBuyPriceOffset() const
{
    return m_settings.value("trading/buy_price_offset", 0.05).toDouble();
}

void PreferencesManager::setBuyPriceOffset(double offset)
{
    m_settings.setValue("trading/buy_price_offset", offset);
    emit preferencesChanged("trading/buy_price_offset");
}

double PreferencesManager::getSellPriceOffset() const
{
    return m_settings.value("trading/sell_price_offset", -0.05).toDouble();
}

void PreferencesManager::setSellPriceOffset(double offset)
{
    m_settings.setValue("trading/sell_price_offset", offset);
    emit preferencesChanged("trading/sell_price_offset");
}

// ============================================================================
// Auto-fill Preferences
// ============================================================================

bool PreferencesManager::getAutoFillQuantity() const
{
    return m_settings.value("trading/autofill_quantity", true).toBool();
}

void PreferencesManager::setAutoFillQuantity(bool enabled)
{
    m_settings.setValue("trading/autofill_quantity", enabled);
    emit preferencesChanged("trading/autofill_quantity");
}

bool PreferencesManager::getAutoFillPrice() const
{
    return m_settings.value("trading/autofill_price", true).toBool();
}

void PreferencesManager::setAutoFillPrice(bool enabled)
{
    m_settings.setValue("trading/autofill_price", enabled);
    emit preferencesChanged("trading/autofill_price");
}

bool PreferencesManager::getAutoCalculatePrice() const
{
    return m_settings.value("trading/auto_calculate_price", true).toBool();
}

void PreferencesManager::setAutoCalculatePrice(bool enabled)
{
    m_settings.setValue("trading/auto_calculate_price", enabled);
    emit preferencesChanged("trading/auto_calculate_price");
}

// ============================================================================
// UI Preferences
// ============================================================================

bool PreferencesManager::getConfirmOrders() const
{
    return m_settings.value("ui/confirm_orders", true).toBool();
}

void PreferencesManager::setConfirmOrders(bool enabled)
{
    m_settings.setValue("ui/confirm_orders", enabled);
    emit preferencesChanged("ui/confirm_orders");
}

bool PreferencesManager::getShowOrderConfirmation() const
{
    return m_settings.value("ui/show_order_confirmation", true).toBool();
}

void PreferencesManager::setShowOrderConfirmation(bool enabled)
{
    m_settings.setValue("ui/show_order_confirmation", enabled);
    emit preferencesChanged("ui/show_order_confirmation");
}

// ============================================================================
// PriceCache Mode Preference
// ============================================================================

bool PreferencesManager::getUseLegacyPriceCache() const
{
    // Default to true (legacy/current implementation) for safety
    // Set to false to enable new zero-copy architecture
    return m_settings.value("pricecache/use_legacy_mode", true).toBool();
}

void PreferencesManager::setUseLegacyPriceCache(bool useLegacy)
{
    m_settings.setValue("pricecache/use_legacy_mode", useLegacy);
    qDebug() << "[PreferencesManager] PriceCache mode changed to:" 
             << (useLegacy ? "LEGACY (current)" : "NEW (zero-copy)");
    emit preferencesChanged("pricecache/use_legacy_mode");
}

// ============================================================================
// Window-specific Preferences
// ============================================================================

QVariant PreferencesManager::getWindowPreference(const QString &window, 
                                                 const QString &key,
                                                 const QVariant &defaultValue) const
{
    QString fullKey = QString("windows/%1/%2").arg(window).arg(key);
    return m_settings.value(fullKey, defaultValue);
}

void PreferencesManager::setWindowPreference(const QString &window, 
                                            const QString &key, 
                                            const QVariant &value)
{
    QString fullKey = QString("windows/%1/%2").arg(window).arg(key);
    m_settings.setValue(fullKey, value);
    emit preferencesChanged(fullKey);
}

// ============================================================================
// Quick Presets
// ============================================================================

PreferencesManager::TradingPreset PreferencesManager::getPreset(const QString &name) const
{
    TradingPreset preset;
    QString prefix = QString("presets/%1/").arg(name);
    
    preset.orderType = m_settings.value(prefix + "order_type", DEFAULT_ORDER_TYPE).toString();
    preset.product = m_settings.value(prefix + "product", DEFAULT_PRODUCT).toString();
    preset.validity = m_settings.value(prefix + "validity", DEFAULT_VALIDITY).toString();
    preset.quantity = m_settings.value(prefix + "quantity", DEFAULT_QUANTITY).toInt();
    
    return preset;
}

// ============================================================================
// Generic Settings Access
// ============================================================================

QVariant PreferencesManager::value(const QString &key, const QVariant &defaultValue) const
{
    return m_settings.value(key, defaultValue);
}

void PreferencesManager::setValue(const QString &key, const QVariant &value)
{
    m_settings.setValue(key, value);
    emit preferencesChanged(key);
}

void PreferencesManager::clear()
{
    m_settings.clear();
    emit preferencesChanged("*");
}

void PreferencesManager::savePreset(const QString &name, const TradingPreset &preset)
{
    QString prefix = QString("presets/%1/").arg(name);
    
    m_settings.setValue(prefix + "order_type", preset.orderType);
    m_settings.setValue(prefix + "product", preset.product);
    m_settings.setValue(prefix + "validity", preset.validity);
    m_settings.setValue(prefix + "quantity", preset.quantity);
    
    qDebug() << "[PreferencesManager] Saved preset:" << name;
    emit preferencesChanged("presets");
}

QStringList PreferencesManager::getPresetNames() const
{
    QSettings &settings = const_cast<QSettings&>(m_settings);
    settings.beginGroup("presets");
    QStringList presets = settings.childGroups();
    settings.endGroup();
    
    return presets;
}

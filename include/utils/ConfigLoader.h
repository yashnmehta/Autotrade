#ifndef CONFIGLOADER_H
#define CONFIGLOADER_H

#include <QString>
#include <QMap>
#include <QSettings>
#include <QJsonObject>

class ConfigLoader
{
public:
    ConfigLoader();
    ~ConfigLoader();

    // Load configuration from file
    bool load(const QString &filePath);

    // Get configuration values
    QString getValue(const QString &section, const QString &key, const QString &defaultValue = "") const;
    int getInt(const QString &section, const QString &key, int defaultValue = 0) const;
    bool getBool(const QString &section, const QString &key, bool defaultValue = false) const;

    // Check if configuration is loaded
    bool isLoaded() const { return m_loaded; }

    // Specific getters for common config values
    QString getDefaultClient() const;
    QString getUserID() const;
    QString getStrategyID() const;

    // Credentials
    QString getMarketDataAppKey() const;
    QString getMarketDataSecretKey() const;
    QString getInteractiveAppKey() const;
    QString getInteractiveSecretKey() const;
    QString getSource() const;

    // XTS settings
    QString getXTSUrl() const;
    QString getXTSMDUrl() const;
    bool getXTSDisableSSL() const;
    QString getXTSBroadcastMode() const;

    // Tokens (if stored)
    QString getIAToken() const;
    QString getMDToken() const;

    // UDP Config
    QString getNSEFOMulticastIP() const;
    int getNSEFOPort() const;
    QString getNSECMMulticastIP() const;
    int getNSECMPort() const;
    QString getBSEFOMulticastIP() const;
    int getBSEFOPort() const;
    QString getBSECMMulticastIP() const;
    int getBSECMPort() const;

    QJsonObject getUDPConfig() const;
    
    // PriceCache Mode
    bool getUseLegacyPriceCache() const;

private:
    bool m_loaded;
    QMap<QString, QMap<QString, QString>> m_config;
    
    void parseINI(const QString &filePath);
};

#endif // CONFIGLOADER_H

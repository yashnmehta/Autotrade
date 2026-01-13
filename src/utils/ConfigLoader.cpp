#include "utils/ConfigLoader.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

ConfigLoader::ConfigLoader()
    : m_loaded(false)
{
}

ConfigLoader::~ConfigLoader()
{
}

bool ConfigLoader::load(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open config file:" << filePath;
        return false;
    }

    QTextStream in(&file);
    QString currentSection;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        // Skip empty lines and comments
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) {
            continue;
        }

        // Check for section header [SECTION]
        if (line.startsWith('[') && line.endsWith(']')) {
            currentSection = line.mid(1, line.length() - 2).trimmed();
            if (!m_config.contains(currentSection)) {
                m_config[currentSection] = QMap<QString, QString>();
            }
            continue;
        }

        // Parse key = value pairs
        int equalPos = line.indexOf('=');
        if (equalPos > 0) {
            QString key = line.left(equalPos).trimmed();
            QString value = line.mid(equalPos + 1).trimmed();

            // Store in default section if no section defined yet
            if (currentSection.isEmpty()) {
                currentSection = "DEFAULT";
                if (!m_config.contains(currentSection)) {
                    m_config[currentSection] = QMap<QString, QString>();
                }
            }

            m_config[currentSection][key] = value;
        }
    }

    file.close();
    m_loaded = true;

    qDebug() << "✅ Configuration loaded from:" << filePath;
    qDebug() << "   Sections found:" << m_config.keys();

    return true;
}

QString ConfigLoader::getValue(const QString &section, const QString &key, const QString &defaultValue) const
{
    if (m_config.contains(section) && m_config[section].contains(key)) {
        return m_config[section][key];
    }
    return defaultValue;
}

int ConfigLoader::getInt(const QString &section, const QString &key, int defaultValue) const
{
    QString value = getValue(section, key);
    if (!value.isEmpty()) {
        bool ok;
        int result = value.toInt(&ok);
        if (ok) return result;
    }
    return defaultValue;
}

bool ConfigLoader::getBool(const QString &section, const QString &key, bool defaultValue) const
{
    QString value = getValue(section, key).toLower();
    if (value == "true" || value == "1" || value == "yes") {
        return true;
    } else if (value == "false" || value == "0" || value == "no") {
        return false;
    }
    return defaultValue;
}

QString ConfigLoader::getDefaultClient() const
{
    return getValue("DEFAULT", "default_client");
}

QString ConfigLoader::getUserID() const
{
    return getValue("DEFAULT", "userid");
}

QString ConfigLoader::getStrategyID() const
{
    return getValue("DEFAULT", "strategy_id");
}

QString ConfigLoader::getMarketDataAppKey() const
{
    return getValue("CREDENTIALS", "marketdata_appkey");
}

QString ConfigLoader::getMarketDataSecretKey() const
{
    return getValue("CREDENTIALS", "marketdata_secretkey");
}

QString ConfigLoader::getInteractiveAppKey() const
{
    return getValue("CREDENTIALS", "interactive_appkey");
}

QString ConfigLoader::getInteractiveSecretKey() const
{
    return getValue("CREDENTIALS", "interactive_secretkey");
}

QString ConfigLoader::getSource() const
{
    return getValue("CREDENTIALS", "source", "WEBAPI");
}

QString ConfigLoader::getXTSUrl() const
{
    return getValue("XTS", "url");
}

QString ConfigLoader::getXTSMDUrl() const
{
    return getValue("XTS", "mdurl");
}

bool ConfigLoader::getXTSDisableSSL() const
{
    return getBool("XTS", "disable_ssl", false);
}

QString ConfigLoader::getXTSBroadcastMode() const
{
    return getValue("XTS", "broadcastmode", "Partial");
}

QString ConfigLoader::getIAToken() const
{
    return getValue("IATOKEN", "token");
}

QString ConfigLoader::getMDToken() const
{
    return getValue("MDTOKEN", "token");
}

QString ConfigLoader::getNSEFOMulticastIP() const
{
    return getValue("UDP", "nse_fo_multicast_ip", "233.1.2.5");
}

int ConfigLoader::getNSEFOPort() const
{
    int p = getInt("UDP", "nse_fo_port", 0);
    if (p == 0) p = getInt("UDP", "udp_fo", 34330);
    return p;
}

QString ConfigLoader::getNSECMMulticastIP() const
{
    return getValue("UDP", "nse_cm_multicast_ip", "233.1.2.5");
}

int ConfigLoader::getNSECMPort() const
{
    return getInt("UDP", "nse_cm_port", 8222);
}

QString ConfigLoader::getBSEFOMulticastIP() const
{
    return getValue("UDP", "bse_fo_multicast_ip", "239.1.2.5");
}

int ConfigLoader::getBSEFOPort() const
{
    return getInt("UDP", "bse_fo_port", 26002);
}

QString ConfigLoader::getBSECMMulticastIP() const
{
    return getValue("UDP", "bse_cm_multicast_ip", "239.1.2.5");
}

int ConfigLoader::getBSECMPort() const
{
    return getInt("UDP", "bse_cm_port", 26001);
}

QJsonObject ConfigLoader::getUDPConfig() const
{
    QJsonObject config;
    QJsonObject exchanges;
    
    // NSE FO Config
    QJsonObject nseFo;
    nseFo["enabled"] = true;
    nseFo["multicastGroup"] = getNSEFOMulticastIP();
    nseFo["port"] = getNSEFOPort();
    nseFo["protocol"] = "binary";
    exchanges["NSEFO"] = nseFo;
    
    // NSE CM Config 
    QJsonObject nseCm;
    nseCm["enabled"] = true;
    nseCm["multicastGroup"] = getValue("UDP", "nse_cm_multicast_ip", "233.1.2.5");
    nseCm["port"] = getInt("UDP", "nse_cm_port", 8222);
    nseCm["protocol"] = "binary";
    exchanges["NSECM"] = nseCm;

    // BSE FO Config
    QJsonObject bseFo;
    bseFo["enabled"] = true;
    bseFo["multicastGroup"] = getValue("UDP", "bse_fo_multicast_ip", "239.1.2.5");
    bseFo["port"] = getInt("UDP", "bse_fo_port", 26002);
    bseFo["protocol"] = "binary";
    exchanges["BSEFO"] = bseFo;

    // BSE CM Config
    QJsonObject bseCm;
    bseCm["enabled"] = true;
    bseCm["multicastGroup"] = getValue("UDP", "bse_cm_multicast_ip", "239.1.2.5");
    bseCm["port"] = getInt("UDP", "bse_cm_port", 26001);
    bseCm["protocol"] = "binary";
    exchanges["BSECM"] = bseCm;
    
    config["exchanges"] = exchanges;
    
    QJsonObject root;
    root["udp"] = config;
    return root;
}

bool ConfigLoader::getUseLegacyPriceCache() const
{
    // Default to true (legacy/current implementation) for safety
    // Set to false in config.ini to enable new zero-copy architecture
    return getBool("PRICECACHE", "use_legacy_mode", true);
}

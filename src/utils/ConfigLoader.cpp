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

QJsonObject ConfigLoader::getUDPConfig() const
{
    QJsonObject config;
    QJsonObject exchanges;
    
    // NSE FO Config
    QJsonObject nseFo;
    nseFo["enabled"] = true;
    nseFo["multicastGroup"] = "233.1.2.5";
    nseFo["port"] = getInt("UDP", "udp_fo", 34331);
    nseFo["protocol"] = "binary";
    exchanges["NSEFO"] = nseFo;
    
    // NSE CM Config 
    QJsonObject nseCm;
    nseCm["enabled"] = true;
    nseCm["multicastGroup"] = "233.1.2.5";
    nseCm["port"] = getInt("UDP", "udp_cash", 8270);
    nseCm["protocol"] = "binary";
    exchanges["NSECM"] = nseCm;

    // BSE FO Config (Placeholder)
    QJsonObject bseFo;
    bseFo["enabled"] = false; // Default false until we have IPs/Ports
    bseFo["multicastGroup"] = "0.0.0.0";
    bseFo["port"] = 0;
    bseFo["protocol"] = "binary";
    exchanges["BSEFO"] = bseFo;

    // BSE CM Config (Placeholder)
    QJsonObject bseCm;
    bseCm["enabled"] = false;
    bseCm["multicastGroup"] = "0.0.0.0";
    bseCm["port"] = 0;
    bseCm["protocol"] = "binary";
    exchanges["BSECM"] = bseCm;
    
    config["exchanges"] = exchanges;
    
    QJsonObject root;
    root["udp"] = config;
    return root;
}

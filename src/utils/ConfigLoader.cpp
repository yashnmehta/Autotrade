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
    m_configFilePath = filePath;

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

QString ConfigLoader::getBasePriceMode() const
{
    // Default to cash for backward compatibility
    return getValue("ATM_WATCH", "base_price_mode", "cash").toLower();
}

// ═══════════════════════════════════════════════════════════════════════════
// Feed Mode Configuration
// ═══════════════════════════════════════════════════════════════════════════

QString ConfigLoader::getFeedMode() const
{
    return getValue("FEED", "mode", "hybrid").toLower().trimmed();
}

QString ConfigLoader::getPrimaryDataProvider() const
{
    // Preferred: explicit primary_data_provider key
    QString provider = getValue("FEED", "primary_data_provider", "").toLower().trimmed();
    if (provider == "udp" || provider == "xts") {
        return provider;
    }

    // Fallback: derive from legacy 'mode' key
    QString mode = getFeedMode();
    if (mode == "xts_only" || mode == "xtsonly" || mode == "websocket") {
        return "xts";
    }
    return "udp"; // default
}

bool ConfigLoader::setPrimaryDataProvider(const QString& provider)
{
    if (m_configFilePath.isEmpty()) {
        qWarning() << "[ConfigLoader] Cannot save — config file path not set";
        return false;
    }

    // Update in-memory config
    m_config["FEED"]["primary_data_provider"] = provider;

    // Also keep 'mode' in sync for backward compatibility
    if (provider == "xts") {
        m_config["FEED"]["mode"] = "xts_only";
    } else {
        m_config["FEED"]["mode"] = "hybrid";
    }

    // Persist to config.ini — rewrite the file section by section
    QFile file(m_configFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[ConfigLoader] Cannot read config for save:" << m_configFilePath;
        return false;
    }

    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    // Find and update the primary_data_provider and mode lines in [FEED]
    bool inFeedSection = false;
    bool foundProvider = false;
    bool foundMode = false;

    for (int i = 0; i < lines.size(); ++i) {
        QString trimmed = lines[i].trimmed();

        // Track section
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            // If we were in FEED section and didn't find provider key, insert before leaving
            if (inFeedSection && !foundProvider) {
                lines.insert(i, QString("primary_data_provider = %1").arg(provider));
                foundProvider = true;
                ++i; // skip the inserted line
            }
            inFeedSection = (trimmed.toUpper() == "[FEED]");
            continue;
        }

        if (inFeedSection) {
            if (trimmed.startsWith("primary_data_provider")) {
                lines[i] = QString("primary_data_provider = %1").arg(provider);
                foundProvider = true;
            } else if (trimmed.startsWith("mode") && !trimmed.startsWith("mode_")) {
                // Update mode but preserve comment structure
                lines[i] = QString("mode = %1").arg(provider == "xts" ? "xts_only" : "hybrid");
                foundMode = true;
            }
        }
    }

    // If FEED section existed but provider key was never found, append it
    if (!foundProvider) {
        // Find end of FEED section or end of file
        bool inFeed = false;
        for (int i = 0; i < lines.size(); ++i) {
            QString trimmed = lines[i].trimmed();
            if (trimmed.toUpper() == "[FEED]") {
                inFeed = true;
                continue;
            }
            if (inFeed && trimmed.startsWith('[')) {
                lines.insert(i, QString("primary_data_provider = %1").arg(provider));
                foundProvider = true;
                break;
            }
        }
        if (!foundProvider) {
            // FEED section is at the end of file
            lines.append(QString("primary_data_provider = %1").arg(provider));
        }
    }

    // Write back
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "[ConfigLoader] Cannot write config:" << m_configFilePath;
        return false;
    }

    QTextStream out(&file);
    for (const QString& line : lines) {
        out << line << "\n";
    }
    file.close();

    qDebug() << "[ConfigLoader] Saved primary_data_provider =" << provider
             << "to" << m_configFilePath;
    return true;
}

int ConfigLoader::getFeedMaxTotalSubscriptions() const
{
    // First try the new global key, then fall back to the legacy per-segment key
    int val = getInt("FEED", "max_total_subscriptions", -1);
    if (val > 0) return val;
    return getInt("FEED", "max_tokens_per_segment", 1000);
}

int ConfigLoader::getFeedMaxRestCallsPerSec() const
{
    return getInt("FEED", "max_rest_calls_per_sec", 10);
}

int ConfigLoader::getFeedBatchSize() const
{
    return getInt("FEED", "batch_size", 50);
}

int ConfigLoader::getFeedBatchIntervalMs() const
{
    return getInt("FEED", "batch_interval_ms", 200);
}

int ConfigLoader::getFeedCooldownOnRateLimitMs() const
{
    return getInt("FEED", "cooldown_on_rate_limit_ms", 5000);
}

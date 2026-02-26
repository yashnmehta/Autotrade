#include "utils/LicenseManager.h"
#include "utils/ConfigLoader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSysInfo>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
LicenseManager& LicenseManager::instance()
{
    static LicenseManager s_instance;
    return s_instance;
}

LicenseManager::LicenseManager(QObject *parent)
    : QObject(parent)
{
    // Default last result: valid (stub behaviour)
    m_lastResult.valid  = true;
    m_lastResult.reason = "Stub: license check always passes";
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void LicenseManager::initialize(const ConfigLoader *config)
{
    if (m_initialized) {
        qWarning() << "[LicenseManager] initialize() called more than once — ignoring";
        return;
    }

    // -----------------------------------------------------------------------
    // Phase 1 (stub): read any key stored in config
    // When real licensing is added, parse the key from a dedicated
    // [License] section, e.g. config->getValue("License", "Key", "").
    // -----------------------------------------------------------------------
    if (config && config->isLoaded()) {
        m_licenseKey = config->getValue("License", "Key", "");
    }

    m_machineId   = generateMachineId();
    m_initialized = true;

    qDebug() << "[LicenseManager] Initialized."
             << "Key:" << (m_licenseKey.isEmpty() ? "(none)" : m_licenseKey)
             << "| MachineId:" << (m_machineId.isEmpty() ? "(stub)" : m_machineId);
}

// ---------------------------------------------------------------------------
// Public check
// ---------------------------------------------------------------------------
LicenseManager::CheckResult LicenseManager::checkLicense() const
{
    if (!m_initialized) {
        qWarning() << "[LicenseManager] checkLicense() called before initialize()";
    }

    m_lastResult = performLocalCheck();

    qDebug() << "[LicenseManager] License check result:"
             << (m_lastResult.valid ? "✅ VALID" : "❌ INVALID")
             << "| Reason:" << m_lastResult.reason
             << "| Trial:" << m_lastResult.isTrial
             << "| Expired:" << m_lastResult.isExpired;

    return m_lastResult;
}

// ---------------------------------------------------------------------------
// Async / online stub
// ---------------------------------------------------------------------------
void LicenseManager::performOnlineCheck(OnlineCheckCallback callback) const
{
    // Phase 2 (future): perform an HTTPS activation request here.
    // For now, immediately call back with the cached local result.
    if (callback) {
        callback(m_lastResult);
    }
}

// ---------------------------------------------------------------------------
// Private: local check (STUB — always valid)
// ---------------------------------------------------------------------------
LicenseManager::CheckResult LicenseManager::performLocalCheck() const
{
    CheckResult result;

    // -----------------------------------------------------------------------
    // STUB IMPLEMENTATION
    // -----------------------------------------------------------------------
    // This is the single extension point for real license validation.
    // To implement real licensing:
    //
    //   1. Verify m_licenseKey is non-empty and well-formed.
    //   2. Check an expiry date embedded in or associated with the key.
    //   3. Optionally bind to m_machineId (hardware fingerprint).
    //   4. Set result.valid, result.reason, result.isExpired, result.isTrial,
    //      result.expiresAt, and per-feature flags accordingly.
    //
    // Example skeleton:
    //
    //   if (m_licenseKey.isEmpty()) {
    //       result.valid  = false;
    //       result.reason = "No license key found. Please activate the application.";
    //       return result;
    //   }
    //   // ... decode / verify key ...
    //   // result.expiresAt = <decoded expiry>;
    //   // result.isExpired = QDateTime::currentDateTime() > result.expiresAt;
    //   // result.valid     = !result.isExpired;
    //
    // -----------------------------------------------------------------------

    result.valid   = true;
    result.reason  = "License valid (stub — real validation not yet implemented)";
    result.isTrial = false;

    // All feature flags default to true in the stub
    result.featureAlgoTrading      = true;
    result.featureOptionsGreeks    = true;
    result.featureStrategyBuilder  = true;

    return result;
}

// ---------------------------------------------------------------------------
// Private: machine fingerprint (STUB — returns empty string)
// ---------------------------------------------------------------------------
QString LicenseManager::generateMachineId() const
{
    // Phase 1 (stub): no machine binding.
    // Phase 2 (future): hash CPU ID + primary MAC address + disk serial.
    // Example using Qt:
    //   QString id = QSysInfo::machineUniqueId();    // Qt 5.11+
    //   if (!id.isEmpty()) return id;
    //   // fallback: hash hostname + primaryInterface MAC …

    QString id = QSysInfo::machineUniqueId();
    if (!id.isEmpty()) {
        return id;
    }

    // Fallback: use product type + cpu architecture as a weak pseudo-ID
    return QSysInfo::productType() + "-" + QSysInfo::currentCpuArchitecture();
}

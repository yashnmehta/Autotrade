#ifndef LICENSEMANAGER_H
#define LICENSEMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <functional>

/**
 * @brief Manages application licensing and entitlement checks.
 *
 * Architecture
 * ============
 * This is a singleton service consulted at startup (after config/preferences
 * are loaded, before the login window appears).  It is intentionally designed
 * for future extensibility:
 *
 *  - Local file / hardware-ID license  (Phase 1 — stub always-valid)
 *  - Online activation / seat check    (Phase 2 — server round-trip)
 *  - Feature-flag entitlements         (Phase 3 — per-module gating)
 *
 * Usage in main.cpp
 * =================
 * @code
 *   LicenseManager &lic = LicenseManager::instance();
 *   lic.initialize(config);          // load license key / machine ID
 *   LicenseManager::CheckResult res = lic.checkLicense();
 *   if (!res.valid) {
 *       // show LicenseDialog or quit
 *   }
 * @endcode
 *
 * Stub behaviour (current)
 * ========================
 * All checks return valid=true so the application boots without a real key.
 * Replace the body of performLocalCheck() when real licensing is needed.
 */
class ConfigLoader;  // forward-declare to avoid a heavy include

class LicenseManager : public QObject
{
    Q_OBJECT

public:
    // ------------------------------------------------------------------
    // Result type returned by checkLicense()
    // ------------------------------------------------------------------
    struct CheckResult {
        bool    valid      = false;   ///< true  → licence is OK, proceed
        QString reason;               ///< human-readable reason for failure
        QString licenseKey;           ///< key that was evaluated (may be empty)
        QDateTime expiresAt;          ///< null  → perpetual / not applicable
        bool    isExpired  = false;   ///< convenience flag
        bool    isTrial    = false;   ///< true  → trial / evaluation mode

        // Feature flags (Phase 3 — future)
        bool featureAlgoTrading = true;
        bool featureOptionsGreeks = true;
        bool featureStrategyBuilder = true;
    };

    // ------------------------------------------------------------------
    // Singleton accessor
    // ------------------------------------------------------------------
    static LicenseManager& instance();

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    /**
     * @brief Initialize the manager with application configuration.
     *
     * Call this once after ConfigLoader has been populated.
     * Reads license key / machine-ID / trial state from config (if any).
     *
     * @param config  Pointer to the loaded ConfigLoader (may be nullptr if
     *                config failed to load — defaults are used in that case).
     */
    void initialize(const ConfigLoader *config);

    /**
     * @brief Run all applicable license checks and return a combined result.
     *
     * Currently stubbed to always return valid=true.
     * Internally calls performLocalCheck() which is the extension point.
     */
    CheckResult checkLicense() const;

    // ------------------------------------------------------------------
    // Convenience accessors (populated after checkLicense())
    // ------------------------------------------------------------------
    bool isValid()   const { return m_lastResult.valid; }
    bool isTrial()   const { return m_lastResult.isTrial; }
    bool isExpired() const { return m_lastResult.isExpired; }
    QString licenseKey() const { return m_licenseKey; }
    QDateTime expiresAt() const { return m_lastResult.expiresAt; }

    // Per-feature entitlement queries
    bool canUseAlgoTrading()      const { return m_lastResult.featureAlgoTrading; }
    bool canUseOptionsGreeks()    const { return m_lastResult.featureOptionsGreeks; }
    bool canUseStrategyBuilder()  const { return m_lastResult.featureStrategyBuilder; }

    // ------------------------------------------------------------------
    // Async / online check hook (Phase 2 — future)
    // ------------------------------------------------------------------
    using OnlineCheckCallback = std::function<void(CheckResult)>;

    /**
     * @brief Trigger an optional asynchronous online activation check.
     *
     * Currently a no-op; callback is invoked immediately with the cached
     * local result.  When a real server round-trip is implemented, the
     * callback will be called on the Qt main thread after the network reply.
     */
    void performOnlineCheck(OnlineCheckCallback callback) const;

signals:
    /// Emitted when the license state changes (e.g. online check completes).
    void licenseStateChanged(LicenseManager::CheckResult result);

private:
    // ------------------------------------------------------------------
    // Private constructor (singleton)
    // ------------------------------------------------------------------
    explicit LicenseManager(QObject *parent = nullptr);
    ~LicenseManager() override = default;

    LicenseManager(const LicenseManager&)            = delete;
    LicenseManager& operator=(const LicenseManager&) = delete;

    // ------------------------------------------------------------------
    // Internal helpers
    // ------------------------------------------------------------------

    /**
     * @brief Extension point for real license validation logic.
     *
     * Replace this implementation when moving from stub to real licensing.
     * Should be pure-local (no network I/O) so it never blocks the UI thread.
     */
    CheckResult performLocalCheck() const;

    /**
     * @brief Generate a stable hardware fingerprint for machine-binding.
     *
     * Currently returns an empty string (stub).  When activated, this should
     * hash CPU ID, MAC address, disk serial, etc. to produce a stable ID.
     */
    QString generateMachineId() const;

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------
    bool         m_initialized = false;
    QString      m_licenseKey;
    QString      m_machineId;
    mutable CheckResult m_lastResult;   ///< cached result of last checkLicense()
};

// Make CheckResult available to QMetaType / cross-thread signals
Q_DECLARE_METATYPE(LicenseManager::CheckResult)

#endif // LICENSEMANAGER_H

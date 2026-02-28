#ifndef GENERICPROFILEMANAGER_H
#define GENERICPROFILEMANAGER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include "models/profiles/GenericTableProfile.h"

/**
 * @brief Generic profile manager for any table-based window.
 *
 * - Holds **preset** profiles (built-in, cannot be deleted/overwritten).
 * - Stores **custom** profiles on disk as JSON files.
 * - Each window type has a unique `windowName` that namespaces files.
 * - Remembers the "default" (last-used) profile name per window.
 *
 * Usage (one instance per window type):
 * @code
 *   GenericProfileManager mgr("profiles", "OptionChain");
 *   mgr.addPreset(defaultProfile);
 *   mgr.addPreset(compactProfile);
 *   mgr.loadCustomProfiles();          // reads profiles/OptionChain_*.json
 *   GenericTableProfile p = mgr.getProfile("Compact");
 * @endcode
 */
class GenericProfileManager {
public:
    GenericProfileManager(const QString &baseDir, const QString &windowName)
        : m_windowName(windowName)
    {
        // Resolve relative paths against the application directory, not the CWD.
        // On macOS the CWD is often "/" when launched from Finder/.app bundle,
        // so a relative "profiles" would write to "/profiles/" — wrong.
        if (QDir::isRelativePath(baseDir)) {
            QString appDir = QCoreApplication::applicationDirPath();
            // Walk up from the binary to the project root.
            // macOS .app bundle: <project>/build/TradingTerminal.app/Contents/MacOS/
            // Normal build:      <project>/build/
            QDir d(appDir);
            // Try to find the profiles dir by walking up (max 5 levels)
            bool found = false;
            for (int i = 0; i < 5; ++i) {
                if (QDir(d.filePath(baseDir)).exists() ||
                    QFileInfo(d.filePath("CMakeLists.txt")).exists()) {
                    m_baseDir = d.filePath(baseDir);
                    found = true;
                    break;
                }
                if (!d.cdUp()) break;
            }
            if (!found) {
                // Fallback: create next to the executable
                m_baseDir = appDir + "/" + baseDir;
            }
        } else {
            m_baseDir = baseDir;
        }
        QDir().mkpath(m_baseDir);
        qDebug() << "[GenericProfileManager]" << m_windowName << "→ baseDir resolved to:" << m_baseDir;
    }

    // ── Preset profiles (built-in, read-only) ────────────────────────────
    void addPreset(const GenericTableProfile &profile) {
        m_presets[profile.name()] = profile;
        if (m_presetOrder.isEmpty() || !m_presetOrder.contains(profile.name()))
            m_presetOrder.append(profile.name());
    }

    QStringList presetNames() const { return m_presetOrder; }

    bool isPreset(const QString &name) const { return m_presets.contains(name); }

    // ── Unified access (presets first, then custom) ──────────────────────
    bool hasProfile(const QString &name) const {
        return m_presets.contains(name) || m_custom.contains(name);
    }

    GenericTableProfile getProfile(const QString &name) const {
        if (m_presets.contains(name)) return m_presets[name];
        if (m_custom.contains(name))  return m_custom[name];
        return GenericTableProfile();
    }

    QStringList customProfileNames() const {
        return m_custom.keys();
    }

    // ── CRUD for custom profiles ─────────────────────────────────────────
    bool saveCustomProfile(const GenericTableProfile &profile) {
        if (isPreset(profile.name())) return false;  // can't overwrite preset
        m_custom[profile.name()] = profile;
        QString path = m_baseDir + "/" + m_windowName + "_" + profile.name() + ".json";
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return false;
        file.write(QJsonDocument(profile.toJson()).toJson(QJsonDocument::Indented));
        return true;
    }

    bool deleteCustomProfile(const QString &name) {
        if (isPreset(name)) return false;
        m_custom.remove(name);
        QString path = m_baseDir + "/" + m_windowName + "_" + name + ".json";
        return QFile::remove(path);
    }

    // ── Persistence ──────────────────────────────────────────────────────
    void loadCustomProfiles() {
        QDir dir(m_baseDir);
        if (!dir.exists()) return;
        QString prefix = m_windowName + "_";
        QStringList files = dir.entryList({prefix + "*.json"}, QDir::Files);
        for (const QString &file : files) {
            QFile f(dir.filePath(file));
            if (!f.open(QIODevice::ReadOnly)) continue;
            GenericTableProfile prof;
            prof.fromJson(QJsonDocument::fromJson(f.readAll()).object());
            if (!isPreset(prof.name()))
                m_custom[prof.name()] = prof;
        }
    }

    // ── Default / current profile name ───────────────────────────────────
    void saveDefaultProfileName(const QString &profileName) {
        QString path = m_baseDir + "/" + m_windowName + "_default.txt";
        QFile file(path);
        if (file.open(QIODevice::WriteOnly))
            file.write(profileName.toUtf8());
    }

    QString loadDefaultProfileName() const {
        QString path = m_baseDir + "/" + m_windowName + "_default.txt";
        QFile file(path);
        if (file.open(QIODevice::ReadOnly))
            return QString::fromUtf8(file.readAll()).trimmed();
        return m_presetOrder.isEmpty() ? "Default" : m_presetOrder.first();
    }

    // ── Last-used profile (always persists, even for preset names) ───────
    /// Saves the current profile state to a special _LastUsed.json file.
    /// Unlike saveCustomProfile(), this works even for preset-named profiles
    /// (e.g. "Default") so that user's column width/order tweaks survive restart.
    bool saveLastUsedProfile(const GenericTableProfile &profile) {
        QDir().mkpath(m_baseDir);
        QString path = m_baseDir + "/" + m_windowName + "_LastUsed.json";
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return false;
        file.write(QJsonDocument(profile.toJson()).toJson(QJsonDocument::Indented));
        return true;
    }

    /// Loads the last-used profile state. Returns true if found.
    bool loadLastUsedProfile(GenericTableProfile &profile) const {
        QString path = m_baseDir + "/" + m_windowName + "_LastUsed.json";
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject()) return false;
        profile.fromJson(doc.object());
        return !profile.name().isEmpty();
    }

    // ── Legacy compat (old API used by GenericProfileDialog) ─────────────
    bool saveProfile(const QString & /*windowName*/, const GenericTableProfile &profile) {
        return saveCustomProfile(profile);
    }
    bool loadProfile(const QString & /*windowName*/, const QString &profileName, GenericTableProfile &profile) {
        if (!hasProfile(profileName)) return false;
        profile = getProfile(profileName);
        return true;
    }
    QStringList listProfiles(const QString & /*windowName*/) {
        return customProfileNames();
    }
    void saveDefaultProfile(const QString & /*windowName*/, const QString &profileName) {
        saveDefaultProfileName(profileName);
    }
    QString getDefaultProfileName(const QString & /*windowName*/) {
        return loadDefaultProfileName();
    }

private:
    QString m_baseDir;
    QString m_windowName;
    QMap<QString, GenericTableProfile> m_presets;
    QStringList m_presetOrder;                     ///< insertion order of presets
    QMap<QString, GenericTableProfile> m_custom;
};

#endif // GENERICPROFILEMANAGER_H

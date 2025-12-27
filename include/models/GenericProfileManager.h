#ifndef GENERICPROFILEMANAGER_H
#define GENERICPROFILEMANAGER_H

#include <QString>
#include <QList>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "models/GenericTableProfile.h"

class GenericProfileManager {
public:
    GenericProfileManager(const QString &baseDir) : m_baseDir(baseDir) {
        QDir().mkpath(m_baseDir);
    }

    bool saveProfile(const QString &windowName, const GenericTableProfile &profile) {
        QString path = m_baseDir + "/" + windowName + "_" + profile.name() + ".json";
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return false;
        file.write(QJsonDocument(profile.toJson()).toJson());
        return true;
    }

    bool loadProfile(const QString &windowName, const QString &profileName, GenericTableProfile &profile) {
        QString path = m_baseDir + "/" + windowName + "_" + profileName + ".json";
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;
        profile.fromJson(QJsonDocument::fromJson(file.readAll()).object());
        return true;
    }

    QStringList listProfiles(const QString &windowName) {
        QDir dir(m_baseDir);
        QStringList filters;
        filters << windowName + "_*.json";
        QStringList files = dir.entryList(filters, QDir::Files);
        
        QStringList names;
        for (const QString &file : files) {
            QString name = file;
            name.remove(0, windowName.length() + 1); // remove "windowName_"
            name.remove(".json");
            names << name;
        }
        return names;
    }

    void saveDefaultProfile(const QString &windowName, const QString &profileName) {
        QString path = m_baseDir + "/" + windowName + "_default.txt";
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(profileName.toUtf8());
        }
    }

    QString getDefaultProfileName(const QString &windowName) {
        QString path = m_baseDir + "/" + windowName + "_default.txt";
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            return QString::fromUtf8(file.readAll());
        }
        return "Default";
    }

private:
    QString m_baseDir;
};

#endif // GENERICPROFILEMANAGER_H

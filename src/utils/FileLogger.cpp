#include "utils/FileLogger.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>
#include <cstdio>

// ── Module-private state (single TU, no ODR risk) ────────────────────────
static QFile  *s_logFile  = nullptr;
static QMutex  s_logMutex;

// ── Qt message handler ───────────────────────────────────────────────────
static void fileMessageHandler(QtMsgType type,
                               const QMessageLogContext & /*context*/,
                               const QString &msg)
{
    QMutexLocker locker(&s_logMutex);

    QString level;
    switch (type) {
    case QtDebugMsg:
        // Silence DEBUG logs for a cleaner user experience
        return;
    case QtInfoMsg:
        level = "INFO ";
        break;
    case QtWarningMsg:
        level = "WARN ";
        break;
    case QtCriticalMsg:
        level = "ERROR";
        break;
    case QtFatalMsg:
        level = "FATAL";
        break;
    }

    QString timestamp =
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString logMessage =
        QString("[%1] [%2] %3\n").arg(timestamp, level, msg);

    // Write to console
    fprintf(stderr, "%s", logMessage.toLocal8Bit().constData());

    // Write to file
    if (s_logFile && s_logFile->isOpen()) {
        QTextStream stream(s_logFile);
        stream << logMessage;
        stream.flush();
    }
}

// ── Public API ───────────────────────────────────────────────────────────

void setupFileLogging()
{
    // Create logs directory
    QDir().mkpath("logs");

    // Create log file with timestamp
    QString timestamp  = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString logFileName = QString("logs/trading_terminal_%1.log").arg(timestamp);

    s_logFile = new QFile(logFileName);
    if (s_logFile->open(QIODevice::WriteOnly | QIODevice::Append |
                        QIODevice::Text)) {
        // Use fprintf here because qDebug is about to be redirected
        fprintf(stderr, "[FileLogger] Log file created: %s\n",
                logFileName.toLocal8Bit().constData());
    } else {
        fprintf(stderr, "[FileLogger] Failed to open log file: %s\n",
                logFileName.toLocal8Bit().constData());
    }

    // Install message handler
    qInstallMessageHandler(fileMessageHandler);
}

void cleanupFileLogging()
{
    if (s_logFile) {
        s_logFile->close();
        delete s_logFile;
        s_logFile = nullptr;
    }
}

// Simple File Logger for Trading Terminal
// Add this to main.cpp before QApplication creation

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QMutex>
#include <QDebug>

static QFile *logFile = nullptr;
static QMutex logMutex;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker locker(&logMutex);
    
    QString level;
    switch (type) {
        case QtDebugMsg:
            // Info Mode: Skip debug logs
            // Change false to true to enable debug logs
            if (false) return; 
            level = "DEBUG"; 
            break;
        case QtInfoMsg:     level = "INFO "; break;
        case QtWarningMsg:  level = "WARN "; break;
        case QtCriticalMsg: level = "ERROR"; break;
        case QtFatalMsg:    level = "FATAL"; break;
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString logMessage = QString("[%1] [%2] %3\n").arg(timestamp).arg(level).arg(msg);
    
    // Write to console
    fprintf(stderr, "%s", logMessage.toLocal8Bit().constData());
    
    // Write to file
    if (logFile && logFile->isOpen()) {
        QTextStream stream(logFile);
        stream << logMessage;
        stream.flush();
    }
}

void setupFileLogging()
{
    // Create logs directory
    QDir().mkpath("logs");
    
    // Create log file with timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString logFileName = QString("logs/trading_terminal_%1.log").arg(timestamp);
    
    logFile = new QFile(logFileName);
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qDebug() << "Log file created:" << logFileName;
    } else {
        fprintf(stderr, "Failed to open log file: %s\n", logFileName.toLocal8Bit().constData());
    }
    
    // Install message handler
    qInstallMessageHandler(messageHandler);
}

void cleanupFileLogging()
{
    if (logFile) {
        logFile->close();
        delete logFile;
        logFile = nullptr;
    }
}

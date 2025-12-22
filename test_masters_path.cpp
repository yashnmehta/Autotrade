#include <QCoreApplication>
#include <QDebug>
#include "repository/RepositoryManager.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("Trading Terminal");
    
    QString mastersDir = RepositoryManager::getMastersDirectory();
    qDebug() << "Masters directory:" << mastersDir;
    qDebug() << "Directory exists:" << QDir(mastersDir).exists();
    
    return 0;
}

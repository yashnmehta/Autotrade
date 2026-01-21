#include "repository/RepositoryManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <iostream>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    QString mastersPath = (argc > 1) ? argv[1] : "../MasterFiles";
    
    std::cout << "Loading repository from: " << mastersPath.toStdString() << std::endl;
    
    RepositoryManager* repoMgr = RepositoryManager::getInstance();
    
    QElapsedTimer timer;
    timer.start();
    
    qDebug() << "Starting loadAll()...";
    bool success = repoMgr->loadAll(mastersPath);
    qDebug() << "loadAll() returned:" << success;
    
    qint64 elapsed = timer.elapsed();
    
    if (success) {
        std::cout << "✓ Repository loaded successfully in " << elapsed << " ms" << std::endl;
        
        const NSEFORepository* nsefo = repoMgr->getNSEFORepository();
        if (nsefo) {
            std::cout << "  NSEFO Total: " << nsefo->getTotalCount() << std::endl;
            std::cout << "  NSEFO Regular: " << nsefo->getRegularCount() << std::endl;
            std::cout << "  NSEFO Spread: " << nsefo->getSpreadCount() << std::endl;
        }
        
        return 0;
    } else {
        std::cerr << "✗ Failed to load repository!" << std::endl;
        return 1;
    }
}

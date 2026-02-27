#include "app/AppBootstrap.h"
#include <QApplication>
#include <cstdio>

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    AppBootstrap bootstrap(&app);
    return bootstrap.run();
}


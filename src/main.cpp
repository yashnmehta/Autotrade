#include "app/MainWindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application metadata
    app.setApplicationName("Trading Terminal");
    app.setOrganizationName("TradingCo");
    app.setApplicationVersion("1.0.0");

    // Create and show main window
    MainWindow window;
    window.show();

    return app.exec();
}

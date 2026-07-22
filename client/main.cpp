#include "main_window.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    application.setStyle(QStyleFactory::create("Fusion"));
    QCoreApplication::setOrganizationName("SmartHomeMonitor");
    QCoreApplication::setApplicationName("SmartHomeMonitorClient");
    MainWindow window; window.show();
    return application.exec();
}

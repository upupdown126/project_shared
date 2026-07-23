#include "main_window.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    application.setStyle(QStyleFactory::create("Fusion"));
    application.setWindowIcon(QIcon(QStringLiteral(":/branding/app_icon.png")));
    QCoreApplication::setOrganizationName("SmartHomeMonitor");
    QCoreApplication::setApplicationName("SmartHomeMonitorClient");
    MainWindow window; window.show();
    return application.exec();
}

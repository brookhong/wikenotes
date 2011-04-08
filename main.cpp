#include <QtGui/QApplication>
#include "qtsingleapplication/qtsingleapplication.h"
#include "mainwindow.h"

MainWindow* g_mainWindow = 0;
int main(int argc, char *argv[])
{
    QtSingleApplication a(argc, argv);
    if (a.isRunning())
        return !a.sendMessage(argv[0]);

    MainWindow w;
    g_mainWindow = &w;
    w.show();
    w.openDB();
    QObject::connect(&a, SIGNAL(messageReceived(const QString&)),&w, SLOT(handleSingleMessage(const QString&)));
    return a.exec();
}

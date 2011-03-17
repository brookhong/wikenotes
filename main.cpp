#include <QtGui/QApplication>
#include "mainwindow.h"

MainWindow* g_mainWindow = 0;
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    g_mainWindow = &w;
    w.show();
    w.init();

    return a.exec();
}


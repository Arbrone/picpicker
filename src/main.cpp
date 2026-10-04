#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("picpicker");
    QApplication::setWindowIcon(QIcon(":/logo.png"));

    MainWindow window;
    window.show();
    if (argc > 1) window.openFolder(QString::fromLocal8Bit(argv[1]));
    return app.exec();
}

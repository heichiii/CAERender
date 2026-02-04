#include "App/MainWindow.h"
#include <QApplication>
#include <QString>
int main(int argc, char* argv[])
{

    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    if (argc == 2)
    {
        QString file(argv[1]);
        window.test_openFile(file);
    }
    return app.exec();
}
#include "App/MainWindow.h"
#include <QApplication>
#include <QString>
#include "TestTool/debug.h"
int main(int argc, char* argv[])
{

    QApplication app(argc, argv);
    MainWindow window;
    window.show();
#ifdef DEBUG_MODE
    QString file = "E:/data/CAE/VTK/motorBike_500.vtk";
    window.test_openFile(file);
#endif
    
    return app.exec();
}
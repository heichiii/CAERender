#include "App/MainWindow.h"
#include <QApplication>
#include <QString>
#include "TestTool/debug.h"

#ifdef _WIN32
extern "C" {
    // Hint to NVIDIA/AMD drivers to prefer the discrete GPU.
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
int main(int argc, char* argv[])
{

    QApplication app(argc, argv);
    MainWindow window;
    window.show();
#if DEBUG_MODE
    window.test_openFile();
#endif
    
    return app.exec();
}
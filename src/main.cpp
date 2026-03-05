#include "App/MainWindow.h"
#include <QApplication>
#include <QString>
#include "TestTool/debug.h"
// #include "TestTool/CallCounter.h"

#ifdef _WIN32
extern "C" {
    #ifdef PREFER_DISCRETE_GPU
        // 告诉 NVIDIA/AMD 驱动程序使用独立显卡
        __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
        __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
    
    
    #elif defined(PREFER_INTEGRATED_GPU)
        // 告诉 NVIDIA/AMD 驱动程序使用集成显卡
        __declspec(dllexport) volatile unsigned long NvOptimusEnablement = 0x00000000;
        __declspec(dllexport) volatile int AmdPowerXpressRequestHighPerformance = 0;
    #endif
}
#endif


int main(int argc, char* argv[])
{
    // COUNT_FUNCTION_CALL
    // TestTool::AutoDumpCallStats autoDumpCallStats;

    QApplication app(argc, argv);
    MainWindow window;
    window.show();
#if DEBUG_MODE
    window.test_openFile();
#endif
    
    return app.exec();
}
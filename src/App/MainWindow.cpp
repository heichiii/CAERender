#include "MainWindow.h"
#include <QAction>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QMenuBar>
#include "Loader/LoaderFactory.h"
#include "TestTool/Profiler.h"
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("Hello, Qt!");
    resize(1200, 800);
    setupMenus();
}
void MainWindow::setupMenus()
{
    QMenu* fileMenu = menuBar()->addMenu("&File");
    openFile_ = fileMenu->addAction("Open &File...");
    openDir_ = fileMenu->addAction("Open &Directory...");
    connect(openFile_, &QAction::triggered, this, &MainWindow::openFile);
    connect(openDir_, &QAction::triggered, this, &MainWindow::openDirectory);
}
void MainWindow::openFile()
{
    QString filename = QFileDialog::getOpenFileName(
        this, "打开VTK文件", "E:/data/CAE", "VTK Files (*.vtk *.vtu *.vtp);;All Files (*.*)");

    if (filename.isEmpty())
    {
        qWarning() << "[MainWindow::openFile] :  No file selected.";
        return;
    }

    glWidget_->loadFile(filename.toStdString());

}
void MainWindow::openDirectory()
{
    // TODO: open directory
}


void MainWindow::test_openFile(const QString& filename)
{
    PROFILE_CODE

    glWidget_->loadFile(filename.toStdString());
    
}
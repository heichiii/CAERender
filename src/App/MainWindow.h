#pragma once

#include "GLWidget.h"
#include <QMainWindow>
#include <QMenu>
#include <QAction>
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override= default;
    void test_openFile(const QString& filename);
private:

    void setupMenus();

    GLWidget* glWidget_;
    QAction* openFile_;
    QAction* openDir_;
    
private slots:
    void openFile();
    void openDirectory();
};
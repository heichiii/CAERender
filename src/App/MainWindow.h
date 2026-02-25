#pragma once

#include "GLWidget.h"
#include <QMainWindow>
#include <QMenu>
#include <QAction>
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override= default;
    void test_openFile(const QString& filename);
private:

    void setupUI();
    void setupMenus();

    GLWidget* glWidget_;
    QAction* openFile_;
    QAction* openDir_;
    
private slots:
    void openFile();
    void openDirectory();
};
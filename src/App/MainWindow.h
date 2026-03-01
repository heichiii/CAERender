#pragma once

#include "GLWidget.h"
#include <QMainWindow>
#include <QMenu>
#include <QAction>
#include <QWidgetAction>
#include <QDockWidget>
#include <QLabel>
#include <string>
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override= default;
    void test_openFile();
private:

    void setupUI();
    void setupMenus();
    void updatePropertiesPanel();

    GLWidget* gl_widget_;
    QDockWidget* render_dock_;
    QDockWidget* properties_dock_;


    QAction* openfile_;
    QAction* opendir_;
    
    // properties panel widgets
    QLabel* file_path_label_;
    QLabel* info_label_;
    std::string current_file_path_;
private slots:
    void openFile();
    void openDirectory();
};
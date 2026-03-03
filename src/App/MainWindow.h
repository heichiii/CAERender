#pragma once

#include "GLWidget.h"
#include <QMainWindow>
#include <QMenu>
#include <QAction>
#include <QWidgetAction>
#include <QDockWidget>
#include <QLabel>
#include <QComboBox>
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
    void updateFieldList();
    void updateFieldOptions();

    GLWidget* gl_widget_;
    QDockWidget* render_dock_;
    QDockWidget* properties_dock_;


    QAction* openfile_;
    QAction* opendir_;
    
    // properties panel widgets
    QLabel* file_path_label_;
    QLabel* info_label_;
    QLabel* fps_label_;
    std::string current_file_path_;
    
    // render options widgets
    QComboBox* mesh_render_mode_combo_;
    QComboBox* field_combo_;
    QWidget* field_options_widget_;
    QLabel* color_scheme_label_;
    QComboBox* color_scheme_combo_;
    QLabel* vector_render_mode_label_;
    QComboBox* vector_render_mode_combo_;
    
private slots:
    void openFile();
    void openDirectory();
    void onFieldSelectionChanged(int index);
    void onMeshRenderModeChanged(int index);
    void onColorSchemeChanged(int index);
    void onVectorRenderModeChanged(int index);
    void onFpsUpdated(float fps);

};
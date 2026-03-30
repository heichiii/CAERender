#pragma once

#include "GLWidget.h"
#include "PropertiesPanelWidget.h"
#include "RenderOptionsWidget.h"
#include "StreamlineOptionsWidget.h"
#include <QMainWindow>
#include <QMenu>
#include <QAction>
#include <QWidgetAction>
#include <QDockWidget>
#include <QLabel>
#include <QVector3D>
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

    // UI components
    GLWidget* gl_widget_;
    QDockWidget* render_dock_;
    QDockWidget* streamline_dock_;
    QDockWidget* properties_dock_;
    RenderOptionsWidget* render_options_widget_;
    PropertiesPanelWidget* properties_panel_widget_;
    StreamlineOptionsWidget* streamline_options_widget_;

    //Menu actions
    QAction* openfile_;
    QAction* opendir_;
    
    // Status bar labels
    QLabel* fps_label_;
    QLabel* lod_label_;

    std::string current_file_path_;

    
private slots:
    void openFile();
    void openDirectory();
    void onFieldSelectionChanged(int index);
    void onMeshRenderModeChanged(int index);
    void onColorSchemeChanged(int index);
    void onVectorRenderModeChanged(int index);
    void onFpsUpdated(float fps);
    void onLodLevelChanged(LODLevel level);
    void onLodCheckBoxToggled(bool checked);
    void onSeedSphereCenterChanged(const QVector3D& center, bool valid);

    // Streamline options signals
    void onStreamlineVectorFieldChanged(const QString& field_name);
    void onStreamlineRenderModeChanged(int mode);
    void onStreamlineLodLevelChanged(LODLevel level);
    void onSeedSphereRadiusChanged(double value);
    void onSeedSphereCountChanged(int value);
    void onSeedSphereOffsetChanged(const QVector3D& offset);
    void onGenerateStreamlinesRequested();

};
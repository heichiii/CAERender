#pragma once

#include <QOpenGLWidget>
#include <QMouseEvent>
#include <QWheelEvent>
#include "Data/CaseData.h"
#include "Render/Renderer.h"
#include "Render/Camera.h"
class GLWidget: public QOpenGLWidget
{
public:
    explicit GLWidget(QWidget* parent = nullptr);
    ~GLWidget() override= default;
    void loadFile(const std::string& filename);
    void setMesh(const GPUData* p_gpu_data);
    CaseData* getCaseData()  { return &case_data_; }
    
    // 渲染控制接口
    void setMeshRenderMode(MeshRenderMode mode);
    void setColorScheme(ColorScheme scheme);
    void setUseFieldColoring(bool use);
    void activateField(const std::string& field_name);
    void activateVectorField(const std::string& field_name);
    void setVectorRenderMode(VectorRenderMode mode);
    void setRenderingVector(bool render_vector);

private:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    CaseData case_data_;
    Renderer renderer_;
    Camera camera_;


    QPoint last_mouse_pos_;
    QPoint mouse_press_pos_;  // 鼠标按下位置
    bool is_rotating_ = false;
    bool is_panning_ = false;
    bool is_zooming_ = false;

    // 计算屏幕中心作为旋转中心
    QVector3D getScreenCenterInWorld() const;
};
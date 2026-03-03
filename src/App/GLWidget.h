#pragma once

#include <QOpenGLWidget>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QElapsedTimer>
#include <QVector3D>
#include <QQuaternion>
#include "Data/CaseData.h"
#include "Render/Renderer.h"
#include "Render/Camera.h"
class GLWidget: public QOpenGLWidget
{
    Q_OBJECT
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
    void setRenderMode(Mode mode);
    
    // 获取帧率
    float getFPS() const { return current_fps_; }
    
    // LOD控制
    void updateLOD();
    void setLODEnabled(bool enabled);
    bool isLODEnabled() const { return lod_enabled_; }

signals:
    void fpsUpdated(float fps);
    void lodLevelChanged(LODLevel level);

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

    // 帧率计算
    QElapsedTimer fps_timer_;
    int frame_count_ = 0;
    float current_fps_ = 0.0f;
    
    // 操作状态标志
    bool is_interacting_ = false;  // 用户正在操作（鼠标按下或滚轮）
    bool lod_enabled_ = true;  // LOD功能是否启用

    // 计算屏幕中心作为旋转中心
    QVector3D getScreenCenterInWorld() const;
};
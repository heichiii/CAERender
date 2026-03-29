#pragma once

#include <QOpenGLWidget>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QElapsedTimer>
#include <QVector3D>
#include <QQuaternion>
#include <QColor>
#include "Data/CaseData.h"
#include "Data/Octree.h"
#include "Render/Renderer.h"
#include "Render/Camera.h"

#include <cstdint>
#include <limits>
#include <vector>

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
    // void activateField(const std::string& field_name);
    // void activateVectorField(const std::string& field_name);
    // void setVectorRenderMode(VectorRenderMode mode);
    void setRenderMode(Mode mode);
    void setSeedSphereRadius(float radius);
    void setSeedSphereOffset(const QVector3D& offset);
    void setStreamlineSeedCount(int count);
    void setSeedSphereEditingEnabled(bool enabled);
    void markStreamlineCacheDirty();
    void regenerateStreamlinesFromSeedSphere();
    float getSeedSphereRadius() const { return seed_sphere_radius_; }
    QVector3D getSeedSphereOffset() const { return seed_offset_obj_; }
    int getStreamlineSeedCount() const { return streamline_seed_count_; }
    
    // 获取帧率
    float getFPS() const { return current_fps_; }
    
    // LOD控制
    void updateLOD();
    void setLODEnabled(bool enabled);
    bool isLODEnabled() const { return lod_enabled_; }

signals:
    void fpsUpdated(float fps);
    void lodLevelChanged(LODLevel level);
    void seedSphereCenterChanged(const QVector3D& center, bool valid);

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
    LODLevel pending_lod_level_ = LODLevel::HIGH;  // 待切换的 LOD 级别
    int lod_update_delay_ = 0;  // LOD 更新延迟计数器（防抖）

    // 计算屏幕中心作为旋转中心
    QVector3D getScreenCenterInWorld() const;
    void drawColorbarOverlay();
    static QColor mapColor(float t, ColorScheme scheme);

    void rebuildPickingCache(const GPUData* p_gpu_data);
    bool screenPointToObjectRay(const QPoint& pos, QVector3D& ray_origin_obj,
                                QVector3D& ray_dir_obj) const;
    bool screenPointToPlaneInObject(const QPoint& pos, const QVector3D& plane_point_obj,
                                    const QVector3D& plane_normal_obj,
                                    QVector3D& out_point_obj) const;
    bool isMouseOnSeedSphere(const QPoint& pos) const;
    bool pickAtScreenPos(const QPoint& pos);
    QVector3D getEffectiveSeedSphereCenter() const;
    void syncSeedSphereToRenderer();

    Data::Octree pick_octree_;
    const GPUData* pick_gpu_data_ = nullptr;
    std::vector<std::vector<uint32_t>> vertex_to_triangles_;
    bool has_pick_cache_ = false;
    float pick_query_radius_ = 0.01f;
    float pick_ray_tmax_ = 10000.0f;

    // 流线种子球参数（对象坐标）
    bool has_seed_anchor_ = false;
    QVector3D seed_anchor_obj_ = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D seed_offset_obj_ = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D mesh_center_obj_ = QVector3D(0.0f, 0.0f, 0.0f);
    float mesh_diag_ = 1.0f;
    float seed_sphere_radius_ = 0.1f;
    int streamline_seed_count_ = 100;

    bool is_dragging_seed_sphere_ = false;
    QVector3D seed_drag_plane_origin_obj_ = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D seed_drag_plane_normal_obj_ = QVector3D(0.0f, 0.0f, 1.0f);
    QVector3D seed_drag_last_point_obj_ = QVector3D(0.0f, 0.0f, 0.0f);
    bool seed_drag_has_last_point_ = false;
    bool seed_sphere_editing_enabled_ = false;

    // 流线缓存状态
    bool streamline_cache_dirty_ = true;
    bool has_streamline_cache_ = false;
    QVector3D cached_seed_center_ = QVector3D(0.0f, 0.0f, 0.0f);
    float cached_seed_radius_ = -1.0f;
    int cached_seed_count_ = -1;
    const Field* cached_vector_field_ = nullptr;
    const float* cached_mesh_data_ptr_ = nullptr;
    size_t cached_mesh_vertex_count_ = 0;
};
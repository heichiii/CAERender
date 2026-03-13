#include "GLWidget.h"
#include "Loader/LoaderFactory.h"
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace
{
constexpr float kRayEpsilon = 1e-6f;

bool rayTriangleIntersect(const QVector3D& ray_origin,
                          const QVector3D& ray_dir,
                          const QVector3D& v0,
                          const QVector3D& v1,
                          const QVector3D& v2,
                          float& t_out)
{
    const QVector3D edge1 = v1 - v0;
    const QVector3D edge2 = v2 - v0;
    const QVector3D pvec = QVector3D::crossProduct(ray_dir, edge2);
    const float det = QVector3D::dotProduct(edge1, pvec);

    if (std::abs(det) < kRayEpsilon)
        return false;

    const float inv_det = 1.0f / det;
    const QVector3D tvec = ray_origin - v0;
    const float u = QVector3D::dotProduct(tvec, pvec) * inv_det;
    if (u < 0.0f || u > 1.0f)
        return false;

    const QVector3D qvec = QVector3D::crossProduct(tvec, edge1);
    const float v = QVector3D::dotProduct(ray_dir, qvec) * inv_det;
    if (v < 0.0f || (u + v) > 1.0f)
        return false;

    const float t = QVector3D::dotProduct(edge2, qvec) * inv_det;
    if (t <= kRayEpsilon)
        return false;

    t_out = t;
    return true;
}
} // namespace

GLWidget::GLWidget(QWidget* parent) : QOpenGLWidget(parent)
{
}
void GLWidget::loadFile(const std::string& filename)
{
    auto loader = LoaderFactory::createLoader(filename);
    if (!loader)
    {
        qWarning() << "[GLWidget::loadFile] :  Unsupported file format:"
                   << QString::fromStdString(filename);
        return;
    }

    case_data_.steps_.clear();
    TimeStepData time_step;
    time_step.time_ = -1; // 单文件
    time_step.parts_.push_back(loader->load());
    time_step.generateGPUData();
    // time_step.activateField("POINT-p-SCALAR");
    case_data_.steps_.push_back(std::move(time_step));
    setMesh(&case_data_.steps_[0].gpu_data_); // 设置网格数据

    update(); // 触发重绘
}

void GLWidget::setMesh(const GPUData* p_gpu_data)
{
    if (!p_gpu_data)
    {
        qWarning() << "Invalid GPU data pointer";
        return;
    }
    makeCurrent();
    renderer_.setMesh(p_gpu_data);
    rebuildPickingCache(p_gpu_data);
    update(); // 触发重绘
}

void GLWidget::initializeGL()
{
    if (auto* context = QOpenGLContext::currentContext())
    {
        auto* funcs = context->functions();
        const GLubyte* vendor = funcs->glGetString(GL_VENDOR);
        const GLubyte* renderer = funcs->glGetString(GL_RENDERER);
        const GLubyte* version = funcs->glGetString(GL_VERSION);
        qInfo() << "OpenGL Vendor:" << reinterpret_cast<const char*>(vendor);
        qInfo() << "OpenGL Renderer:" << reinterpret_cast<const char*>(renderer);
        qInfo() << "OpenGL Version:" << reinterpret_cast<const char*>(version);
    }
    renderer_.initialize();

    // 初始化旋转中心为原点
    camera_.updateRotationCenter(QVector3D(0.0f, 0.0f, 0.0f));

    // 初始化帧率计时器
    fps_timer_.start();
}

void GLWidget::paintGL()
{
    // 根据操作状态更新LOD级别
    updateLOD();

    renderer_.render(camera_);

    // 更新帧率计算
    frame_count_++;
    if (fps_timer_.elapsed() >= 1000) // 每1秒更新一次FPS
    {
        current_fps_ = frame_count_ * 1000.0f / fps_timer_.elapsed();
        frame_count_ = 0;
        fps_timer_.restart();
        emit fpsUpdated(current_fps_);
    }
}

void GLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    camera_.setAspectRatio(static_cast<float>(w) / static_cast<float>(h));
}

QVector3D GLWidget::getScreenCenterInWorld() const
{
    // 简化版本：屏幕中心映射到世界坐标（此处使用物体原点）
    // 在实际应用中，可以进行光线投射来获取3D场景中的精确位置
    return QVector3D(0.0f, 0.0f, 0.0f);
}

void GLWidget::mousePressEvent(QMouseEvent* event)
{
    last_mouse_pos_ = event->pos();
    mouse_press_pos_ = event->pos(); // 记录按下位置

    if (event->button() == Qt::LeftButton)
    {
        is_rotating_ = true;
        lod_update_delay_ = 0;  // 重置 LOD 延迟计数
        // 每次按下鼠标时更新旋转中心
        camera_.updateRotationCenter(getScreenCenterInWorld());
    }
    else if (event->button() == Qt::MiddleButton)
    {
        is_panning_ = true;
    }
    else if (event->button() == Qt::RightButton)
    {
        is_zooming_ = true;
        // 每次按下右键时更新缩放中心
        camera_.updateRotationCenter(getScreenCenterInWorld());
    }
}

void GLWidget::mouseMoveEvent(QMouseEvent* event)
{
    // 设置操作标志
    is_interacting_ = true;
    QPoint delta = event->pos() - last_mouse_pos_;
    last_mouse_pos_ = event->pos();

    if (is_rotating_)
    {
        // 使用四元数旋转
        camera_.applyRotationDelta(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
    }
    else if (is_panning_)
    {
        camera_.pan(delta.x() * 0.005f, delta.y() * 0.005f);
    }
    else if (is_zooming_)
    {
        camera_.zoom(delta.y() * 0.1f);
    }

    update();
}

void GLWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        const QPoint release_pos = event->pos();
        const int move_distance = (release_pos - mouse_press_pos_).manhattanLength();
        constexpr int kClickThreshold = 4;
        if (move_distance <= kClickThreshold)
        {
            pickAtScreenPos(release_pos);
        }
        is_rotating_ = false;
    }
    else if (event->button() == Qt::MiddleButton)
    {
        is_panning_ = false;
    }
    else if (event->button() == Qt::RightButton)
    {
        is_zooming_ = false;
    }


    is_interacting_ = false;
    renderer_.setLODLevel(LODLevel::HIGH);
    emit lodLevelChanged(LODLevel::HIGH);
    update();
}

void GLWidget::wheelEvent(QWheelEvent* event)
{
    // 每次滚轮时更新缩放中心
    camera_.updateRotationCenter(getScreenCenterInWorld());
    camera_.zoom(event->angleDelta().y() * 0.005f);
    update();
}

void GLWidget::setMeshRenderMode(MeshRenderMode mode)
{
    makeCurrent();
    renderer_.setMeshRenderMode(mode);
    update();
}

void GLWidget::setColorScheme(ColorScheme scheme)
{
    makeCurrent();
    renderer_.setColorScheme(scheme);
    update();
}

void GLWidget::setUseFieldColoring(bool use)
{
    makeCurrent();
    renderer_.setUseFieldColoring(use);
    update();
}

// void GLWidget::setVectorRenderMode(VectorRenderMode mode)
// {
//     makeCurrent();
//     renderer_.setVectorRenderMode(mode);
//     update();
// }

void GLWidget::setRenderMode(Mode mode)
{
    makeCurrent();
    renderer_.setMode(mode);

    update();
}

void GLWidget::updateLOD()
{
    // 如果LOD功能已禁用，始终使用完整LOD
    if (!lod_enabled_)
    {
        if (renderer_.getLODLevel() != LODLevel::HIGH)
        {
            renderer_.setLODLevel(LODLevel::HIGH);
            emit lodLevelChanged(LODLevel::HIGH);
        }
        lod_update_delay_ = 0;
        return;
    }
    
    // 确定目标 LOD 级别
    LODLevel target_level = LODLevel::HIGH;
    if (is_interacting_)
    {
        target_level = LODLevel::MEDIUM;  // 操作时用中细节
    }
    
    // 防抖机制：延迟 2 帧才切换回 HIGH（避免频繁切换）
    if (target_level == LODLevel::HIGH && renderer_.getLODLevel() != LODLevel::HIGH)
    {
        lod_update_delay_++;
        if (lod_update_delay_ < 2)  // 延迟 2 帧
        {
            return;
        }
        lod_update_delay_ = 0;
    }
    else
    {
        lod_update_delay_ = 0;
    }

    // 只在 LOD 级别改变时更新
    if (target_level != renderer_.getLODLevel())
    {
        renderer_.setLODLevel(target_level);
        emit lodLevelChanged(target_level);
    }
}

void GLWidget::setLODEnabled(bool enabled)
{
    lod_enabled_ = enabled;
    if (enabled)
    {
        // 启用后更新LOD
        update();
    }
    else
    {
        // 禁用后切到完整LOD
        renderer_.setLODLevel(LODLevel::HIGH);
        emit lodLevelChanged(LODLevel::HIGH);
        update();
    }
}

void GLWidget::rebuildPickingCache(const GPUData* p_gpu_data)
{
    pick_octree_.clear();
    pick_gpu_data_ = nullptr;
    vertex_to_triangles_.clear();
    has_pick_cache_ = false;
    pick_query_radius_ = 0.01f;
    pick_ray_tmax_ = 10000.0f;
    renderer_.clearPickedPoint();

    if (!p_gpu_data || p_gpu_data->surface_vertices_.empty() || p_gpu_data->indices_.empty())
    {
        return;
    }

    pick_octree_.build(p_gpu_data->surface_vertices_);

    const size_t vertex_count = p_gpu_data->surface_vertices_.size() / 3;
    vertex_to_triangles_.assign(vertex_count, {});
    for (size_t i = 0; i + 2 < p_gpu_data->indices_.size(); i += 3)
    {
        const uint32_t i0 = p_gpu_data->indices_[i + 0];
        const uint32_t i1 = p_gpu_data->indices_[i + 1];
        const uint32_t i2 = p_gpu_data->indices_[i + 2];
        if (i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count)
            continue;

        const uint32_t tri_id = static_cast<uint32_t>(i / 3);
        vertex_to_triangles_[i0].push_back(tri_id);
        vertex_to_triangles_[i1].push_back(tri_id);
        vertex_to_triangles_[i2].push_back(tri_id);
    }

    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float min_z = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    float max_z = std::numeric_limits<float>::lowest();
    for (size_t i = 0; i < vertex_count; ++i)
    {
        const float x = p_gpu_data->surface_vertices_[i * 3 + 0];
        const float y = p_gpu_data->surface_vertices_[i * 3 + 1];
        const float z = p_gpu_data->surface_vertices_[i * 3 + 2];
        min_x = std::min(min_x, x);
        min_y = std::min(min_y, y);
        min_z = std::min(min_z, z);
        max_x = std::max(max_x, x);
        max_y = std::max(max_y, y);
        max_z = std::max(max_z, z);
    }

    const float dx = max_x - min_x;
    const float dy = max_y - min_y;
    const float dz = max_z - min_z;
    const float diag = std::sqrt(dx * dx + dy * dy + dz * dz);
    pick_query_radius_ = std::max(diag * 0.01f, 1e-4f);
    pick_ray_tmax_ = std::max(diag * 3.0f, 1.0f);

    pick_gpu_data_ = p_gpu_data;
    has_pick_cache_ = true;
}

bool GLWidget::screenPointToObjectRay(const QPoint& pos,
                                      QVector3D& ray_origin_obj,
                                      QVector3D& ray_dir_obj) const
{
    if (width() <= 0 || height() <= 0)
        return false;

    const float ndc_x = 2.0f * static_cast<float>(pos.x()) / static_cast<float>(width()) - 1.0f;
    const float ndc_y = 1.0f - 2.0f * static_cast<float>(pos.y()) / static_cast<float>(height());

    bool ok = false;
    const QMatrix4x4 inv_vp = (camera_.getProjectionMatrix() * camera_.getViewMatrix()).inverted(&ok);
    if (!ok)
        return false;

    QVector4D near_world = inv_vp * QVector4D(ndc_x, ndc_y, -1.0f, 1.0f);
    QVector4D far_world = inv_vp * QVector4D(ndc_x, ndc_y, 1.0f, 1.0f);
    if (std::abs(near_world.w()) < kRayEpsilon || std::abs(far_world.w()) < kRayEpsilon)
        return false;

    near_world /= near_world.w();
    far_world /= far_world.w();

    const QMatrix4x4 inv_model = camera_.getModelMatrix().inverted(&ok);
    if (!ok)
        return false;

    QVector4D near_obj4 = inv_model * near_world;
    QVector4D far_obj4 = inv_model * far_world;
    if (std::abs(near_obj4.w()) < kRayEpsilon || std::abs(far_obj4.w()) < kRayEpsilon)
        return false;

    near_obj4 /= near_obj4.w();
    far_obj4 /= far_obj4.w();

    ray_origin_obj = near_obj4.toVector3D();
    ray_dir_obj = (far_obj4 - near_obj4).toVector3D();
    if (ray_dir_obj.lengthSquared() < kRayEpsilon)
        return false;

    ray_dir_obj.normalize();
    return true;
}

bool GLWidget::pickAtScreenPos(const QPoint& pos)
{
    if (!has_pick_cache_ || !pick_gpu_data_)
    {
        renderer_.clearPickedPoint();
        return false;
    }

    QVector3D ray_origin_obj;
    QVector3D ray_dir_obj;
    if (!screenPointToObjectRay(pos, ray_origin_obj, ray_dir_obj))
    {
        renderer_.clearPickedPoint();
        return false;
    }

    const float origin_arr[3] = {ray_origin_obj.x(), ray_origin_obj.y(), ray_origin_obj.z()};
    const float dir_arr[3] = {ray_dir_obj.x(), ray_dir_obj.y(), ray_dir_obj.z()};
    auto candidate_vertices =
        pick_octree_.findRayCandidates(origin_arr, dir_arr, pick_query_radius_, pick_ray_tmax_, 256);

    if (candidate_vertices.empty())
    {
        renderer_.clearPickedPoint();
        return false;
    }

    std::unordered_set<uint32_t> candidate_triangles;
    candidate_triangles.reserve(candidate_vertices.size() * 2);
    for (int v_idx : candidate_vertices)
    {
        if (v_idx < 0 || static_cast<size_t>(v_idx) >= vertex_to_triangles_.size())
            continue;
        for (uint32_t tri_id : vertex_to_triangles_[static_cast<size_t>(v_idx)])
            candidate_triangles.insert(tri_id);
    }

    const auto& vertices = pick_gpu_data_->surface_vertices_;
    const auto& indices = pick_gpu_data_->indices_;

    float closest_t = std::numeric_limits<float>::max();
    QVector3D hit_point;
    bool hit = false;

    for (uint32_t tri_id : candidate_triangles)
    {
        const size_t base = static_cast<size_t>(tri_id) * 3;
        if (base + 2 >= indices.size())
            continue;

        const uint32_t i0 = indices[base + 0];
        const uint32_t i1 = indices[base + 1];
        const uint32_t i2 = indices[base + 2];
        const size_t p0 = static_cast<size_t>(i0) * 3;
        const size_t p1 = static_cast<size_t>(i1) * 3;
        const size_t p2 = static_cast<size_t>(i2) * 3;
        if (p2 + 2 >= vertices.size())
            continue;

        const QVector3D v0(vertices[p0 + 0], vertices[p0 + 1], vertices[p0 + 2]);
        const QVector3D v1(vertices[p1 + 0], vertices[p1 + 1], vertices[p1 + 2]);
        const QVector3D v2(vertices[p2 + 0], vertices[p2 + 1], vertices[p2 + 2]);

        float t = 0.0f;
        if (!rayTriangleIntersect(ray_origin_obj, ray_dir_obj, v0, v1, v2, t))
            continue;

        if (t < closest_t)
        {
            closest_t = t;
            hit_point = ray_origin_obj + ray_dir_obj * t;
            hit = true;
        }
    }

    if (hit)
    {
        renderer_.setPickedPoint(hit_point);
        return true;
    }

    renderer_.clearPickedPoint();
    return false;
}

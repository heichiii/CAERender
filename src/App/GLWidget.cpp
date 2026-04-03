#include "GLWidget.h"
#include "Loader/LoaderFactory.h"
#include "TestTool/Profiler.h"
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPainter>
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
    PROFILE_CODE
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
    drawColorbarOverlay();

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

QColor GLWidget::mapColor(float t, ColorScheme scheme)
{
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    auto toRgb = [](float r, float g, float b) -> QColor
    {
        return QColor::fromRgbF(std::clamp(r, 0.0f, 1.0f), std::clamp(g, 0.0f, 1.0f),
                                std::clamp(b, 0.0f, 1.0f));
    };

    switch (scheme)
    {
        case ColorScheme::RAINBOW:
        {
            if (clamped < 0.125f)
            {
                return toRgb(0.0f, 0.0f, 0.5f + 0.5f * (clamped / 0.125f));
            }
            if (clamped < 0.375f)
            {
                return toRgb(0.0f, (clamped - 0.125f) / 0.25f, 1.0f);
            }
            if (clamped < 0.625f)
            {
                const float s = (clamped - 0.375f) / 0.25f;
                return toRgb(s, 1.0f, 1.0f - s);
            }
            if (clamped < 0.875f)
            {
                return toRgb(1.0f, 1.0f - (clamped - 0.625f) / 0.25f, 0.0f);
            }
            return toRgb(1.0f - 0.5f * (clamped - 0.875f) / 0.125f, 0.0f, 0.0f);
        }
        case ColorScheme::HEATMAP:
        {
            if (clamped < 0.25f)
            {
                return toRgb(clamped * 4.0f, 0.0f, 0.0f);
            }
            if (clamped < 0.5f)
            {
                return toRgb(1.0f, (clamped - 0.25f) * 4.0f, 0.0f);
            }
            if (clamped < 0.75f)
            {
                return toRgb(1.0f, 1.0f, (clamped - 0.5f) * 4.0f);
            }
            return toRgb(1.0f, 1.0f, 1.0f);
        }
        case ColorScheme::COOL_WARM:
        {
            if (clamped < 0.5f)
            {
                const float s = clamped * 2.0f;
                return toRgb(s, s, 1.0f);
            }
            const float s = (clamped - 0.5f) * 2.0f;
            return toRgb(1.0f, 1.0f - s, 1.0f - s);
        }
        case ColorScheme::GRAYSCALE:
            return toRgb(clamped, clamped, clamped);
        case ColorScheme::BLUE_WHITE_RED:
        {
            if (clamped < 0.5f)
            {
                const float s = clamped * 2.0f;
                return toRgb(0.0f + s, 0.0f + s, 0.5f + 0.5f * s);
            }
            const float s = (clamped - 0.5f) * 2.0f;
            return toRgb(1.0f - 0.5f * s, 1.0f - s, 1.0f - s);
        }
        default:
            return toRgb(clamped, clamped, clamped);
    }
}

void GLWidget::drawColorbarOverlay()
{
    float value_min = 0.0f;
    float value_max = 1.0f;
    ColorScheme scheme = ColorScheme::RAINBOW;
    if (!renderer_.getColorbarRange(value_min, value_max, scheme))
    {
        return;
    }

    if (!std::isfinite(value_min) || !std::isfinite(value_max))
    {
        return;
    }
    if (std::abs(value_max - value_min) < 1e-12f)
    {
        value_max = value_min + 1.0f;
    }

    const int bar_width = 220;
    const int bar_height = 18;
    const int margin = 16;
    const int text_width = 62;
    const QRect panel_rect(width() - bar_width - text_width - margin, margin, bar_width + text_width,
                           72);
    const QRect bar_rect(panel_rect.left() + 8, panel_rect.top() + 30, bar_width, bar_height);

    QPainter painter(this);
    painter.beginNativePainting();
    painter.endNativePainting();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 110));
    painter.drawRoundedRect(panel_rect, 8.0, 8.0);

    QLinearGradient gradient(bar_rect.topLeft(), bar_rect.topRight());
    constexpr int kColorSamples = 32;
    for (int i = 0; i <= kColorSamples; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(kColorSamples);
        gradient.setColorAt(t, mapColor(t, scheme));
    }
    painter.fillRect(bar_rect, gradient);
    painter.setPen(QPen(QColor(230, 230, 230, 220), 1));
    painter.drawRect(bar_rect);

    auto formatValue = [](float value) -> QString
    {
        const float abs_value = std::abs(value);
        if (abs_value >= 10000.0f || (abs_value > 0.0f && abs_value < 0.001f))
        {
            return QString::number(value, 'e', 2);
        }
        return QString::number(value, 'f', 3);
    };

    QString title;
    if (renderer_.getMode() == Mode::BASIC)
    {
        bool vector_magnitude_mode = false;
        if (!case_data_.steps_.empty() && !case_data_.steps_[0].parts_.empty())
        {
            const Field* active_field = case_data_.steps_[0].parts_[0].active_field_;
            vector_magnitude_mode = active_field && active_field->type_ == Type::VECTOR;
        }
        title = vector_magnitude_mode ? "Vector Magnitude" : "Scalar";
    }
    else if (renderer_.getMode() == Mode::ARROW)
    {
        title = "Vector Magnitude";
    }
    else
    {
        title = "Streamline Magnitude";
    }

    painter.setPen(QColor(245, 245, 245));
    painter.drawText(QRect(panel_rect.left() + 8, panel_rect.top() + 8, panel_rect.width() - 16, 18),
                     Qt::AlignLeft | Qt::AlignVCenter, title);

    painter.drawText(QRect(bar_rect.left(), bar_rect.bottom() + 4, 60, 16),
                     Qt::AlignLeft | Qt::AlignVCenter, formatValue(value_min));
    painter.drawText(QRect(bar_rect.right() - 60, bar_rect.bottom() + 4, 60, 16),
                     Qt::AlignRight | Qt::AlignVCenter, formatValue(value_max));

    painter.beginNativePainting();
    painter.endNativePainting();
    painter.end();

    // 避免 QPainter 覆盖绘制残留状态影响下一帧 3D 渲染。
    if (auto* context = QOpenGLContext::currentContext())
    {
        auto* funcs = context->functions();
        funcs->glEnable(GL_DEPTH_TEST);
        funcs->glDepthMask(GL_TRUE);
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
    is_dragging_seed_sphere_ = false;
    seed_drag_has_last_point_ = false;

    if (event->button() == Qt::LeftButton)
    {
        if (seed_sphere_editing_enabled_ && has_pick_cache_ && isMouseOnSeedSphere(event->pos()))
        {
            bool ok = false;
            const QMatrix4x4 inv_view = camera_.getViewMatrix().inverted(&ok);
            if (ok)
            {
                QVector3D forward_world =
                    (inv_view * QVector4D(0.0f, 0.0f, -1.0f, 0.0f)).toVector3D();
                if (forward_world.lengthSquared() > kRayEpsilon)
                {
                    forward_world.normalize();

                    const QMatrix4x4 inv_model = camera_.getModelMatrix().inverted(&ok);
                    if (ok)
                    {
                        QVector3D normal_obj =
                            (inv_model * QVector4D(forward_world, 0.0f)).toVector3D();
                        if (normal_obj.lengthSquared() > kRayEpsilon)
                        {
                            normal_obj.normalize();
                            const QVector3D center_obj = getEffectiveSeedSphereCenter();
                            QVector3D point_obj;
                            if (screenPointToPlaneInObject(event->pos(), center_obj, normal_obj,
                                                           point_obj))
                            {
                                is_dragging_seed_sphere_ = true;
                                seed_drag_plane_origin_obj_ = center_obj;
                                seed_drag_plane_normal_obj_ = normal_obj;
                                seed_drag_last_point_obj_ = point_obj;
                                seed_drag_has_last_point_ = true;
                            }
                        }
                    }
                }
            }
        }

        if (!is_dragging_seed_sphere_)
        {
            is_rotating_ = true;
            lod_update_delay_ = 0; // 重置 LOD 延迟计数
            // 每次按下鼠标时更新旋转中心
            camera_.updateRotationCenter(getScreenCenterInWorld());
        }
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
    QPoint delta = event->pos() - last_mouse_pos_;
    last_mouse_pos_ = event->pos();
    bool camera_motion_for_lod = false;

    if (is_dragging_seed_sphere_)
    {
        QVector3D point_obj;
        if (screenPointToPlaneInObject(event->pos(), seed_drag_plane_origin_obj_,
                                       seed_drag_plane_normal_obj_, point_obj) &&
            seed_drag_has_last_point_)
        {
            const QVector3D move_delta = point_obj - seed_drag_last_point_obj_;
            seed_anchor_obj_ += move_delta;
            seed_drag_plane_origin_obj_ += move_delta;
            seed_drag_last_point_obj_ = point_obj;
            markStreamlineCacheDirty();
            syncSeedSphereToRenderer();
        }
    }
    else if (is_rotating_)
    {
        // 使用四元数旋转
        camera_.applyRotationDelta(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
        camera_motion_for_lod = true;
    }
    else if (is_panning_)
    {
        camera_.pan(delta.x() * 0.005f, delta.y() * 0.005f);
        camera_motion_for_lod = true;
    }
    else if (is_zooming_)
    {
        camera_.zoom(delta.y() * 0.1f);
    }

    if (camera_motion_for_lod)
    {
        is_lod_interacting_ = true;
    }

    update();
}

void GLWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        is_rotating_ = false;
        is_dragging_seed_sphere_ = false;
        seed_drag_has_last_point_ = false;
    }
    else if (event->button() == Qt::MiddleButton)
    {
        is_panning_ = false;
    }
    else if (event->button() == Qt::RightButton)
    {
        is_zooming_ = false;
    }
    is_lod_interacting_ = false;
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

void GLWidget::setMeshVisible(bool visible)
{
    makeCurrent();
    renderer_.setMeshVisible(visible);
    update();
}

void GLWidget::setStreamlineVisible(bool visible)
{
    makeCurrent();
    renderer_.setStreamlineVisible(visible);
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

    if (mode == Mode::STREAMLINE)
    {
        if (!case_data_.steps_.empty())
        {
            auto& step = case_data_.steps_[0];
            step.gpu_data_.streamline_vertices_.clear();
            step.gpu_data_.streamline_magnitudes_.clear();
            step.gpu_data_.streamline_line_starts_.clear();
            step.gpu_data_.streamline_line_counts_.clear();
            step.gpu_data_.streamline_magnitude_min_ = 0.0f;
            step.gpu_data_.streamline_magnitude_max_ = 1.0f;
            // renderer_.setMode(Mode::STREAMLINE);
        }
        syncSeedSphereToRenderer();
    }
    update();
}

void GLWidget::setSeedSphereRadius(float radius)
{
    const float min_radius = std::max(mesh_diag_ * 1e-4f, 1e-5f);
    const float new_radius = std::max(radius, min_radius);
    if (std::abs(new_radius - seed_sphere_radius_) > 1e-8f)
    {
        seed_sphere_radius_ = new_radius;
        markStreamlineCacheDirty();
    }
    syncSeedSphereToRenderer();
}

void GLWidget::setSeedSphereOffset(const QVector3D& offset)
{
    if ((seed_offset_obj_ - offset).lengthSquared() > 1e-12f)
    {
        seed_offset_obj_ = offset;
        markStreamlineCacheDirty();
    }
    syncSeedSphereToRenderer();
}

void GLWidget::setStreamlineSeedCount(int count)
{
    const int new_count = std::max(1, count);
    if (new_count != streamline_seed_count_)
    {
        streamline_seed_count_ = new_count;
        markStreamlineCacheDirty();
    }
}

void GLWidget::setSeedSphereEditingEnabled(bool enabled)
{
    seed_sphere_editing_enabled_ = enabled;
    syncSeedSphereToRenderer();
    update();
}

void GLWidget::markStreamlineCacheDirty()
{
    streamline_cache_dirty_ = true;
}

void GLWidget::regenerateStreamlinesFromSeedSphere()
{
    if (case_data_.steps_.empty() || case_data_.steps_[0].parts_.empty())
    {
        return;
    }

    auto& step = case_data_.steps_[0];
    Field* active_field = step.parts_[0].active_field_;
    if (!active_field || active_field->type_ != Type::VECTOR)
    {
        return;
    }

    const QVector3D center = getEffectiveSeedSphereCenter();
    const float* mesh_data_ptr =
        step.parts_[0].vertices_.empty() ? nullptr : step.parts_[0].vertices_.data();
    const size_t mesh_vertex_count = step.parts_[0].vertices_.size();

    const bool same_center = (center - cached_seed_center_).lengthSquared() <= 1e-12f;
    const bool same_radius = std::abs(seed_sphere_radius_ - cached_seed_radius_) <= 1e-8f;
    const bool same_count = streamline_seed_count_ == cached_seed_count_;
    const bool same_field = active_field == cached_vector_field_;
    const bool same_mesh =
        mesh_data_ptr == cached_mesh_data_ptr_ && mesh_vertex_count == cached_mesh_vertex_count_;

    if (!streamline_cache_dirty_ && has_streamline_cache_ && same_center && same_radius &&
        same_count && same_field && same_mesh)
    {
        makeCurrent();
        if (renderer_.getMode() != Mode::STREAMLINE)
        {
            renderer_.setMode(Mode::STREAMLINE);
        }
        update();
        return;
    }

    step.updateStreamlineBufferFromSphere(center, seed_sphere_radius_, streamline_seed_count_);

    cached_seed_center_ = center;
    cached_seed_radius_ = seed_sphere_radius_;
    cached_seed_count_ = streamline_seed_count_;
    cached_vector_field_ = active_field;
    cached_mesh_data_ptr_ = mesh_data_ptr;
    cached_mesh_vertex_count_ = mesh_vertex_count;
    has_streamline_cache_ = true;
    streamline_cache_dirty_ = false;

    makeCurrent();
    renderer_.setMode(Mode::STREAMLINE);
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
    if (is_lod_interacting_)
    {
        target_level = interaction_lod_level_;
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

void GLWidget::setInteractionLODLevel(LODLevel level)
{
    interaction_lod_level_ = level;
    update();
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
    renderer_.clearSeedSphere();
    has_seed_anchor_ = false;
    seed_anchor_obj_ = QVector3D(0.0f, 0.0f, 0.0f);
    mesh_center_obj_ = QVector3D(0.0f, 0.0f, 0.0f);
    mesh_diag_ = 1.0f;
    seed_sphere_radius_ = 0.1f;
    seed_offset_obj_ = QVector3D(0.0f, 0.0f, 0.0f);
    markStreamlineCacheDirty();
    has_streamline_cache_ = false;
    cached_vector_field_ = nullptr;
    cached_mesh_data_ptr_ = nullptr;
    cached_mesh_vertex_count_ = 0;

    if (!p_gpu_data || p_gpu_data->surface_vertices_.empty() || p_gpu_data->indices_.empty())
    {
        emit seedSphereCenterChanged(QVector3D(0.0f, 0.0f, 0.0f), false);
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
    mesh_diag_ = std::max(diag, 1e-4f);
    mesh_center_obj_ = QVector3D((min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f,
                                 (min_z + max_z) * 0.5f);
    pick_query_radius_ = std::max(diag * 0.01f, 1e-4f);
    pick_ray_tmax_ = std::max(diag * 3.0f, 1.0f);
    seed_sphere_radius_ = std::max(mesh_diag_ * 0.08f, 1e-4f);
    has_seed_anchor_ = true;
    seed_anchor_obj_ = mesh_center_obj_;

    pick_gpu_data_ = p_gpu_data;
    has_pick_cache_ = true;
    syncSeedSphereToRenderer();
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

bool GLWidget::screenPointToPlaneInObject(const QPoint& pos,
                                          const QVector3D& plane_point_obj,
                                          const QVector3D& plane_normal_obj,
                                          QVector3D& out_point_obj) const
{
    QVector3D ray_origin_obj;
    QVector3D ray_dir_obj;
    if (!screenPointToObjectRay(pos, ray_origin_obj, ray_dir_obj))
    {
        return false;
    }

    const float denom = QVector3D::dotProduct(plane_normal_obj, ray_dir_obj);
    if (std::abs(denom) < kRayEpsilon)
    {
        return false;
    }

    const float t = QVector3D::dotProduct(plane_normal_obj, (plane_point_obj - ray_origin_obj)) /
                    denom;
    if (t < 0.0f)
    {
        return false;
    }

    out_point_obj = ray_origin_obj + ray_dir_obj * t;
    return true;
}

bool GLWidget::isMouseOnSeedSphere(const QPoint& pos) const
{
    if (!seed_sphere_editing_enabled_ || !has_pick_cache_)
    {
        return false;
    }

    QVector3D ray_origin_obj;
    QVector3D ray_dir_obj;
    if (!screenPointToObjectRay(pos, ray_origin_obj, ray_dir_obj))
    {
        return false;
    }

    const QVector3D center = getEffectiveSeedSphereCenter();
    const float radius = std::max(seed_sphere_radius_, 1e-6f);
    const QVector3D oc = ray_origin_obj - center;

    // 归一化方向下: t^2 + 2*b*t + c = 0
    const float b = QVector3D::dotProduct(oc, ray_dir_obj);
    const float c = QVector3D::dotProduct(oc, oc) - radius * radius;
    const float discriminant = b * b - c;

    if (discriminant < 0.0f)
    {
        return false;
    }

    const float sqrt_disc = std::sqrt(discriminant);
    const float t0 = -b - sqrt_disc;
    const float t1 = -b + sqrt_disc;
    return t0 >= 0.0f || t1 >= 0.0f;
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

        has_seed_anchor_ = true;
        seed_anchor_obj_ = hit_point;
        syncSeedSphereToRenderer();

        if (renderer_.getMode() == Mode::STREAMLINE)
        {
            regenerateStreamlinesFromSeedSphere();
        }

        return true;
    }

    renderer_.clearPickedPoint();
    return false;
}

QVector3D GLWidget::getEffectiveSeedSphereCenter() const
{
    if (!has_seed_anchor_)
    {
        return mesh_center_obj_ + seed_offset_obj_;
    }
    return seed_anchor_obj_ + seed_offset_obj_;
}

void GLWidget::syncSeedSphereToRenderer()
{
    if (!seed_sphere_editing_enabled_)
    {
        renderer_.clearSeedSphere();
        update();
        return;
    }

    if (!has_pick_cache_)
    {
        renderer_.clearSeedSphere();
        emit seedSphereCenterChanged(QVector3D(0.0f, 0.0f, 0.0f), false);
        update();
        return;
    }

    const QVector3D center = getEffectiveSeedSphereCenter();
    renderer_.setSeedSphere(center, seed_sphere_radius_, true);
    emit seedSphereCenterChanged(center, true);
    update();
}

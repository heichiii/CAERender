#include "GLWidget.h"
#include "Loader/LoaderFactory.h"
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

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

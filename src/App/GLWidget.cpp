#include "GLWidget.h"
#include "Loader/LoaderFactory.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>

GLWidget::GLWidget(QWidget* parent): QOpenGLWidget(parent)
{
}
void GLWidget::loadFile(const std::string& filename)
{
    auto loader = LoaderFactory::createLoader(filename);
    if (!loader)
    {
        qWarning() << "[GLWidget::loadFile] :  Unsupported file format:" << QString::fromStdString(filename);
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
    if(!p_gpu_data)
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
}

void GLWidget::paintGL()
{
    renderer_.render(camera_);
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
    mouse_press_pos_ = event->pos();  // 记录按下位置
    
    if (event->button() == Qt::LeftButton)
    {
        is_rotating_ = true;
        // 每次按下鼠标时更新旋转中心
        camera_.updateRotationCenter(getScreenCenterInWorld());
        qDebug() << "Rotation center updated at: " 
                 << camera_.getRotationCenter().x() << ","
                 << camera_.getRotationCenter().y() << ","
                 << camera_.getRotationCenter().z();
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
        qDebug() << "Zoom center updated at: " 
                 << camera_.getRotationCenter().x() << ","
                 << camera_.getRotationCenter().y() << ","
                 << camera_.getRotationCenter().z();
    }
}

void GLWidget::mouseMoveEvent(QMouseEvent* event)
{
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
    else if (event->button() == Qt::RightButton)
    {
        is_panning_ = false;
    }
    else if (event->button() == Qt::MiddleButton)
    {
        is_zooming_ = false;
    }
}

void GLWidget::wheelEvent(QWheelEvent* event)
{
    // 每次滚轮时更新缩放中心
    camera_.updateRotationCenter(getScreenCenterInWorld());
    camera_.zoom(event->angleDelta().y() * 0.01f);
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

void GLWidget::setVectorRenderMode(VectorRenderMode mode)
{
    makeCurrent();
    renderer_.setVectorRenderMode(mode);
    update();
}

void GLWidget::setRenderMode(Mode mode)
{
    makeCurrent();
    renderer_.setMode(mode);
    
    update();
}

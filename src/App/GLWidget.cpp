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
}

void GLWidget::paintGL()
{
    renderer_.render(camera_);
}

void GLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);

}

void GLWidget::mousePressEvent(QMouseEvent* event)
{
    last_mouse_pos_ = event->pos();
    if (event->button() == Qt::LeftButton)
        is_rotating_ = true;
    else if (event->button() == Qt::RightButton)
        is_panning_ = true;
    else if (event->button() == Qt::MiddleButton)
        is_zooming_ = true;
}

void GLWidget::mouseMoveEvent(QMouseEvent* event)
{
    QPoint delta = event->pos() - last_mouse_pos_;
    last_mouse_pos_ = event->pos();

    if (is_rotating_)
        camera_.rotate(delta.x(), delta.y());
    else if (is_panning_)
        camera_.pan(delta.x() * 0.01f, -delta.y() * 0.01f);
    else if (is_zooming_)
        camera_.zoom(delta.y() * 0.1f);

    update();
}

void GLWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        is_rotating_ = false;
    else if (event->button() == Qt::RightButton)
        is_panning_ = false;
    else if (event->button() == Qt::MiddleButton)
        is_zooming_ = false;
}

void GLWidget::wheelEvent(QWheelEvent* event)
{
    camera_.zoom(event->angleDelta().y() * 0.01f);
    update();
}

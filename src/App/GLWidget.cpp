#include "GLWidget.h"
#include "Loader/LoaderFactory.h"

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
    renderer_.initialize();
}

void GLWidget::paintGL()
{
    renderer_.render();
}

void GLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}
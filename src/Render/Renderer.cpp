#include "Renderer.h"
#include <QDebug>
#include <QDir>
Renderer::Renderer():
    gpu_data_(nullptr),
    vbo_(QOpenGLBuffer::VertexBuffer),
    normal_(QOpenGLBuffer::VertexBuffer),
    ebo_(QOpenGLBuffer::IndexBuffer)
{
}

void Renderer::initialize()
{
    initializeOpenGLFunctions();

    shader_program_ = std::make_unique<ShaderProgram>();
    qInfo() << "Current working directory:" << QDir::currentPath();
    if (!shader_program_->createFromFiles("../src/Shader/basic.vert", "../src/Shader/basic.frag"))
    {
        qWarning() << "Failed to create shader program";
    }
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);  // 开启双面光照，禁用背面剔除
}

void Renderer::render(const Camera& camera)
{
    if (!gpu_data_)
    {
        return; // 没有数据可渲染
    }

    // 清空颜色和深度缓冲区
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader_program_->bind();

    // 使用Camera提供的模型矩阵
    QMatrix4x4 model = camera.getModelMatrix();
    QMatrix4x4 view = camera.getViewMatrix();
    QMatrix4x4 projection = camera.getProjectionMatrix();
    
    shader_program_->getProgram()->setUniformValue("u_model", model);
    shader_program_->getProgram()->setUniformValue("u_view", view);
    shader_program_->getProgram()->setUniformValue("u_projection", projection);

    shader_program_->getProgram()->setUniformValue("u_light_pos", QVector3D(5.0f, 5.0f, 15.0f));
    shader_program_->getProgram()->setUniformValue("u_view_pos", QVector3D(0.0f, 0.0f, 10.0f));
    shader_program_->getProgram()->setUniformValue("u_object_color", QVector3D(0.8f, 0.8f, 0.9f));




    vao_.bind();

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(gpu_data_->indices_.size()), GL_UNSIGNED_INT, nullptr);

    vao_.release();
    shader_program_->release();
}


void Renderer::setMesh(const GPUData* p_gpu_data)
{
    //TODO:添加更多buffer
    if (!p_gpu_data)
    {
        qWarning() << "Invalid GPU data pointer";
        return;
    }
    gpu_data_ = p_gpu_data;

    if (vao_.isCreated())
    {
        vao_.destroy();
    }
    if (vbo_.isCreated())
    {
        vbo_.destroy();
    }
    if (ebo_.isCreated())
    {
        ebo_.destroy();
    }

    vao_.create();
    vao_.bind();
    //position
    vbo_.create();
    vbo_.bind();
    vbo_.allocate(gpu_data_->surface_vertices_.data(), static_cast<int>(gpu_data_->surface_vertices_.size() * sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    //normal
    normal_.create();
    normal_.bind();
    normal_.allocate(gpu_data_->normals_.data(), static_cast<int>(gpu_data_->normals_.size() * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    //ebo
    ebo_.create();
    ebo_.bind();
    ebo_.allocate(gpu_data_->indices_.data(), static_cast<int>(gpu_data_->indices_.size() * sizeof(uint32_t)));


    vao_.release();


}

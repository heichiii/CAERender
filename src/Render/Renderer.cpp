#include "Renderer.h"
#include <QDebug>
#include <QDir>
Renderer::Renderer()
    : gpu_data_(nullptr), vbo_(QOpenGLBuffer::VertexBuffer), normal_(QOpenGLBuffer::VertexBuffer),
      ebo_(QOpenGLBuffer::IndexBuffer), arrow_pos_buffer_(QOpenGLBuffer::VertexBuffer),
      arrow_dir_buffer_(QOpenGLBuffer::VertexBuffer),
      arrow_mag_buffer_(QOpenGLBuffer::VertexBuffer), mesh_render_mode_(MeshRenderMode::SOLID),
      color_scheme_(ColorScheme::RAINBOW), use_field_coloring_(false),
      vector_render_mode_(VectorRenderMode::ARROW), mode_(Mode::BASIC)
// render_vector_(false)
{
}

void Renderer::initialize()
{
    initializeOpenGLFunctions();

    shader_program_ = std::make_unique<ShaderProgram>();
    qInfo() << "Current working directory:" << QDir::currentPath();
    if (!shader_program_->createFromFiles("../src/Shader/basic.vert", "../src/Shader/basic.frag"))
    {
        qWarning() << "Failed to create basic shader program";
    }

    // 初始化箭头着色器程序
    arrow_shader_program_ = std::make_unique<ShaderProgram>();
    if (!arrow_shader_program_->createFromFiles(
            "../src/Shader/arrow.vert", "../src/Shader/arrow.frag", "../src/Shader/arrow.geom"))
    {
        qWarning() << "Failed to create arrow shader program";
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); // 开启双面光照，禁用背面剔除
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
    if (mode_ == Mode::BASIC)
    {
        renderBasic(camera);
    }
    else if (mode_ == Mode::ARROW)
    {
        renderArrows(camera);
    }
    else if (mode_ == Mode::STREAMLINE)
    {
        // renderStreamlines(camera);
    }

}


void Renderer::setMesh(const GPUData* p_gpu_data)
{
    if (!p_gpu_data)
    {
        qWarning() << "Invalid GPU data pointer";
        return;
    }
    gpu_data_ = p_gpu_data;

    updateBasicBuffers();
}

void Renderer::setMeshRenderMode(MeshRenderMode mode)
{
    mesh_render_mode_ = mode;
}

void Renderer::setColorScheme(ColorScheme scheme)
{
    color_scheme_ = scheme;
}

void Renderer::setUseFieldColoring(bool use)
{
    use_field_coloring_ = use;
}

void Renderer::setVectorRenderMode(VectorRenderMode mode)
{
    vector_render_mode_ = mode;
}

void Renderer::setMode(Mode mode)
{
    mode_ = mode;
    if(mode_ == Mode::BASIC)
    {
        updateBasicBuffers();
    }
    else if(mode_ == Mode::ARROW)
    {
        updateArrowBuffers();
    }
    
}
// {
//     render_vector_ = render_vector;
//     if (render_vector)
//     {
//         updateArrowBuffers();
//     }
//}
void Renderer::updateBasicBuffers()
{
    if (!gpu_data_)
    {
        qWarning() << "No GPU data available";
        return;
    }
    if (vao_.isCreated())
    {
        vao_.destroy();
    }
    if (vbo_.isCreated())
    {
        vbo_.destroy();
    }
    if (normal_.isCreated())
    {
        normal_.destroy();
    }
    if (scalar_fields_.isCreated())
    {
        scalar_fields_.destroy();
    }
    if (ebo_.isCreated())
    {
        ebo_.destroy();
    }

    vao_.create();
    vao_.bind();
    // position
    vbo_.create();
    vbo_.bind();
    vbo_.allocate(gpu_data_->surface_vertices_.data(),
                  static_cast<int>(gpu_data_->surface_vertices_.size() * sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    // normal
    normal_.create();
    normal_.bind();
    normal_.allocate(gpu_data_->normals_.data(),
                     static_cast<int>(gpu_data_->normals_.size() * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    // scalar fields
    scalar_fields_.create();
    scalar_fields_.bind();
    std::vector<float> default_scalar(gpu_data_->surface_vertices_.size() / 3, 0.0f);
    if (gpu_data_->scalar_fields_.empty())
    {
        qDebug() << "Warning: scalar_fields_ is empty, using default values";
        scalar_fields_.allocate(default_scalar.data(),
                                static_cast<int>(default_scalar.size() * sizeof(float)));
    }
    else
    {
        qDebug() << "scalar_fields_ size:" << gpu_data_->scalar_fields_.size()
                 << "min:" << gpu_data_->scalar_min_ << "max:" << gpu_data_->scalar_max_;
        scalar_fields_.allocate(gpu_data_->scalar_fields_.data(),
                                static_cast<int>(gpu_data_->scalar_fields_.size() * sizeof(float)));
    }
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 1 * sizeof(float), nullptr);
    // ebo
    ebo_.create();
    ebo_.bind();
    ebo_.allocate(gpu_data_->indices_.data(),
                  static_cast<int>(gpu_data_->indices_.size() * sizeof(uint32_t)));


    vao_.release();
}
void Renderer::updateArrowBuffers()
{
    if (!gpu_data_ || gpu_data_->vector_field_positions_.empty())
    {
        qWarning() << "No vector field data available";
        return;
    }

    size_t num_vectors = gpu_data_->vector_field_positions_.size() / 3;

    qDebug() << "Updating arrow buffers:" << num_vectors << "vectors";
    qDebug() << "Vector magnitude range: [" << gpu_data_->vector_magnitude_min_ << ", "
             << gpu_data_->vector_magnitude_max_ << "]";

    if (arrow_vao_.isCreated())
    {
        arrow_vao_.destroy();
    }
    if (arrow_pos_buffer_.isCreated())
    {
        arrow_pos_buffer_.destroy();
    }
    if (arrow_dir_buffer_.isCreated())
    {
        arrow_dir_buffer_.destroy();
    }
    if (arrow_mag_buffer_.isCreated())
    {
        arrow_mag_buffer_.destroy();
    }

    arrow_vao_.create();
    arrow_vao_.bind();

    // 位置缓冲
    arrow_pos_buffer_.create();
    arrow_pos_buffer_.bind();
    arrow_pos_buffer_.allocate(
        gpu_data_->vector_field_positions_.data(),
        static_cast<int>(gpu_data_->vector_field_positions_.size() * sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    // 方向缓冲
    arrow_dir_buffer_.create();
    arrow_dir_buffer_.bind();
    arrow_dir_buffer_.allocate(
        gpu_data_->vector_field_directions_.data(),
        static_cast<int>(gpu_data_->vector_field_directions_.size() * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    // 幅值缓冲
    arrow_mag_buffer_.create();
    arrow_mag_buffer_.bind();
    arrow_mag_buffer_.allocate(
        gpu_data_->vector_field_magnitudes_.data(),
        static_cast<int>(gpu_data_->vector_field_magnitudes_.size() * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 1 * sizeof(float), nullptr);

    arrow_vao_.release();
}
void Renderer::renderBasic(const Camera& camera)
{
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

    shader_program_->getProgram()->setUniformValue("u_scalar_min", gpu_data_->scalar_min_);
    shader_program_->getProgram()->setUniformValue("u_scalar_max", gpu_data_->scalar_max_);

    // 设置渲染参数
    shader_program_->getProgram()->setUniformValue("u_color_scheme",
                                                   static_cast<int>(color_scheme_));
    shader_program_->getProgram()->setUniformValue("u_use_field_coloring", use_field_coloring_);
    shader_program_->getProgram()->setUniformValue("u_point_size", 3.0f); // 点云大小

    vao_.bind();

    // 根据渲染模式选择不同的绘制方式
    switch (mesh_render_mode_)
    {
        case MeshRenderMode::SOLID:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(gpu_data_->indices_.size()),
                           GL_UNSIGNED_INT, nullptr);
            break;

        case MeshRenderMode::POINT_CLOUD:
            glEnable(GL_PROGRAM_POINT_SIZE); // 启用着色器控制的点大小
            glDrawElements(GL_POINTS, static_cast<GLsizei>(gpu_data_->indices_.size()),
                           GL_UNSIGNED_INT, nullptr);
            glDisable(GL_PROGRAM_POINT_SIZE);
            break;

        case MeshRenderMode::WIREFRAME:
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(gpu_data_->indices_.size()),
                           GL_UNSIGNED_INT, nullptr);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // 恢复默认
            break;
    }

    vao_.release();
    shader_program_->release();
}
void Renderer::renderArrows(const Camera& camera)
{
    if (!arrow_shader_program_ || !arrow_shader_program_->getProgram())
    {
        return;
    }

    size_t num_vectors = gpu_data_->vector_field_positions_.size() / 3;
    if (num_vectors == 0)
    {
        return;
    }

    arrow_shader_program_->bind();

    QMatrix4x4 model = camera.getModelMatrix();
    QMatrix4x4 view = camera.getViewMatrix();
    QMatrix4x4 projection = camera.getProjectionMatrix();

    arrow_shader_program_->getProgram()->setUniformValue("u_model", model);
    arrow_shader_program_->getProgram()->setUniformValue("u_view", view);
    arrow_shader_program_->getProgram()->setUniformValue("u_projection", projection);

    arrow_shader_program_->getProgram()->setUniformValue("u_light_pos",
                                                         QVector3D(5.0f, 5.0f, 15.0f));
    arrow_shader_program_->getProgram()->setUniformValue("u_view_pos",
                                                         QVector3D(0.0f, 0.0f, 10.0f));

    arrow_shader_program_->getProgram()->setUniformValue("u_arrow_scale", 0.02f); // 缩小10倍
    arrow_shader_program_->getProgram()->setUniformValue("u_color_scheme",
                                                         static_cast<int>(color_scheme_));
    arrow_shader_program_->getProgram()->setUniformValue("u_vector_magnitude_min",
                                                         gpu_data_->vector_magnitude_min_);
    arrow_shader_program_->getProgram()->setUniformValue("u_vector_magnitude_max",
                                                         gpu_data_->vector_magnitude_max_);

    arrow_vao_.bind();
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(num_vectors));
    arrow_vao_.release();

    arrow_shader_program_->release();
}

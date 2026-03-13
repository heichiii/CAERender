#include "Renderer.h"
#include <QDebug>
#include <QDir>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
static const char* kPickPointVertexShader = R"(
#version 450 core
layout(location = 0) in vec3 a_pos;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform float u_point_size;

void main()
{
    gl_PointSize = u_point_size;
    gl_Position = u_projection * u_view * u_model * vec4(a_pos, 1.0);
}
)";

static const char* kPickPointFragmentShader = R"(
#version 450 core
out vec4 frag_color;

uniform vec3 u_color;

void main()
{
    frag_color = vec4(u_color, 1.0);
}
)";

struct CellKey
{
    int x;
    int y;
    int z;

    bool operator==(const CellKey& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct CellKeyHash
{
    size_t operator()(const CellKey& key) const
    {
        size_t seed = 0;
        seed ^= std::hash<int>{}(key.x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<int>{}(key.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<int>{}(key.z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

struct TriangleKeyHash
{
    size_t operator()(const std::array<uint32_t, 3>& triangle) const
    {
        size_t seed = 0;
        seed ^= std::hash<uint32_t>{}(triangle[0]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint32_t>{}(triangle[1]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint32_t>{}(triangle[2]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

static int clampCellIndex(float value, float minValue, float step, int divisions)
{
    if (step <= std::numeric_limits<float>::epsilon())
    {
        return 0;
    }

    const float normalized = (value - minValue) / step;
    const int idx = static_cast<int>(std::floor(normalized));
    return std::clamp(idx, 0, divisions - 1);
}

static std::vector<uint32_t> buildClusteredIndices(const std::vector<float>& vertices,
                                                   const std::vector<uint32_t>& indices,
                                                   int divisionsPerAxis)
{
    if (vertices.size() < 9 || indices.size() < 3 || divisionsPerAxis <= 1)
    {
        return indices;
    }

    const size_t vertexCount = vertices.size() / 3;
    std::vector<uint32_t> representative(vertexCount, 0);

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();

    for (size_t i = 0; i < vertexCount; ++i)
    {
        const float x = vertices[i * 3 + 0];
        const float y = vertices[i * 3 + 1];
        const float z = vertices[i * 3 + 2];

        minX = std::min(minX, x);
        minY = std::min(minY, y);
        minZ = std::min(minZ, z);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
        maxZ = std::max(maxZ, z);
    }

    const float stepX = (maxX - minX) / static_cast<float>(divisionsPerAxis);
    const float stepY = (maxY - minY) / static_cast<float>(divisionsPerAxis);
    const float stepZ = (maxZ - minZ) / static_cast<float>(divisionsPerAxis);

    std::unordered_map<CellKey, uint32_t, CellKeyHash> cellRepresentative;
    cellRepresentative.reserve(vertexCount / 2);

    for (size_t i = 0; i < vertexCount; ++i)
    {
        const float x = vertices[i * 3 + 0];
        const float y = vertices[i * 3 + 1];
        const float z = vertices[i * 3 + 2];

        const CellKey key{clampCellIndex(x, minX, stepX, divisionsPerAxis),
                          clampCellIndex(y, minY, stepY, divisionsPerAxis),
                          clampCellIndex(z, minZ, stepZ, divisionsPerAxis)};

        const auto it = cellRepresentative.find(key);
        if (it == cellRepresentative.end())
        {
            const auto rep = static_cast<uint32_t>(i);
            representative[i] = rep;
            cellRepresentative.emplace(key, rep);
        }
        else
        {
            representative[i] = it->second;
        }
    }

    std::vector<uint32_t> simplified;
    simplified.reserve(indices.size());
    std::unordered_set<std::array<uint32_t, 3>, TriangleKeyHash> uniqueTriangles;
    uniqueTriangles.reserve(indices.size() / 3);

    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        const uint32_t a = representative[indices[i + 0]];
        const uint32_t b = representative[indices[i + 1]];
        const uint32_t c = representative[indices[i + 2]];

        if (a == b || b == c || a == c)
        {
            continue;
        }

        std::array<uint32_t, 3> canonical{a, b, c};
        std::sort(canonical.begin(), canonical.end());
        if (!uniqueTriangles.insert(canonical).second)
        {
            continue;
        }

        simplified.push_back(a);
        simplified.push_back(b);
        simplified.push_back(c);
    }

    if (simplified.empty())
    {
        return indices;
    }

    return simplified;
}
} // namespace
Renderer::Renderer()
    : gpu_data_(nullptr), vbo_(QOpenGLBuffer::VertexBuffer), normal_(QOpenGLBuffer::VertexBuffer),
      ebo_(QOpenGLBuffer::IndexBuffer), arrow_pos_buffer_(QOpenGLBuffer::VertexBuffer),
      arrow_dir_buffer_(QOpenGLBuffer::VertexBuffer),
    arrow_mag_buffer_(QOpenGLBuffer::VertexBuffer), pick_point_vbo_(QOpenGLBuffer::VertexBuffer),
    mesh_render_mode_(MeshRenderMode::SOLID),
      color_scheme_(ColorScheme::RAINBOW), use_field_coloring_(false)
    //   vector_render_mode_(VectorRenderMode::ARROW), mode_(Mode::BASIC), lod_level_(LODLevel::HIGH)
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

    // 初始化流线着色器程序
    streamline_shader_program_ = std::make_unique<ShaderProgram>();
    if (!streamline_shader_program_->createFromFiles("../src/Shader/streamline.vert", "../src/Shader/streamline.frag"))
    {
        qWarning() << "Failed to create streamline shader program";
    }

    pick_point_shader_program_ = std::make_unique<ShaderProgram>();
    if (!pick_point_shader_program_->createFromSource(kPickPointVertexShader, kPickPointFragmentShader))
    {
        qWarning() << "Failed to create pick point shader program";
    }

    pick_point_vao_.create();
    pick_point_vao_.bind();
    pick_point_vbo_.create();
    pick_point_vbo_.bind();
    const float init_pos[3] = {0.0f, 0.0f, 0.0f};
    pick_point_vbo_.allocate(init_pos, static_cast<int>(sizeof(init_pos)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    pick_point_vao_.release();

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
        renderStreamlines(camera);
    }

    if (has_picked_point_)
    {
        renderPickedPoint(camera);
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

// void Renderer::setVectorRenderMode(VectorRenderMode mode)
// {
//     vector_render_mode_ = mode;
// }

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
    else if(mode_ == Mode::STREAMLINE)
    {
        updateStreamlineBuffers();
    }
    
}

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
    rebuildLODIndices();

    // ebo
    ebo_.create();
    ebo_.bind();
    applyLODToIndexBuffer();


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
void Renderer::updateStreamlineBuffers()
{
    if (!gpu_data_ || gpu_data_->streamline_vertices_.empty())
    {
        qWarning() << "No streamline data available";
        return;
    }

    size_t num_vertices = gpu_data_->streamline_vertices_.size() / 3;

    qDebug() << "Updating streamline buffers:" << num_vertices << "vertices";
    qDebug() << "Streamline magnitude range: [" << gpu_data_->streamline_magnitude_min_
             << ", " << gpu_data_->streamline_magnitude_max_ << "]";

    // 销毁旧缓冲
    if (streamline_vao_.isCreated())
    {
        streamline_vao_.destroy();
    }
    if (streamline_pos_buffer_.isCreated())
    {
        streamline_pos_buffer_.destroy();
    }
    if (streamline_mag_buffer_.isCreated())
    {
        streamline_mag_buffer_.destroy();
    }

    // 创建顶点数组对象
    streamline_vao_.create();
    streamline_vao_.bind();

    // 位置缓冲
    streamline_pos_buffer_.create();
    streamline_pos_buffer_.bind();
    streamline_pos_buffer_.allocate(gpu_data_->streamline_vertices_.data(),
                                    static_cast<int>(gpu_data_->streamline_vertices_.size() * sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    // 幅值缓冲
    streamline_mag_buffer_.create();
    streamline_mag_buffer_.bind();
    streamline_mag_buffer_.allocate(gpu_data_->streamline_magnitudes_.data(),
                                    static_cast<int>(gpu_data_->streamline_magnitudes_.size() * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 1 * sizeof(float), nullptr);

    streamline_vao_.release();

    qDebug() << "Streamline buffers updated successfully";
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

    const size_t lod_index_count = active_lod_index_count_;

    // 根据渲染模式选择不同的绘制方式
    switch (mesh_render_mode_)
    {
        case MeshRenderMode::SOLID:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(lod_index_count),
                           GL_UNSIGNED_INT, nullptr);
            break;

        case MeshRenderMode::POINT_CLOUD:
            glEnable(GL_PROGRAM_POINT_SIZE); // 启用着色器控制的点大小
            glDrawElements(GL_POINTS, static_cast<GLsizei>(lod_index_count),
                           GL_UNSIGNED_INT, nullptr);
            glDisable(GL_PROGRAM_POINT_SIZE);
            break;

        case MeshRenderMode::WIREFRAME:
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(lod_index_count),
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

void Renderer::renderStreamlines(const Camera& camera)
{
    if (!gpu_data_ || gpu_data_->streamline_vertices_.empty() || gpu_data_->streamline_line_counts_.empty())
    {
        qWarning() << "No streamline data to render";
        return;
    }

    // 确保着色器程序存在
    if (!streamline_shader_program_ || !streamline_shader_program_->getProgram())
    {
        qWarning() << "Streamline shader program not available";
        return;
    }

    streamline_shader_program_->bind();

    // 获取矩阵
    QMatrix4x4 model = camera.getModelMatrix();
    QMatrix4x4 view = camera.getViewMatrix();
    QMatrix4x4 projection = camera.getProjectionMatrix();

    // 设置统一变量
    streamline_shader_program_->getProgram()->setUniformValue("u_model", model);
    streamline_shader_program_->getProgram()->setUniformValue("u_view", view);
    streamline_shader_program_->getProgram()->setUniformValue("u_projection", projection);

    streamline_shader_program_->getProgram()->setUniformValue("u_light_pos", QVector3D(5.0f, 5.0f, 15.0f));
    streamline_shader_program_->getProgram()->setUniformValue("u_view_pos", QVector3D(0.0f, 0.0f, 10.0f));

    streamline_shader_program_->getProgram()->setUniformValue("u_color_scheme", static_cast<int>(color_scheme_));
    streamline_shader_program_->getProgram()->setUniformValue("u_magnitude_min", gpu_data_->streamline_magnitude_min_);
    streamline_shader_program_->getProgram()->setUniformValue("u_magnitude_max", gpu_data_->streamline_magnitude_max_);

    // 绑定VAO
    streamline_vao_.bind();

    // 绑定线条宽度
    glLineWidth(2.0f);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // 使用 glMultiDrawArrays 批量绘制所有流线（性能更优）
    size_t num_streamlines = gpu_data_->streamline_line_counts_.size();
    if (num_streamlines > 0)
    {
        // 转换数据类型以适应 glMultiDrawArrays 的参数要求
        std::vector<GLint> first_array;
        std::vector<GLsizei> count_array;

        first_array.reserve(num_streamlines);
        count_array.reserve(num_streamlines);

        for (size_t i = 0; i < num_streamlines; ++i)
        {
            first_array.push_back(static_cast<GLint>(gpu_data_->streamline_line_starts_[i]));
            count_array.push_back(static_cast<GLsizei>(gpu_data_->streamline_line_counts_[i]));
        }

        // 单次调用绘制所有流线（相比循环调用要快）
        glMultiDrawArrays(GL_LINE_STRIP, first_array.data(), count_array.data(), 
                         static_cast<GLsizei>(num_streamlines));
    }

    glDisable(GL_LINE_SMOOTH);
    glLineWidth(1.0f);

    streamline_vao_.release();
    streamline_shader_program_->release();
}

void Renderer::setLODLevel(LODLevel level)
{
    if (lod_level_ == level)
    {
        return;
    }

    lod_level_ = level;
    applyLODToIndexBuffer();
}

void Renderer::rebuildLODIndices()
{
    if (!gpu_data_ || gpu_data_->indices_.empty())
    {
        lod_high_indices_.clear();
        lod_medium_indices_.clear();
        lod_low_indices_.clear();
        active_lod_index_count_ = 0;
        ebo_capacity_ = 0;  // 重置缓冲区容量
        return;
    }

    lod_high_indices_ = gpu_data_->indices_;
    lod_medium_indices_ = buildClusteredIndices(gpu_data_->surface_vertices_, gpu_data_->indices_, 36);
    lod_low_indices_ = buildClusteredIndices(gpu_data_->surface_vertices_, gpu_data_->indices_, 20);

    if (lod_medium_indices_.size() > lod_high_indices_.size())
    {
        lod_medium_indices_ = lod_high_indices_;
    }
    if (lod_low_indices_.size() > lod_medium_indices_.size())
    {
        lod_low_indices_ = lod_medium_indices_;
    }
    
    ebo_capacity_ = 0;  // 重置缓冲区容量，强制重新分配
}

void Renderer::applyLODToIndexBuffer()
{
    if (!gpu_data_ || !vao_.isCreated() || !ebo_.isCreated())
    {
        return;
    }

    const std::vector<uint32_t>* activeIndices = &lod_high_indices_;
    switch (lod_level_)
    {
        case LODLevel::HIGH:
            activeIndices = &lod_high_indices_;
            break;
        case LODLevel::MEDIUM:
            activeIndices = &lod_medium_indices_;
            break;
        case LODLevel::LOW:
            activeIndices = &lod_low_indices_;
            break;
        default:
            activeIndices = &lod_high_indices_;
            break;
    }

    if (activeIndices->empty())
    {
        activeIndices = &lod_high_indices_;
    }

    active_lod_index_count_ = activeIndices->size();
    size_t required_size = activeIndices->size() * sizeof(uint32_t);

    vao_.bind();
    ebo_.bind();
    
    // 若需要更多容量，则重新分配；否则使用 glBufferSubData 更新数据
    if (required_size > ebo_capacity_)
    {
        // 预分配大于需要的空间（减少重新分配次数）
        ebo_capacity_ = static_cast<size_t>(required_size * 1.5f);
        // 先分配空的缓冲区
        ebo_.allocate(nullptr, static_cast<int>(ebo_capacity_));
        // 再写入实际数据
        ebo_.write(0, activeIndices->data(), static_cast<int>(required_size));
        qDebug() << QString("LOD: Reallocated EBO buffer to %1 bytes").arg(static_cast<int>(ebo_capacity_));
    }
    else
    {
        // 使用 glBufferSubData 更新，避免重新分配
        ebo_.write(0, activeIndices->data(), static_cast<int>(required_size));
    }
    
    vao_.release();
}

void Renderer::setPickedPoint(const QVector3D& point_obj)
{
    picked_point_obj_ = point_obj;
    has_picked_point_ = true;

    if (!pick_point_vbo_.isCreated())
        return;

    const float pos[3] = {point_obj.x(), point_obj.y(), point_obj.z()};
    pick_point_vao_.bind();
    pick_point_vbo_.bind();
    pick_point_vbo_.write(0, pos, static_cast<int>(sizeof(pos)));
    pick_point_vao_.release();
}

void Renderer::clearPickedPoint()
{
    has_picked_point_ = false;
}

void Renderer::renderPickedPoint(const Camera& camera)
{
    if (!pick_point_shader_program_ || !pick_point_shader_program_->getProgram() ||
        !pick_point_vao_.isCreated())
    {
        return;
    }

    pick_point_shader_program_->bind();
    pick_point_shader_program_->getProgram()->setUniformValue("u_model", camera.getModelMatrix());
    pick_point_shader_program_->getProgram()->setUniformValue("u_view", camera.getViewMatrix());
    pick_point_shader_program_->getProgram()->setUniformValue("u_projection", camera.getProjectionMatrix());
    pick_point_shader_program_->getProgram()->setUniformValue("u_point_size", 12.0f);
    pick_point_shader_program_->getProgram()->setUniformValue("u_color", QVector3D(1.0f, 0.1f, 0.1f));

    glEnable(GL_PROGRAM_POINT_SIZE);
    pick_point_vao_.bind();
    glDrawArrays(GL_POINTS, 0, 1);
    pick_point_vao_.release();
    glDisable(GL_PROGRAM_POINT_SIZE);

    pick_point_shader_program_->release();
}
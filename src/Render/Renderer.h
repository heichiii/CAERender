#pragma once

#include "Camera.h"
#include "Data/CaseData.h"
#include "Data/GPUData.h"
#include "ShaderProgram.h"
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLVertexArrayObject>
#include <cstdint>
#include <vector>

enum class Mode
{
    BASIC,
    ARROW,
    STREAMLINE
};
// 网格渲染模式枚举
enum class MeshRenderMode
{
    SOLID,       // 实体渲染
    POINT_CLOUD, // 点云渲染
    WIREFRAME    // 线框渲染
};

// 配色方案枚举
enum class ColorScheme
{
    RAINBOW,       // 彩虹（Jet）
    HEATMAP,       // 热力图
    COOL_WARM,     // 冷暖
    GRAYSCALE,     // 灰度
    BLUE_WHITE_RED // 蓝白红
};

// 矢量可视化渲染模式枚举
enum class VectorRenderMode
{
    ARROW,     // 箭头渲染
    STREAMLINE // 流线渲染（待实现）
};

// LOD级别枚举
enum class LODLevel
{
    HIGH,   // 高细节
    MEDIUM, // 中细节
    LOW     // 低细节
};

class Renderer : public QOpenGLFunctions_4_5_Core
{
public:
    Renderer();
    ~Renderer() override = default;

    void initialize();
    void render(const Camera& camera);
    void setMesh(const GPUData* p_gpu_data);

    // 渲染模式控制
    void setMeshRenderMode(MeshRenderMode mode);
    void setColorScheme(ColorScheme scheme);
    void setUseFieldColoring(bool use);
    void setVectorRenderMode(VectorRenderMode mode);
    void setMeshVisible(bool visible);
    void setStreamlineVisible(bool visible);
    
    void setMode(Mode mode);
    
    // LOD控制
    void setLODLevel(LODLevel level);
    LODLevel getLODLevel() const { return lod_level_; }

    void setPickedPoint(const QVector3D& point_obj);
    void clearPickedPoint();
    void setSeedSphere(const QVector3D& center_obj, float radius, bool visible);
    void clearSeedSphere();
    bool getColorbarRange(float& out_min, float& out_max, ColorScheme& out_scheme) const;

    Mode getMode() const
    {
        return mode_;
    }
    MeshRenderMode getMeshRenderMode() const
    {
        return mesh_render_mode_;
    }
    ColorScheme getColorScheme() const
    {
        return color_scheme_;
    }
    bool isUsingFieldColoring() const
    {
        return use_field_coloring_;
    }
    bool isMeshVisible() const
    {
        return show_mesh_;
    }
    bool isStreamlineVisible() const
    {
        return show_streamline_;
    }
    // VectorRenderMode getVectorRenderMode() const
    // {
    //     return vector_render_mode_;
    // }

private:
    // GPU数据指针
    const GPUData* gpu_data_;

    // Basic
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer vbo_;
    QOpenGLBuffer normal_;
    QOpenGLBuffer scalar_fields_;
    QOpenGLBuffer ebo_;
    std::unique_ptr<ShaderProgram> shader_program_;

    // arrow渲染相关
    QOpenGLVertexArrayObject arrow_vao_;
    QOpenGLBuffer arrow_pos_buffer_;
    QOpenGLBuffer arrow_dir_buffer_;
    QOpenGLBuffer arrow_mag_buffer_;
    std::unique_ptr<ShaderProgram> arrow_shader_program_;


    // streamline渲染相关
    QOpenGLVertexArrayObject streamline_vao_;
    QOpenGLBuffer streamline_pos_buffer_;
    QOpenGLBuffer streamline_mag_buffer_;
    QOpenGLBuffer streamline_ebo_;
    std::unique_ptr<ShaderProgram> streamline_shader_program_;
    std::vector<GLint> streamline_first_array_;
    std::vector<GLsizei> streamline_count_array_;
    size_t streamline_pos_capacity_bytes_ = 0;
    size_t streamline_mag_capacity_bytes_ = 0;

    // 拾取点高亮
    QOpenGLVertexArrayObject pick_point_vao_;
    QOpenGLBuffer pick_point_vbo_;
    std::unique_ptr<ShaderProgram> pick_point_shader_program_;
    bool has_picked_point_ = false;
    QVector3D picked_point_obj_ = QVector3D(0.0f, 0.0f, 0.0f);

    // 种子球体预览
    QOpenGLVertexArrayObject seed_sphere_vao_;
    QOpenGLBuffer seed_sphere_vbo_;
    bool show_seed_sphere_ = false;
    int seed_sphere_vertex_count_ = 0;


    QMatrix4x4 model_matrix_; // 模型矩阵，包含旋转和平移

    // 渲染参数
    Mode mode_;
    MeshRenderMode mesh_render_mode_;
    ColorScheme color_scheme_;
    bool use_field_coloring_;
    bool show_mesh_;
    bool show_streamline_;
    // VectorRenderMode vector_render_mode_;
    LODLevel lod_level_;
    
    // LOD数据
    std::vector<uint32_t> lod_high_indices_;
    std::vector<uint32_t> lod_medium_indices_;
    std::vector<uint32_t> lod_low_indices_;
    size_t active_lod_index_count_ = 0;
    size_t ebo_capacity_ = 0;  // EBO 预分配大小
    bool lod_buffer_dirty_ = false;  // 标记是否需要更新 EBO

    // 私有方法
    void rebuildLODIndices();
    void applyLODToIndexBuffer();
    void updateBasicBuffers();
    void updateArrowBuffers();
    void updateStreamlineBuffers();
    void renderBasic(const Camera& camera);
    void renderArrows(const Camera& camera);
    void renderStreamlines(const Camera& camera);
    void renderPickedPoint(const Camera& camera);
    void renderSeedSphere(const Camera& camera);

};

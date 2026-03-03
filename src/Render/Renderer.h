#pragma once

#include "Camera.h"
#include "Data/CaseData.h"
#include "Data/GPUData.h"
#include "ShaderProgram.h"
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLVertexArrayObject>
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
    HIGH,   // 高细节 - 距离 < 15
    MEDIUM, // 中细节 - 距离 15-50
    LOW     // 低细节 - 距离 > 50
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
    
    void setMode(Mode mode);
    
    // LOD控制
    void setLODLevel(LODLevel level);
    LODLevel getLODLevel() const { return lod_level_; }

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
    VectorRenderMode getVectorRenderMode() const
    {
        return vector_render_mode_;
    }

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


    QMatrix4x4 model_matrix_; // 模型矩阵，包含旋转和平移

    // 渲染参数
    Mode mode_;
    MeshRenderMode mesh_render_mode_;
    ColorScheme color_scheme_;
    bool use_field_coloring_;
    VectorRenderMode vector_render_mode_;
    LODLevel lod_level_;
    
    // LOD数据
    std::vector<uint32_t> lod_high_indices_;
    std::vector<uint32_t> lod_medium_indices_;
    std::vector<uint32_t> lod_low_indices_;
    size_t active_lod_index_count_ = 0;

    // 私有方法
    void rebuildLODIndices();
    void applyLODToIndexBuffer();
    void updateBasicBuffers();
    void updateArrowBuffers();
    void updateStreamlineBuffers();
    void renderBasic(const Camera& camera);
    void renderArrows(const Camera& camera);
    void renderStreamlines(const Camera& camera);

};

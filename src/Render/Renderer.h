#pragma once

#include "Data/CaseData.h"
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include "Data/GPUData.h"
#include "ShaderProgram.h"
#include "Camera.h"

// 网格渲染模式枚举
enum class MeshRenderMode
{
    SOLID,      // 实体渲染
    POINT_CLOUD, // 点云渲染
    WIREFRAME   // 线框渲染
};

// 配色方案枚举
enum class ColorScheme
{
    RAINBOW,    // 彩虹（Jet）
    HEATMAP,    // 热力图
    COOL_WARM,  // 冷暖
    GRAYSCALE,  // 灰度
    BLUE_WHITE_RED // 蓝白红
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
    
    MeshRenderMode getMeshRenderMode() const { return mesh_render_mode_; }
    ColorScheme getColorScheme() const { return color_scheme_; }
    bool isUsingFieldColoring() const { return use_field_coloring_; }

private:

    // GPUData gpu_data_;
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer vbo_;
    QOpenGLBuffer normal_;
    QOpenGLBuffer scalar_fields_;
    QOpenGLBuffer ebo_;

    const GPUData  * gpu_data_;
    std::unique_ptr<ShaderProgram> shader_program_;
    QMatrix4x4 model_matrix_;  // 模型矩阵，包含旋转和平移
    
    // 渲染参数
    MeshRenderMode mesh_render_mode_;
    ColorScheme color_scheme_;
    bool use_field_coloring_;
};

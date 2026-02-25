#pragma once

#include "Data/CaseData.h"
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include "Data/GPUData.h"
#include "ShaderProgram.h"
#include "Camera.h"

class Renderer : public QOpenGLFunctions_4_5_Core
{
public:
    Renderer();
    ~Renderer() override = default;

    void initialize();
    void render(const Camera& camera);
    void setMesh(const GPUData* p_gpu_data);

private:

    // GPUData gpu_data_;
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer vbo_;
    QOpenGLBuffer normal_;
    // QOpenGLBuffer scalar_fields_;
    QOpenGLBuffer ebo_;

    const GPUData  * gpu_data_;
    std::unique_ptr<ShaderProgram> shader_program_;
};

#pragma once

#include "Data/CaseData.h"
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include "Data/GPUData.h"

class Renderer : public QOpenGLFunctions_4_5_Core
{
public:
    Renderer();
    ~Renderer() override = default;

    void initialize();
    void render(const CaseData& caseData);

private:

    GPUData gpu_data_;
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer vbo_;
    QOpenGLBuffer ebo_;
    QOpenGLBuffer normal_;
    QOpenGLBuffer scalar_fields_;
};

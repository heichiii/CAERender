#pragma once

#include <QOpenGLWidget>
#include "Data/CaseData.h"
#include "Render/Renderer.h"
class GLWidget: public QOpenGLWidget
{
public:
    GLWidget(QWidget* parent = nullptr): QOpenGLWidget(parent) {};
    ~GLWidget() override= default;
    void loadFile(const std::string& filename);
    void setMesh(const GPUData* p_gpu_data);

private:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    CaseData case_data_;
    Renderer renderer_;
};
#pragma once

#include <QOpenGLWidget>
#include "Render/Renderer.h"
class GLWidget: public QOpenGLWidget
{
public:
    GLWidget(QWidget* parent = nullptr): QOpenGLWidget(parent) {};
    ~GLWidget() override= default;
    void loadFile(const std::string& filename);

private:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    CaseData case_data_;
    Renderer renderer_;
};
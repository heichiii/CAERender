#pragma once

#include <QMatrix4x4>
#include <QVector3D>


class Camera
{
//TODO:  摄像机类设计
public:
    Camera();
    ~Camera() = default;
    
    const QMatrix4x4 getViewMatrix() const;
    const QMatrix4x4 getProjectionMatrix() const;

    void rotate(float delta_phi, float delta_theta);
    void zoom(float delta);
    void pan(float delta_x, float delta_y);
    void reset();

private:
    void updatePosition();

    QVector3D position_;
    QVector3D target_;
    QVector3D up_;
    float fov_;
    float aspect_ratio_;
    float near_plane_;
    float far_plane_;
    float distance_to_target_;
    float phi_;
    float theta_;
};



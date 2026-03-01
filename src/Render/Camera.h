#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QQuaternion>


class Camera
{
public:
    Camera();
    ~Camera() = default;
    
    const QMatrix4x4 getViewMatrix() const;
    const QMatrix4x4 getProjectionMatrix() const;

    // 旋转和缩放相关
    void rotate(float delta_x, float delta_y);
    void applyRotationDelta(float delta_x, float delta_y);
    void updateRotationCenter(const QVector3D& new_center);
    void zoom(float delta);
    void pan(float delta_x, float delta_y);
    void reset();
    QVector3D getRotationCenter() const { return pivot_point_; }
    QQuaternion getRotationQuaternion() const { return object_rotation_; }
    QMatrix4x4 getModelMatrix() const { return model_matrix_; }

    // 窗口大小改变时更新投影矩阵
    void setAspectRatio(float aspect_ratio) { aspect_ratio_ = aspect_ratio; }

private:
    void updatePosition();
    void updateModelMatrix();

    // 摄像机参数
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

    // 四元数和旋转中心
    QVector3D pivot_point_;           // 旋转中心
    QQuaternion object_rotation_;      // 物体旋转四元数
    QMatrix4x4 model_matrix_;          // 模型矩阵
    float rotation_sensitivity_;       // 旋转灵敏度
};



#include "Camera.h"
#include <cmath>

Camera::Camera()
    : position_(0.0f, 0.0f, 5.0f),
      target_(0.0f, 0.0f, 0.0f),
      up_(0.0f, 1.0f, 0.0f),
      fov_(45.0f),
      aspect_ratio_(4.0f / 3.0f),
      near_plane_(0.1f),
      far_plane_(100.0f),
      distance_to_target_(5.0f),
      phi_(90.0f),
      theta_(90.0f),
      pivot_point_(0.0f, 0.0f, 0.0f),
      object_rotation_(1.0f, 0.0f, 0.0f, 0.0f),  // 单位四元数
      rotation_sensitivity_(0.15f)
{
    updatePosition();
    updateModelMatrix();
}

const QMatrix4x4 Camera::getViewMatrix() const
{
    QMatrix4x4 view;
    view.lookAt(position_, target_, up_);
    return view;
}

const QMatrix4x4 Camera::getProjectionMatrix() const
{
    QMatrix4x4 projection;
    projection.perspective(fov_, aspect_ratio_, near_plane_, far_plane_);
    return projection;
}

void Camera::updateRotationCenter(const QVector3D& new_center)
{
    pivot_point_ = new_center;
    updateModelMatrix();
}

void Camera::rotate(float delta_x, float delta_y)
{
    // 使用球面坐标旋转摄像机视角
    phi_ += delta_x * 0.5f;
    theta_ -= delta_y * 0.5f;
    
    // 限制theta角度在0-180度之间
    if (theta_ < 5.0f) theta_ = 5.0f;
    if (theta_ > 175.0f) theta_ = 175.0f;
    
    updatePosition();
}

void Camera::applyRotationDelta(float delta_x, float delta_y)
{
    // 使用四元数增量旋转物体
    // 基于鼠标移动创建旋转四元数
    
    float angle = std::sqrt(delta_x * delta_x + delta_y * delta_y) * rotation_sensitivity_;
    
    if (angle > 0.0001f)
    {
        // 计算旋转轴（垂直于鼠标移动方向）
        QVector3D rotation_axis(delta_y, delta_x, 0.0f);
        rotation_axis.normalize();
        
        // 创建增量旋转四元数
        QQuaternion delta_rotation = QQuaternion::fromAxisAndAngle(rotation_axis, angle);
        
        // 与当前旋转合并（相乘）
        object_rotation_ = delta_rotation * object_rotation_;
        object_rotation_.normalize();
        
        updateModelMatrix();
    }
}

void Camera::updateModelMatrix()
{
    model_matrix_.setToIdentity();
    
    // 平移到旋转中心原点
    model_matrix_.translate(-pivot_point_);
    
    // 应用四元数旋转（使用QMatrix4x4的rotate方法）
    // 将四元数转换为轴角表示
    QVector3D axis = object_rotation_.vector();
    if (axis.length() > 0.0001f)
    {
        axis.normalize();
        float angle = 2.0f * std::acos(qBound(-1.0f, object_rotation_.scalar(), 1.0f));
        angle = qRadiansToDegrees(angle);
        model_matrix_.rotate(angle, axis);
    }
    
    // 平移回去
    model_matrix_.translate(pivot_point_);
}

void Camera::zoom(float delta)
{
    distance_to_target_ -= delta;
    
    // 限制缩放距离
    if (distance_to_target_ < 0.1f) distance_to_target_ = 0.1f;
    if (distance_to_target_ > 1000.0f) distance_to_target_ = 1000.0f;
    
    updatePosition();
}

void Camera::pan(float delta_x, float delta_y)
{
    QVector3D right = QVector3D::crossProduct(position_ - target_, up_).normalized();
    QVector3D up = QVector3D::crossProduct(right, position_ - target_).normalized();
    position_ += right * delta_x + up * delta_y;
    target_ += right * delta_x + up * delta_y;
}

void Camera::reset()
{
    position_ = QVector3D(0.0f, 0.0f, 10.0f);
    target_ = QVector3D(0.0f, 0.0f, 0.0f);
    up_ = QVector3D(0.0f, 1.0f, 0.0f);
    fov_ = 45.0f;
    aspect_ratio_ = 4.0f / 3.0f;
    near_plane_ = 0.1f;
    far_plane_ = 100.0f;
    distance_to_target_ = 5.0f;
    phi_ = 90.0f;
    theta_ = 90.0f;
    
    pivot_point_ = QVector3D(0.0f, 0.0f, 0.0f);
    object_rotation_ = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
    
    updatePosition();
    updateModelMatrix();
}

void Camera::updatePosition()
{
    float rad_phi = qDegreesToRadians(phi_);
    float rad_theta = qDegreesToRadians(theta_);

    position_.setX(target_.x() + distance_to_target_ * sin(rad_theta) * cos(rad_phi));
    position_.setY(target_.y() + distance_to_target_ * cos(rad_theta));
    position_.setZ(target_.z() + distance_to_target_ * sin(rad_theta) * sin(rad_phi));
}




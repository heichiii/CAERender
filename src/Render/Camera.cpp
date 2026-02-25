#include "Camera.h"

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
      theta_(90.0f)
{
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

void Camera::rotate(float delta_phi, float delta_theta)
{
    phi_ += delta_phi;
    theta_ += delta_theta;
    updatePosition();
}

void Camera::zoom(float delta)
{
    distance_to_target_ += delta;
    updatePosition();
}

void Camera::pan(float delta_x, float delta_y)
{
    QVector3D right = QVector3D::crossProduct(target_ - position_, up_).normalized();
    QVector3D up = QVector3D::crossProduct(right, target_ - position_).normalized();
    position_ += right * delta_x + up * delta_y;
    target_ += right * delta_x + up * delta_y;
}

void Camera::reset()
{
    position_ = QVector3D(0.0f, 0.0f, 5.0f);
    target_ = QVector3D(0.0f, 0.0f, 0.0f);
    up_ = QVector3D(0.0f, 1.0f, 0.0f);
    fov_ = 45.0f;
    aspect_ratio_ = 4.0f / 3.0f;
    near_plane_ = 0.1f;
    far_plane_ = 100.0f;
    distance_to_target_ = 5.0f;
    phi_ = 90.0f;
    theta_ = 90.0f;
}

void Camera::updatePosition()
{
    float rad_phi = qDegreesToRadians(phi_);
    float rad_theta = qDegreesToRadians(theta_);

    position_.setX(target_.x() + distance_to_target_ * sin(rad_theta) * cos(rad_phi));
    position_.setY(target_.y() + distance_to_target_ * cos(rad_theta));
    position_.setZ(target_.z() + distance_to_target_ * sin(rad_theta) * sin(rad_phi));
}




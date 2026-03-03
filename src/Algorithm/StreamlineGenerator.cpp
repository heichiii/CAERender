#include "StreamlineGenerator.h"
#include <cmath>
#include <algorithm>
#include <omp.h>
#include <limits>
#include <QDebug>

namespace Streamline {

StreamlineGenerator::StreamlineGenerator() = default;

StreamlineGenerator::~StreamlineGenerator() = default;

std::vector<Streamline> StreamlineGenerator::generate(
    const std::vector<QVector3D>& seed_positions,
    const Field* vector_field,
    const std::vector<float>& mesh_vertices,
    const StreamlineParams& params)
{
    if (!vector_field || seed_positions.empty() || mesh_vertices.empty()) {
        qWarning() << "StreamlineGenerator: Invalid input data";
        return {};
    }

    std::vector<Streamline> streamlines;
    streamlines.resize(seed_positions.size());

    // 初始化网格边界缓存
    if (!bounds_initialized_) {
        mesh_min_ = QVector3D(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        );
        mesh_max_ = QVector3D(
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        );

        for (size_t i = 0; i < mesh_vertices.size(); i += 3) {
            mesh_min_.setX(std::min(mesh_min_.x(), mesh_vertices[i]));
            mesh_min_.setY(std::min(mesh_min_.y(), mesh_vertices[i + 1]));
            mesh_min_.setZ(std::min(mesh_min_.z(), mesh_vertices[i + 2]));

            mesh_max_.setX(std::max(mesh_max_.x(), mesh_vertices[i]));
            mesh_max_.setY(std::max(mesh_max_.y(), mesh_vertices[i + 1]));
            mesh_max_.setZ(std::max(mesh_max_.z(), mesh_vertices[i + 2]));
        }
        bounds_initialized_ = true;

        qDebug() << "Mesh bounds: min" << mesh_min_ << "max" << mesh_max_;
    }

    // 并行生成流线
    #pragma omp parallel for num_threads(params.num_threads) schedule(dynamic)
    for (int i = 0; i < static_cast<int>(seed_positions.size()); ++i) {
        streamlines[i] = generateSingleStreamline(
            seed_positions[i],
            vector_field,
            mesh_vertices,
            params
        );
    }

    qDebug() << "Generated" << streamlines.size() << "streamlines";

    return streamlines;
}

Streamline StreamlineGenerator::generateSingleStreamline(
    const QVector3D& seed,
    const Field* vector_field,
    const std::vector<float>& mesh_vertices,
    const StreamlineParams& params)
{
    Streamline streamline;

    // 检查种子点有效性
    if (!isWithinBounds(seed, mesh_vertices)) {
        qWarning() << "Seed point out of bounds:" << seed;
        streamline.valid = false;
        return streamline;
    }

    QVector3D current_pos = seed;
    streamline.points.reserve(params.max_iterations);

    // 记录初始点
    StreamlinePoint start_point{current_pos.x(), current_pos.y(), current_pos.z(), 0.0f};
    streamline.points.push_back(start_point);

    // 迭代生成流线点
    for (int iter = 0; iter < params.max_iterations; ++iter) {
        // 获取当前位置的矢量
        QVector3D velocity = interpolateVector(current_pos, vector_field, mesh_vertices);
        float velocity_magnitude = velocity.length();

        // 停止条件1：速度过小（到达静止区域）
        if (velocity_magnitude < params.min_velocity) {
            qDebug() << "Streamline stopped: velocity too small" << velocity_magnitude;
            break;
        }

        // 停止条件2：超出边界
        if (!isWithinBounds(current_pos, mesh_vertices)) {
            qDebug() << "Streamline stopped: out of bounds";
            break;
        }

        // 停止条件3：长度超限
        if (streamline.total_length > params.max_length) {
            qDebug() << "Streamline stopped: max length exceeded";
            break;
        }

        // RK4积分求下一步位置
        QVector3D next_pos = rk4Step(current_pos, params.dt, vector_field, mesh_vertices);

        // 计算步长距离
        float step_distance = (next_pos - current_pos).length();
        streamline.total_length += step_distance;

        // 保存新点
        StreamlinePoint new_point{next_pos.x(), next_pos.y(), next_pos.z(), velocity_magnitude};
        streamline.points.push_back(new_point);

        current_pos = next_pos;
    }

    streamline.valid = streamline.points.size() > 1;
    qDebug() << "Generated streamline with" << streamline.points.size() << "points, length:" << streamline.total_length;

    return streamline;
}

// ==================== RK4核心算法实现 ====================

QVector3D StreamlineGenerator::rk4Step(
    const QVector3D& current_pos,
    float dt,
    const Field* vector_field,
    const std::vector<float>& mesh_vertices)
{
    // k1 = f(t, y)
    QVector3D k1 = interpolateVector(current_pos, vector_field, mesh_vertices);

    // k2 = f(t + dt/2, y + dt*k1/2)
    QVector3D k2 = interpolateVector(
        current_pos + 0.5f * dt * k1,
        vector_field,
        mesh_vertices
    );

    // k3 = f(t + dt/2, y + dt*k2/2)
    QVector3D k3 = interpolateVector(
        current_pos + 0.5f * dt * k2,
        vector_field,
        mesh_vertices
    );

    // k4 = f(t + dt, y + dt*k3)
    QVector3D k4 = interpolateVector(
        current_pos + dt * k3,
        vector_field,
        mesh_vertices
    );

    // y(t+dt) = y(t) + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
    QVector3D next_pos = current_pos + (dt / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);

    return next_pos;
}

// ==================== 矢量插值 ====================

QVector3D StreamlineGenerator::interpolateVector(
    const QVector3D& pos,
    const Field* vector_field,
    const std::vector<float>& mesh_vertices)
{
    if (!vector_field || vector_field->data.empty()) {
        return QVector3D(0, 0, 0);
    }

    // 简化版本：直接从最近的数据点获取（可改进为真正的插值）
    // TODO: 实现完整的三线性插值
    
    float min_dist = std::numeric_limits<float>::max();
    int nearest_idx = -1;

    // 查找网格中最近的顶点
    int num_vertices = mesh_vertices.size() / 3;
    for (int i = 0; i < num_vertices; ++i) {
        QVector3D vert(
            mesh_vertices[i * 3],
            mesh_vertices[i * 3 + 1],
            mesh_vertices[i * 3 + 2]
        );
        float dist = (pos - vert).length();
        if (dist < min_dist) {
            min_dist = dist;
            nearest_idx = i;
        }
    }

    if (nearest_idx >= 0 && nearest_idx * vector_field->num_components_ + 2 < static_cast<int>(vector_field->data.size())) {
        return QVector3D(
            vector_field->data[nearest_idx * vector_field->num_components_],
            vector_field->data[nearest_idx * vector_field->num_components_ + 1],
            vector_field->data[nearest_idx * vector_field->num_components_ + 2]
        );
    }

    return QVector3D(0, 0, 0);
}

// ==================== 边界检测 ====================

bool StreamlineGenerator::isWithinBounds(
    const QVector3D& pos,
    const std::vector<float>& mesh_vertices)
{
    return pos.x() >= mesh_min_.x() && pos.x() <= mesh_max_.x() &&
           pos.y() >= mesh_min_.y() && pos.y() <= mesh_max_.y() &&
           pos.z() >= mesh_min_.z() && pos.z() <= mesh_max_.z();
}

int StreamlineGenerator::findContainingCell(
    const QVector3D& pos,
    const std::vector<float>& mesh_vertices)
{
    // TODO: 实现基于八叉树或网格加速结构的查找
    // 当前简化实现：线性搜索（性能较差）
    for (size_t i = 0; i < mesh_vertices.size() / 3; ++i) {
        QVector3D v(
            mesh_vertices[i * 3],
            mesh_vertices[i * 3 + 1],
            mesh_vertices[i * 3 + 2]
        );
        // 检查是否在该顶点附近
        if ((pos - v).length() < 0.1f) {
            return i;
        }
    }
    return -1;
}

QVector3D StreamlineGenerator::getLocalCoordinates(
    const QVector3D& point,
    const std::vector<QVector3D>& cell_vertices)
{
    // TODO: 实现牛顿法求解局部坐标
    // 当前简化：返回单位立方体中的相对位置
    if (cell_vertices.size() < 8) {
        return QVector3D(-1, -1, -1);
    }

    QVector3D min_v = cell_vertices[0];
    QVector3D max_v = cell_vertices[0];

    for (const auto& v : cell_vertices) {
        if (v.x() < min_v.x()) min_v.setX(v.x());
        if (v.y() < min_v.y()) min_v.setY(v.y());
        if (v.z() < min_v.z()) min_v.setZ(v.z());

        if (v.x() > max_v.x()) max_v.setX(v.x());
        if (v.y() > max_v.y()) max_v.setY(v.y());
        if (v.z() > max_v.z()) max_v.setZ(v.z());
    }

    QVector3D range = max_v - min_v;
    if (range.x() < 1e-6f || range.y() < 1e-6f || range.z() < 1e-6f) {
        return QVector3D(-1, -1, -1);
    }

    QVector3D local = (point - min_v) / range;
    
    // 检查是否在单元内
    if (local.x() < 0 || local.x() > 1 ||
        local.y() < 0 || local.y() > 1 ||
        local.z() < 0 || local.z() > 1) {
        return QVector3D(-1, -1, -1);
    }

    return local;
}

QVector3D StreamlineGenerator::trilinearInterpolate(
    const QVector3D& local_coords,
    const std::vector<QVector3D>& values)
{
    if (values.size() < 8) {
        return QVector3D(0, 0, 0);
    }

    float x = local_coords.x();
    float y = local_coords.y();
    float z = local_coords.z();

    // 8个顶点的权重（三线性基函数）
    float w[8] = {
        (1 - x) * (1 - y) * (1 - z),  // v000
        x * (1 - y) * (1 - z),        // v100
        (1 - x) * y * (1 - z),        // v010
        x * y * (1 - z),              // v110
        (1 - x) * (1 - y) * z,        // v001
        x * (1 - y) * z,              // v101
        (1 - x) * y * z,              // v011
        x * y * z                     // v111
    };

    QVector3D result(0, 0, 0);
    for (int i = 0; i < 8; ++i) {
        result += w[i] * values[i];
    }

    return result;
}

} // namespace Streamline

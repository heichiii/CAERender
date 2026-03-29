#include "StreamlineGenerator.h"
#include "TestTool/Profiler.h"
#include <QDebug>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <omp.h>
namespace Streamline
{

    namespace
    {
        inline QVector3D vertexAt(const std::vector<float>& mesh_vertices, uint32_t vid)
        {
            const size_t base = static_cast<size_t>(vid) * 3;
            return QVector3D(mesh_vertices[base], mesh_vertices[base + 1], mesh_vertices[base + 2]);
        }

        bool barycentricInTetra(const QVector3D& p,
                                const QVector3D& a,
                                const QVector3D& b,
                                const QVector3D& c,
                                const QVector3D& d,
                                std::array<float, 4>& w)
        {
            const QVector3D v0 = b - a;
            const QVector3D v1 = c - a;
            const QVector3D v2 = d - a;
            const QVector3D vp = p - a;

            const float det = QVector3D::dotProduct(v0, QVector3D::crossProduct(v1, v2));
            if (std::abs(det) < 1e-12f)
            {
                return false;
            }

            const float inv_det = 1.0f / det;
            const float w1 =
                QVector3D::dotProduct(vp, QVector3D::crossProduct(v1, v2)) * inv_det;
            const float w2 =
                QVector3D::dotProduct(v0, QVector3D::crossProduct(vp, v2)) * inv_det;
            const float w3 =
                QVector3D::dotProduct(v0, QVector3D::crossProduct(v1, vp)) * inv_det;
            const float w0 = 1.0f - w1 - w2 - w3;

            w = {w0, w1, w2, w3};

            constexpr float eps = -1e-4f;
            return w0 >= eps && w1 >= eps && w2 >= eps && w3 >= eps;
        }

        inline QVector3D samplePointVector(const Field* vector_field, uint32_t point_id)
        {
            const size_t base = static_cast<size_t>(point_id) *
                                static_cast<size_t>(vector_field->num_components_);
            if (base + 2 >= vector_field->data.size())
            {
                return QVector3D(0.0f, 0.0f, 0.0f);
            }
            return QVector3D(vector_field->data[base], vector_field->data[base + 1],
                             vector_field->data[base + 2]);
        }

        inline QVector3D sampleCellVector(const Field* vector_field, size_t cell_id)
        {
            const size_t base = cell_id * static_cast<size_t>(vector_field->num_components_);
            if (base + 2 >= vector_field->data.size())
            {
                return QVector3D(0.0f, 0.0f, 0.0f);
            }
            return QVector3D(vector_field->data[base], vector_field->data[base + 1],
                             vector_field->data[base + 2]);
        }
    } // namespace

    StreamlineGenerator::StreamlineGenerator() = default;

    StreamlineGenerator::~StreamlineGenerator() = default;

    std::vector<Streamline> StreamlineGenerator::generate(
        const std::vector<QVector3D>& seed_positions, const Field* vector_field,
        const std::vector<float>& mesh_vertices, const StreamlineParams& params,
        const MeshPart* mesh_part)
    {
        PROFILE_CODE
        if (!vector_field || seed_positions.empty() || mesh_vertices.empty())
        {
            qWarning() << "StreamlineGenerator: Invalid input data";
            return {};
        }

        std::vector<Streamline> streamlines;
        streamlines.resize(seed_positions.size());

        mesh_part_ = mesh_part;

        const bool mesh_changed = (cached_mesh_ptr_ != mesh_vertices.data()) ||
                      (cached_mesh_size_ != mesh_vertices.size()) ||
                      (mesh_part_ == nullptr && !supported_cell_ids_.empty());

        // 初始化/更新网格边界缓存
        if (!bounds_initialized_ || mesh_changed)
        {
            //TODO: 这里可以考虑并行化计算边界
            mesh_min_ =
                QVector3D(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                          std::numeric_limits<float>::max());
            mesh_max_ = QVector3D(std::numeric_limits<float>::lowest(),
                                  std::numeric_limits<float>::lowest(),
                                  std::numeric_limits<float>::lowest());

            for (size_t i = 0; i < mesh_vertices.size(); i += 3)
            {
                mesh_min_.setX(std::min(mesh_min_.x(), mesh_vertices[i]));
                mesh_min_.setY(std::min(mesh_min_.y(), mesh_vertices[i + 1]));
                mesh_min_.setZ(std::min(mesh_min_.z(), mesh_vertices[i + 2]));

                mesh_max_.setX(std::max(mesh_max_.x(), mesh_vertices[i]));
                mesh_max_.setY(std::max(mesh_max_.y(), mesh_vertices[i + 1]));
                mesh_max_.setZ(std::max(mesh_max_.z(), mesh_vertices[i + 2]));
            }
            bounds_initialized_ = true;

            qDebug() << "Mesh bounds: min" << mesh_min_ << "max" << mesh_max_;
            octree_.build(mesh_vertices);
            qDebug() << "Octree built with" << octree_.totalPoints() << "points";

            const int num_vertices = static_cast<int>(mesh_vertices.size() / 3);
            float diag = (mesh_max_ - mesh_min_).length();
            if (!std::isfinite(diag) || diag <= 0.0f)
            {
                diag = 1.0f;
            }

            float avg_nn = 0.0f;
            int nn_count = 0;
            if (num_vertices >= 2)
            {
                const int sample_count = std::min(1024, num_vertices);
                const int stride = std::max(1, num_vertices / sample_count);

                for (int i = 0; i < num_vertices; i += stride)
                {
                    const float p[3] = {mesh_vertices[i * 3], mesh_vertices[i * 3 + 1],
                                        mesh_vertices[i * 3 + 2]};
                    auto nearest = octree_.findNearestK(p, 2);
                    if (nearest.size() >= 2)
                    {
                        const float sq_d = nearest[1].first;
                        if (sq_d > 1e-14f)
                        {
                            avg_nn += std::sqrt(sq_d);
                            ++nn_count;
                        }
                    }
                }
            }

            if (nn_count > 0)
            {
                avg_nn /= static_cast<float>(nn_count);
            }
            else
            {
                avg_nn = diag * 0.01f;
            }

            const float support_radius = std::max(avg_nn * 3.0f, diag * 0.005f);
            support_radius_sq_ = support_radius * support_radius;
            cached_mesh_ptr_ = mesh_vertices.data();
            cached_mesh_size_ = mesh_vertices.size();
            qDebug() << "Streamline support radius:" << support_radius;

            supported_cell_ids_.clear();
            vertex_to_supported_cells_.clear();
            supported_cell_min_.clear();
            supported_cell_max_.clear();
            if (mesh_part_)
            {
                const size_t point_count = mesh_vertices.size() / 3;
                vertex_to_supported_cells_.resize(point_count);
                supported_cell_min_.resize(mesh_part_->cells_.size());
                supported_cell_max_.resize(mesh_part_->cells_.size());

                supported_cell_ids_.reserve(mesh_part_->cells_.size());
                for (size_t cid = 0; cid < mesh_part_->cells_.size(); ++cid)
                {
                    const CellData& cell = mesh_part_->cells_[cid];
                    if (cell.kind == CellKind::TETRA || cell.kind == CellKind::HEXAHEDRON)
                    {
                        bool valid_points = true;
                        QVector3D bmin(std::numeric_limits<float>::max(),
                                       std::numeric_limits<float>::max(),
                                       std::numeric_limits<float>::max());
                        QVector3D bmax(std::numeric_limits<float>::lowest(),
                                       std::numeric_limits<float>::lowest(),
                                       std::numeric_limits<float>::lowest());

                        for (uint8_t i = 0; i < cell.num_points; ++i)
                        {
                            const uint32_t pid = cell.point_ids[i];
                            if (pid >= point_count)
                            {
                                valid_points = false;
                                break;
                            }

                            const size_t base = static_cast<size_t>(pid) * 3;
                            const float x = mesh_vertices[base + 0];
                            const float y = mesh_vertices[base + 1];
                            const float z = mesh_vertices[base + 2];

                            bmin.setX(std::min(bmin.x(), x));
                            bmin.setY(std::min(bmin.y(), y));
                            bmin.setZ(std::min(bmin.z(), z));

                            bmax.setX(std::max(bmax.x(), x));
                            bmax.setY(std::max(bmax.y(), y));
                            bmax.setZ(std::max(bmax.z(), z));

                            vertex_to_supported_cells_[pid].push_back(cid);
                        }

                        if (!valid_points)
                        {
                            continue;
                        }

                        supported_cell_min_[cid] = bmin;
                        supported_cell_max_[cid] = bmax;
                        supported_cell_ids_.push_back(cid);
                    }
                }
                qDebug() << "Cell-aware interpolation enabled with" << supported_cell_ids_.size()
                         << "supported cells";
            }
        }

// 并行生成流线
#pragma omp parallel for num_threads(params.num_threads) schedule(dynamic)
        for (int i = 0; i < static_cast<int>(seed_positions.size()); ++i)
        {
            streamlines[i] =
                generateSingleStreamline(seed_positions[i], vector_field, mesh_vertices, params);
        }

        qDebug() << "Generated" << streamlines.size() << "streamlines";

        // 后处理平滑
        if (params.enable_smoothing && params.smooth_iterations > 0)
        {
            smoothStreamlines(streamlines, params.smooth_iterations);
            qDebug() << "Streamlines smoothed with" << params.smooth_iterations << "iterations";
        }

        return streamlines;
    }

    Streamline
    StreamlineGenerator::generateSingleStreamline(const QVector3D& seed, const Field* vector_field,
                                                  const std::vector<float>& mesh_vertices,
                                                  const StreamlineParams& params)
    {
        Streamline streamline;

        // 检查种子点有效性
        if (!isWithinBounds(seed))
        {
            qWarning() << "Seed point out of bounds:" << seed;
            streamline.valid = false;
            return streamline;
        }

        const float abs_dt = std::abs(params.dt);
        if (abs_dt <= std::numeric_limits<float>::epsilon())
        {
            streamline.valid = false;
            return streamline;
        }

        std::vector<StreamlinePoint> backward_points;
        std::vector<StreamlinePoint> forward_points;
        backward_points.reserve(params.max_iterations);
        forward_points.reserve(params.max_iterations);

        auto integrateDirection = [&](float dt_sign, std::vector<StreamlinePoint>& out_points,
                                      float& out_length)
        {
            QVector3D current_pos = seed;
            out_length = 0.0f;
            float elapsed_time = 0.0f;
            float step_size = abs_dt;

            // 基于基准步长的简易自适应控制参数
            const float min_step = std::max(abs_dt * 0.125f, 1e-5f);
            const float max_step = std::max(abs_dt * 4.0f, min_step);
            const float error_tolerance = std::max(abs_dt * 0.05f, 1e-5f);
            constexpr int max_retry = 8;

            for (int iter = 0; iter < params.max_iterations; ++iter)
            {
                if (!isWithinBounds(current_pos))
                {
                    break;
                }

                QVector3D velocity = interpolateVector(current_pos, vector_field, mesh_vertices);
                const float velocity_magnitude = velocity.length();

                // 停止条件1：速度过小（到达静止区域）
                if (velocity_magnitude < params.min_velocity)
                {
                    break;
                }

                // 停止条件2：长度超限
                if (out_length > params.max_length)
                {
                    break;
                }

                // 停止条件3：传播时间超限（仅当启用时）
                if (params.max_propagation_time > 0.0f && elapsed_time > params.max_propagation_time)
                {
                    break;
                }

                bool accepted = false;
                QVector3D next_pos;
                float used_step = step_size;

                // 使用 step-doubling 估计局部误差，自动缩放步长
                for (int retry = 0; retry < max_retry; ++retry)
                {
                    const float signed_step = dt_sign * used_step;
                    const QVector3D full_step =
                        rk4Step(current_pos, signed_step, vector_field, mesh_vertices, params);
                    const QVector3D half_step =
                        rk4Step(current_pos, signed_step * 0.5f, vector_field, mesh_vertices,
                                params);

                    if (!isWithinBounds(full_step) || !isWithinBounds(half_step))
                    {
                        used_step *= 0.5f;
                        if (used_step < min_step)
                        {
                            break;
                        }
                        continue;
                    }

                    const QVector3D two_half_step =
                        rk4Step(half_step, signed_step * 0.5f, vector_field, mesh_vertices,
                                params);
                    if (!isWithinBounds(two_half_step))
                    {
                        used_step *= 0.5f;
                        if (used_step < min_step)
                        {
                            break;
                        }
                        continue;
                    }

                    const float local_error = (two_half_step - full_step).length();
                    if (local_error > error_tolerance && used_step > min_step)
                    {
                        used_step = std::max(used_step * 0.5f, min_step);
                        continue;
                    }

                    next_pos = two_half_step;
                    accepted = true;

                    if (local_error < error_tolerance * 0.25f)
                    {
                        step_size = std::min(used_step * 1.5f, max_step);
                    }
                    else
                    {
                        step_size = used_step;
                    }
                    break;
                }

                if (!accepted)
                {
                    break;
                }

                if (!isWithinBounds(next_pos))
                {
                    break;
                }

                // 计算步长距离
                const float step_distance = (next_pos - current_pos).length();
                if (step_distance < 1e-8f)
                {
                    break;
                }
                if (out_length + step_distance > params.max_length)
                {
                    break;
                }
                out_length += step_distance;
                elapsed_time += used_step;

                // 保存新点
                const float next_magnitude =
                    interpolateVector(next_pos, vector_field, mesh_vertices).length();
                StreamlinePoint new_point{next_pos.x(), next_pos.y(), next_pos.z(),
                                          next_magnitude};
                out_points.push_back(new_point);

                current_pos = next_pos;
            }
        };

        float backward_length = 0.0f;
        float forward_length = 0.0f;
        integrateDirection(-abs_dt, backward_points, backward_length); // 负时间积分
        integrateDirection(abs_dt, forward_points, forward_length);    // 正时间积分

        streamline.points.reserve(backward_points.size() + 1 + forward_points.size());

        // 合并顺序：backward(反转) -> seed -> forward
        for (auto it = backward_points.rbegin(); it != backward_points.rend(); ++it)
        {
            streamline.points.push_back(*it);
        }

        QVector3D seed_velocity = interpolateVector(seed, vector_field, mesh_vertices);
        StreamlinePoint seed_point{seed.x(), seed.y(), seed.z(), seed_velocity.length()};
        streamline.points.push_back(seed_point);

        streamline.points.insert(streamline.points.end(), forward_points.begin(),
                                 forward_points.end());

        streamline.total_length = backward_length + forward_length;
        streamline.valid = streamline.points.size() > 1;
        // qDebug() << "Generated streamline with" << streamline.points.size()
        //          << "points, length:" << streamline.total_length;

        return streamline;
    }

    // ==================== RK4核心算法实现 ====================

    QVector3D StreamlineGenerator::rk4Step(const QVector3D& current_pos, float dt,
                                           const Field* vector_field,
                                           const std::vector<float>& mesh_vertices,
                                           const StreamlineParams& params)
    {
        const auto sampleVelocity = [&](const QVector3D& pos) {
            const QVector3D v = interpolateVector(pos, vector_field, mesh_vertices);
            if (params.use_physical_velocity)
            {
                return v;
            }

            const float mag = v.length();
            if (mag <= std::numeric_limits<float>::epsilon())
            {
                return QVector3D(0.0f, 0.0f, 0.0f);
            }
            return v / mag;
        };

        const QVector3D k1 = sampleVelocity(current_pos);
        if (k1.lengthSquared() <= 1e-12f)
        {
            return current_pos;
        }

        // k2 = f(t + dt/2, y + dt*k1/2)
        const QVector3D k2 = sampleVelocity(current_pos + 0.5f * dt * k1);

        // k3 = f(t + dt/2, y + dt*k2/2)
        const QVector3D k3 = sampleVelocity(current_pos + 0.5f * dt * k2);

        // k4 = f(t + dt, y + dt*k3)
        const QVector3D k4 = sampleVelocity(current_pos + dt * k3);

        // y(t+dt) = y(t) + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
        const QVector3D next_pos = current_pos + (dt / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);

        return next_pos;
    }

    // ==================== 矢量插值 ====================

    QVector3D StreamlineGenerator::interpolateVector(const QVector3D& pos,
                                                     const Field* vector_field,
                                                     const std::vector<float>& mesh_vertices)
    {
        if (!vector_field || vector_field->data.empty())
        {
            return QVector3D(0, 0, 0);
        }

        QVector3D cell_interp(0.0f, 0.0f, 0.0f);
        if (interpolateVectorInCell(pos, vector_field, mesh_vertices, cell_interp))
        {
            return cell_interp;
        }

        int num_vertices = mesh_vertices.size() / 3;
        const int K = std::min(8, num_vertices); // 使用最近的K个点（最多8个）

        // 使用八叉树查询最近的K个顶点（返回平方距离）
        const float pos_arr[3] = {pos.x(), pos.y(), pos.z()};
        auto distances = octree_.findNearestK(pos_arr, K);

        if (distances.empty())
        {
            return QVector3D(0, 0, 0);
        }

        // 若查询点远离网格采样支撑区域，则认为离域，返回零速度触发终止。
        if (support_radius_sq_ > 0.0f && distances[0].first > support_radius_sq_)
        {
            return QVector3D(0, 0, 0);
        }

        // 反距离加权（IDW）插值
        // 如果查询点恰好在顶点上（平方距离近似为0），直接返回该点的值
        if (distances[0].first < 1e-12f)
        {
            int idx = distances[0].second;
            if (idx * vector_field->num_components_ + 2 <
                static_cast<int>(vector_field->data.size()))
            {
                return QVector3D(
                    vector_field->data[idx * vector_field->num_components_],
                    vector_field->data[idx * vector_field->num_components_ + 1],
                    vector_field->data[idx * vector_field->num_components_ + 2]);
            }
        }

        // IDW插值：八叉树返回平方距离，power=2 时权重直接为 1/sq_dist
        float total_weight = 0.0f;
        QVector3D interpolated(0, 0, 0);

        for (const auto& [sq_dist, idx] : distances)
        {
            float weight = 1.0f / (sq_dist + 1e-12f);
            total_weight += weight;

            // 获取该点的向量值
            if (idx * vector_field->num_components_ + 2 <
                static_cast<int>(vector_field->data.size()))
            {
                QVector3D vec(vector_field->data[idx * vector_field->num_components_],
                             vector_field->data[idx * vector_field->num_components_ + 1],
                             vector_field->data[idx * vector_field->num_components_ + 2]);
                interpolated += weight * vec;
            }
        }

        // 归一化
        if (total_weight > 1e-6f)
        {
            interpolated /= total_weight;
        }

        return interpolated;
    }

    bool StreamlineGenerator::interpolateVectorInCell(const QVector3D& pos,
                                                      const Field* vector_field,
                                                      const std::vector<float>& mesh_vertices,
                                                      QVector3D& out_vec)
    {
        if (!mesh_part_ || supported_cell_ids_.empty() || !vector_field)
        {
            return false;
        }

        const size_t point_count = mesh_vertices.size() / 3;
        const int kCandidateVertices = std::min<int>(24, static_cast<int>(point_count));
        if (kCandidateVertices <= 0)
        {
            return false;
        }

        const float pos_arr[3] = {pos.x(), pos.y(), pos.z()};
        auto nearest = octree_.findNearestK(pos_arr, kCandidateVertices);
        if (nearest.empty())
        {
            return false;
        }

        // 热路径优化：使用线程局部去重表，避免每次 sort+unique 和频繁分配。
        thread_local std::vector<size_t> candidate_cells;
        thread_local std::vector<uint32_t> visited_stamp;
        thread_local uint32_t stamp = 1;
        thread_local size_t last_hit_cell = std::numeric_limits<size_t>::max();

        if (visited_stamp.size() < mesh_part_->cells_.size())
        {
            visited_stamp.resize(mesh_part_->cells_.size(), 0u);
        }

        ++stamp;
        if (stamp == 0u)
        {
            std::fill(visited_stamp.begin(), visited_stamp.end(), 0u);
            stamp = 1u;
        }

        candidate_cells.clear();
        candidate_cells.reserve(256);
        for (const auto& [sq_dist, vid] : nearest)
        {
            (void)sq_dist;
            if (vid < 0 || static_cast<size_t>(vid) >= vertex_to_supported_cells_.size())
            {
                continue;
            }

            const auto& cells = vertex_to_supported_cells_[static_cast<size_t>(vid)];
            for (size_t cid : cells)
            {
                if (cid >= visited_stamp.size())
                {
                    continue;
                }
                if (visited_stamp[cid] == stamp)
                {
                    continue;
                }
                visited_stamp[cid] = stamp;
                candidate_cells.push_back(cid);
            }
        }

        if (candidate_cells.empty())
        {
            return false;
        }

        constexpr std::array<std::array<int, 4>, 5> kHexTets = {
            std::array<int, 4>{0, 1, 3, 4}, std::array<int, 4>{1, 2, 3, 6},
            std::array<int, 4>{1, 3, 4, 6}, std::array<int, 4>{1, 4, 5, 6},
            std::array<int, 4>{3, 4, 6, 7}};

        auto tryCell = [&](size_t cell_id) -> bool
        {
            if (cell_id >= mesh_part_->cells_.size())
            {
                return false;
            }

            const CellData& cell = mesh_part_->cells_[cell_id];
            bool valid_points = true;
            for (uint8_t i = 0; i < cell.num_points; ++i)
            {
                if (cell.point_ids[i] >= point_count)
                {
                    valid_points = false;
                    break;
                }
            }
            if (!valid_points)
            {
                return false;
            }

            const QVector3D& bmin = supported_cell_min_[cell_id];
            const QVector3D& bmax = supported_cell_max_[cell_id];
            constexpr float kBboxEps = 1e-5f;
            if (pos.x() < bmin.x() - kBboxEps || pos.x() > bmax.x() + kBboxEps ||
                pos.y() < bmin.y() - kBboxEps || pos.y() > bmax.y() + kBboxEps ||
                pos.z() < bmin.z() - kBboxEps || pos.z() > bmax.z() + kBboxEps)
            {
                return false;
            }

            if (cell.kind == CellKind::TETRA && cell.num_points >= 4)
            {
                const QVector3D p0 = vertexAt(mesh_vertices, cell.point_ids[0]);
                const QVector3D p1 = vertexAt(mesh_vertices, cell.point_ids[1]);
                const QVector3D p2 = vertexAt(mesh_vertices, cell.point_ids[2]);
                const QVector3D p3 = vertexAt(mesh_vertices, cell.point_ids[3]);

                std::array<float, 4> w{0.0f, 0.0f, 0.0f, 0.0f};
                if (!barycentricInTetra(pos, p0, p1, p2, p3, w))
                {
                    return false;
                }

                if (vector_field->location_ == Location::CELL)
                {
                    out_vec = sampleCellVector(vector_field, cell_id);
                    last_hit_cell = cell_id;
                    return true;
                }

                const QVector3D v0 = samplePointVector(vector_field, cell.point_ids[0]);
                const QVector3D v1 = samplePointVector(vector_field, cell.point_ids[1]);
                const QVector3D v2 = samplePointVector(vector_field, cell.point_ids[2]);
                const QVector3D v3 = samplePointVector(vector_field, cell.point_ids[3]);
                out_vec = v0 * w[0] + v1 * w[1] + v2 * w[2] + v3 * w[3];
                last_hit_cell = cell_id;
                return true;
            }

            if (cell.kind == CellKind::HEXAHEDRON && cell.num_points >= 8)
            {
                bool found = false;
                std::array<float, 4> w{0.0f, 0.0f, 0.0f, 0.0f};
                std::array<uint32_t, 4> tet_ids{0, 0, 0, 0};

                for (const auto& t : kHexTets)
                {
                    tet_ids = {cell.point_ids[static_cast<size_t>(t[0])],
                               cell.point_ids[static_cast<size_t>(t[1])],
                               cell.point_ids[static_cast<size_t>(t[2])],
                               cell.point_ids[static_cast<size_t>(t[3])]};

                    const QVector3D p0 = vertexAt(mesh_vertices, tet_ids[0]);
                    const QVector3D p1 = vertexAt(mesh_vertices, tet_ids[1]);
                    const QVector3D p2 = vertexAt(mesh_vertices, tet_ids[2]);
                    const QVector3D p3 = vertexAt(mesh_vertices, tet_ids[3]);

                    if (barycentricInTetra(pos, p0, p1, p2, p3, w))
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    return false;
                }

                if (vector_field->location_ == Location::CELL)
                {
                    out_vec = sampleCellVector(vector_field, cell_id);
                    last_hit_cell = cell_id;
                    return true;
                }

                const QVector3D v0 = samplePointVector(vector_field, tet_ids[0]);
                const QVector3D v1 = samplePointVector(vector_field, tet_ids[1]);
                const QVector3D v2 = samplePointVector(vector_field, tet_ids[2]);
                const QVector3D v3 = samplePointVector(vector_field, tet_ids[3]);
                out_vec = v0 * w[0] + v1 * w[1] + v2 * w[2] + v3 * w[3];
                last_hit_cell = cell_id;
                return true;
            }

            return false;
        };

        if (last_hit_cell < visited_stamp.size() &&
            visited_stamp[last_hit_cell] == stamp)
        {
            if (tryCell(last_hit_cell))
            {
                return true;
            }
        }

        for (size_t cell_id : candidate_cells)
        {
            if (cell_id == last_hit_cell)
            {
                continue;
            }
            if (tryCell(cell_id))
            {
                return true;
            }
        }

        return false;
    }

    // ==================== 边界检测 ====================

    bool StreamlineGenerator::isWithinBounds(const QVector3D& pos)
    {
        return pos.x() >= mesh_min_.x() && pos.x() <= mesh_max_.x() && pos.y() >= mesh_min_.y() &&
               pos.y() <= mesh_max_.y() && pos.z() >= mesh_min_.z() && pos.z() <= mesh_max_.z();
    }

    // ==================== 后处理平滑 ====================

    void StreamlineGenerator::smoothStreamlines(std::vector<Streamline>& streamlines, int iterations)
    {
        for (auto& streamline : streamlines)
        {
            if (streamline.points.size() < 3)
            {
                continue;
            }

            // Catmull-Rom 风格平滑：多次迭代
            for (int iter = 0; iter < iterations; ++iter)
            {
                std::vector<StreamlinePoint> smoothed_points;
                smoothed_points.reserve(streamline.points.size());

                // 保留第一个点
                smoothed_points.push_back(streamline.points[0]);

                // 对内部点进行平滑
                for (size_t i = 1; i < streamline.points.size() - 1; ++i)
                {
                    const StreamlinePoint& p0 = streamline.points[i - 1];
                    const StreamlinePoint& p1 = streamline.points[i];
                    const StreamlinePoint& p2 = streamline.points[i + 1];

                    // 简单的 Catmull-Rom 三次样条：新位置 = 0.5 * (p0 + p2)
                    // 或者用加权平均（当前点权重较大）来保持原始拓扑
                    const float alpha = 0.5f; // 平滑强度（0~1，值越大越平滑）
                    const QVector3D p0_pos(p0.x, p0.y, p0.z);
                    const QVector3D p1_pos(p1.x, p1.y, p1.z);
                    const QVector3D p2_pos(p2.x, p2.y, p2.z);

                    // 邻域加权平均（避免过度平滑变形）
                    const QVector3D neighbor_avg = (p0_pos + p2_pos) * 0.5f;
                    const QVector3D smoothed_pos =
                        p1_pos * (1.0f - alpha) + neighbor_avg * alpha;

                    StreamlinePoint smoothed_point;
                    smoothed_point.x = smoothed_pos.x();
                    smoothed_point.y = smoothed_pos.y();
                    smoothed_point.z = smoothed_pos.z();
                    smoothed_point.magnitude = p1.magnitude; // 保留原始幅度
                    smoothed_points.push_back(smoothed_point);
                }

                // 保留最后一个点
                smoothed_points.push_back(streamline.points.back());

                streamline.points = smoothed_points;
            }

            // 重新计算总长度
            streamline.total_length = 0.0f;
            for (size_t i = 1; i < streamline.points.size(); ++i)
            {
                const StreamlinePoint& prev = streamline.points[i - 1];
                const StreamlinePoint& curr = streamline.points[i];
                const QVector3D prev_pos(prev.x, prev.y, prev.z);
                const QVector3D curr_pos(curr.x, curr.y, curr.z);
                streamline.total_length += (curr_pos - prev_pos).length();
            }
        }
    }

} // namespace Streamline

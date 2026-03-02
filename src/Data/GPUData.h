#pragma once
#include <vector>
#include <cstdint>
class GPUData
{
public:
    // 表面网格数据
    std::vector<float> surface_vertices_;
    std::vector<float> scalar_fields_;
    std::vector<float> normals_;
    std::vector<uint32_t> indices_;
    float scalar_min_;
    float scalar_max_;
    
    // 矢量场数据（用于箭头渲染）
    std::vector<float> vector_field_positions_;  // 矢量起始点 (x, y, z)
    std::vector<float> vector_field_directions_; // 矢量方向 (vx, vy, vz)
    std::vector<float> vector_field_magnitudes_; // 矢量幅值 (scalar value)
    float vector_magnitude_min_;
    float vector_magnitude_max_;

    // 流线数据
    std::vector<float> streamline_vertices_; // 流线顶点 (x, y, z)
    std::vector<float> streamline_magnitudes_; // 流线幅值 (scalar value)
    std::vector<uint32_t> streamline_indices_; // 流线索引
    std::vector<uint32_t> streamline_counts_;  // 每条流线的顶点数量
    float streamline_magnitude_min_;
    float streamline_magnitude_max_;
};
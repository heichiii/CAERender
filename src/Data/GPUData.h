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
    std::vector<float> streamline_vertices_; // 所有流线的连续顶点序列 (x, y, z)
    std::vector<float> streamline_magnitudes_; // 对应顶点的矢量幅值
    std::vector<uint32_t> streamline_line_starts_; // 每条流线在顶点数组中的起始索引
    std::vector<uint32_t> streamline_line_counts_;  // 每条流线的顶点数
    float streamline_magnitude_min_;
    float streamline_magnitude_max_;
    // 在数据上传到显存后，可调用此方法释放占用大量 RAM 的 CPU-side 副本
    void releaseCPUMemory()
    {
        std::vector<float>().swap(surface_vertices_);
        std::vector<float>().swap(scalar_fields_);
        std::vector<float>().swap(normals_);
        std::vector<uint32_t>().swap(indices_);

        std::vector<float>().swap(vector_field_positions_);
        std::vector<float>().swap(vector_field_directions_);
        std::vector<float>().swap(vector_field_magnitudes_);

        std::vector<float>().swap(streamline_vertices_);
        std::vector<float>().swap(streamline_magnitudes_);
        std::vector<uint32_t>().swap(streamline_line_starts_);
        std::vector<uint32_t>().swap(streamline_line_counts_);
    }
};
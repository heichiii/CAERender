#include "TimeStepData.h"
#include <execution>
#include <cmath>
#include <iostream>
#include "TestTool/Profiler.h"
void TimeStepData::generateGPUData()
{
    PROFILE_CODE
    if (parts_.empty())
    {
        std::cerr << "[TimeStepData::generateGPUData] Empty parts_." << std::endl;
        return;
    }
    if (parts_[0].vertices_.empty())
    {
        std::cerr << "[TimeStepData::generateGPUData] Empty vertices_." << std::endl;
        return;
    }
    if (parts_[0].vertices_.size() % 3 != 0)
    {
        std::cerr << "[TimeStepData::generateGPUData] Invalid vertices_ size." << std::endl;
        return;
    }
    if (parts_[0].faces_.empty())
    {
        std::cerr << "[TimeStepData::generateGPUData] Empty faces_." << std::endl;
        return;
    }
    gpu_data_.surface_vertices_.clear();
    // gpu_data_.scalar_fields_.clear();
    gpu_data_.normals_.clear();
    gpu_data_.indices_.clear();

    //表面提取：提取只出现一次的边界面
    std::sort(std::execution::par_unseq, parts_[0].faces_.begin(), parts_[0].faces_.end()); // TODO:openMP std::execution对比
    std::vector<size_t> boundary_face_indices;
    boundary_face_indices.reserve(parts_[0].faces_.size());
    for (size_t i = 0; i < parts_[0].faces_.size(); ++i)
    {
        const uint8_t nv = parts_[0].faces_[i].num_vertices;
        if (nv < 3 || nv > 4)
        {
            std::cerr << "[TimeStepData::generateGPUData] Invalid face vertex count." << std::endl;
            continue;
        }
        // 检查这个面是否是边界面（只出现一次）
        bool is_boundary = true;
        // 检查前一个面
        if (i > 0 && parts_[0].faces_[i] == parts_[0].faces_[i - 1])
            is_boundary = false;
        // 检查后一个面
        if (i + 1 < parts_[0].faces_.size() && parts_[0].faces_[i] == parts_[0].faces_[i + 1])
            is_boundary = false;
        
        if (is_boundary)
        {
            boundary_face_indices.push_back(i);
        }
    }
    // 生成flat shading顶点
    size_t num_triangles = 0;
    for (const auto& face : parts_[0].faces_)
    {
        if (face.num_vertices < 3 || face.num_vertices > 4)
            continue;
        num_triangles += static_cast<size_t>(face.num_vertices - 2); // 三角形数量
    }
    gpu_data_.surface_vertices_.reserve(num_triangles * 9); // 每个三角形3个顶点，每个顶点3个坐标
    gpu_data_.normals_.reserve(num_triangles * 9); // 每个三角形3个顶点，每个顶点3个坐标
    gpu_data_.indices_.reserve(num_triangles * 3); // 每个三角形3个顶点索引

    size_t vertex_index = 0;
    auto emitTriangle = [&](const Face& face, uint32_t i0, uint32_t i1, uint32_t i2)
    {
        const size_t vcount = parts_[0].vertices_.size() / 3;
        if (i0 >= face.num_vertices || i1 >= face.num_vertices || i2 >= face.num_vertices)
        {
            std::cerr << "[TimeStepData::generateGPUData] Triangle index out of face range." << std::endl;
            return;
        }
        if (face.original[i0] >= vcount || face.original[i1] >= vcount || face.original[i2] >= vcount)
        {
            std::cerr << "[TimeStepData::generateGPUData] Face index out of range." << std::endl;
            return;
        }
        float v0[3] = { parts_[0].vertices_[face.original[i0] * 3], parts_[0].vertices_[face.original[i0] * 3 + 1], parts_[0].vertices_[face.original[i0] * 3 + 2] };
        float v1[3] = { parts_[0].vertices_[face.original[i1] * 3], parts_[0].vertices_[face.original[i1] * 3 + 1], parts_[0].vertices_[face.original[i1] * 3 + 2] };
        float v2[3] = { parts_[0].vertices_[face.original[i2] * 3], parts_[0].vertices_[face.original[i2] * 3 + 1], parts_[0].vertices_[face.original[i2] * 3 + 2] };
        gpu_data_.surface_vertices_.insert(gpu_data_.surface_vertices_.end(), { v0[0], v0[1], v0[2], v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] });
        // 计算法线
        float edge1[3] = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] };
        float edge2[3] = { v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2] };
        float normal[3] = { edge1[1] * edge2[2] - edge1[2] * edge2[1], edge1[2] * edge2[0] - edge1[0] * edge2[2], edge1[0] * edge2[1] - edge1[1] * edge2[0] };
        float length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
        if (length > 1e-6f)        {
            normal[0] /= length;
            normal[1] /= length;
            normal[2] /= length;
        }
        gpu_data_.normals_.insert(gpu_data_.normals_.end(), { normal[0], normal[1], normal[2], normal[0], normal[1], normal[2], normal[0], normal[1], normal[2] });
        gpu_data_.indices_.insert(gpu_data_.indices_.end(), { static_cast<uint32_t>(vertex_index), static_cast<uint32_t>(vertex_index + 1), static_cast<uint32_t>(vertex_index + 2) });
        vertex_index += 3;
    };
    for (size_t idx : boundary_face_indices)
    {
        const Face& face = parts_[0].faces_[idx];
        for (size_t i = 1; i + 1 < face.num_vertices; ++i)
        {
            emitTriangle(face, 0, static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1));
        }
    }


}
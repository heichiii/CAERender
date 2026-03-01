#include "TimeStepData.h"
#include "TestTool/Profiler.h"
#include <cmath>
#include <limits>
#ifdef emit
#undef emit
#endif
#include <execution>
#include <iostream>
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
    gpu_data_.scalar_fields_.clear();
    gpu_data_.normals_.clear();
    gpu_data_.indices_.clear();

    // 表面提取：提取只出现一次的边界面
    std::sort(std::execution::par_unseq, parts_[0].faces_.begin(),
              parts_[0].faces_.end()); // TODO:openMP std::execution对比
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
    gpu_data_.normals_.reserve(num_triangles * 9);          // 每个三角形3个顶点，每个顶点3个坐标
    gpu_data_.indices_.reserve(num_triangles * 3);          // 每个三角形3个顶点索引
    // if(parts_[0].active_field_ && parts_[0].active_field_->type_ == Type::SCALAR)
    // {
    //     gpu_data_.scalar_fields_.reserve(num_triangles * 3); //
    //     每个三角形3个顶点，每个顶点1个标量值
    // }

    size_t vertex_index = 0;
    auto emitTriangle = [&](const Face& face, uint32_t i0, uint32_t i1, uint32_t i2)
    {
        const size_t vcount = parts_[0].vertices_.size() / 3;
        if (i0 >= face.num_vertices || i1 >= face.num_vertices || i2 >= face.num_vertices)
        {
            std::cerr << "[TimeStepData::generateGPUData] Triangle index out of face range."
                      << std::endl;
            return;
        }
        if (face.original[i0] >= vcount || face.original[i1] >= vcount ||
            face.original[i2] >= vcount)
        {
            std::cerr << "[TimeStepData::generateGPUData] Face index out of range." << std::endl;
            return;
        }
        float v0[3] = {parts_[0].vertices_[face.original[i0] * 3],
                       parts_[0].vertices_[face.original[i0] * 3 + 1],
                       parts_[0].vertices_[face.original[i0] * 3 + 2]};
        float v1[3] = {parts_[0].vertices_[face.original[i1] * 3],
                       parts_[0].vertices_[face.original[i1] * 3 + 1],
                       parts_[0].vertices_[face.original[i1] * 3 + 2]};
        float v2[3] = {parts_[0].vertices_[face.original[i2] * 3],
                       parts_[0].vertices_[face.original[i2] * 3 + 1],
                       parts_[0].vertices_[face.original[i2] * 3 + 2]};
        gpu_data_.surface_vertices_.insert(
            gpu_data_.surface_vertices_.end(),
            {v0[0], v0[1], v0[2], v1[0], v1[1], v1[2], v2[0], v2[1], v2[2]});
        // 计算法线
        float edge1[3] = {v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]};
        float edge2[3] = {v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]};
        float normal[3] = {edge1[1] * edge2[2] - edge1[2] * edge2[1],
                           edge1[2] * edge2[0] - edge1[0] * edge2[2],
                           edge1[0] * edge2[1] - edge1[1] * edge2[0]};
        float length =
            std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
        if (length > 1e-6f)
        {
            normal[0] /= length;
            normal[1] /= length;
            normal[2] /= length;
        }
        gpu_data_.normals_.insert(gpu_data_.normals_.end(),
                                  {normal[0], normal[1], normal[2], normal[0], normal[1], normal[2],
                                   normal[0], normal[1], normal[2]});
        gpu_data_.indices_.insert(gpu_data_.indices_.end(),
                                  {static_cast<uint32_t>(vertex_index),
                                   static_cast<uint32_t>(vertex_index + 1),
                                   static_cast<uint32_t>(vertex_index + 2)});

        parts_[0].vertex_to_point_map_.push_back(face.original[i0]);
        parts_[0].vertex_to_point_map_.push_back(face.original[i1]);
        parts_[0].vertex_to_point_map_.push_back(face.original[i2]);
        parts_[0].vertex_to_cell_map_.push_back(face.cell_id);
        parts_[0].vertex_to_cell_map_.push_back(face.cell_id);
        parts_[0].vertex_to_cell_map_.push_back(face.cell_id);

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

void TimeStepData::activateField(const std::string& field_name)
{
    if(field_name == "无")
    {
        for (auto& part : parts_)
        {
            part.active_field_ = nullptr;
        }
        gpu_data_.scalar_fields_.clear();
        gpu_data_.scalar_min_ = 0.0f;
        gpu_data_.scalar_max_ = 1.0f;
        return;
    }
    for (auto& part : parts_)
    {
        for (const auto& field : part.point_fields_)
        {
            if (field.name_ == field_name)
            {
                part.active_field_ = &field;
                updateScalarBuffer(); // 激活新字段后更新GPU缓冲区
                gpu_data_.scalar_min_ = field.min_value;
                gpu_data_.scalar_max_ = field.max_value;
                return;
            }
        }
        for (const auto& field : part.cell_fields_)
        {
            if (field.name_ == field_name)
            {
                part.active_field_ = &field;
                updateScalarBuffer(); // 激活新字段后更新GPU缓冲区
                gpu_data_.scalar_min_ = field.min_value;
                gpu_data_.scalar_max_ = field.max_value;
                return;
            }
        }
    }
    std::cerr << "[TimeStepData::activateField] Field not found: " << field_name << std::endl;
}

void TimeStepData::updateScalarBuffer()
{
    if (parts_.empty() || !parts_[0].active_field_)
    {
        std::cerr << "[TimeStepData::updateScalarBuffer] No active field to update." << std::endl;
        return;
    }
    const Field* active_field = parts_[0].active_field_;
    if (active_field->type_ != Type::SCALAR)
    {
        std::cerr << "[TimeStepData::updateScalarBuffer] Active field is not scalar." << std::endl;
        return;
    }
    gpu_data_.scalar_fields_.clear();
    gpu_data_.scalar_fields_.resize(parts_[0].vertex_to_point_map_.size());
    
    std::cout << "[TimeStepData::updateScalarBuffer] Updating " << gpu_data_.scalar_fields_.size() 
              << " scalar values from field: " << active_field->name_
              << " (location: " << (active_field->location_ == Location::POINT ? "POINT" : "CELL")
              << ", min: " << active_field->min_value << ", max: " << active_field->max_value << ")" << std::endl;
    
    // 根据场量位置选择不同的映射
    if (active_field->location_ == Location::POINT)
    {
        // 点场：使用vertex_to_point_map_
        for (size_t vertex_idx = 0; vertex_idx < parts_[0].vertex_to_point_map_.size(); ++vertex_idx)
        {
            size_t point_idx = parts_[0].vertex_to_point_map_[vertex_idx];
            if (point_idx < active_field->num_tuples_)
            {
                gpu_data_.scalar_fields_[vertex_idx] =
                    active_field->data[point_idx * active_field->num_components_];
            }
            else
            {
                gpu_data_.scalar_fields_[vertex_idx] = 0.0f;
                std::cerr << "[TimeStepData::updateScalarBuffer] Point index out of range: "
                          << point_idx << std::endl;
            }
        }
    }
    else if (active_field->location_ == Location::CELL)
    {
        // 单元场：使用vertex_to_cell_map_
        for (size_t vertex_idx = 0; vertex_idx < parts_[0].vertex_to_cell_map_.size(); ++vertex_idx)
        {
            size_t cell_idx = parts_[0].vertex_to_cell_map_[vertex_idx];
            if (cell_idx < active_field->num_tuples_)
            {
                gpu_data_.scalar_fields_[vertex_idx] =
                    active_field->data[cell_idx * active_field->num_components_];
            }
            else
            {
                gpu_data_.scalar_fields_[vertex_idx] = 0.0f;
                std::cerr << "[TimeStepData::updateScalarBuffer] Cell index out of range: "
                          << cell_idx << std::endl;
            }
        }
    }
}

void TimeStepData::updateVectorBuffer(const std::string& field_name)
{
    qInfo() << "[TimeStepData::updateVectorBuffer] Updating vector buffer for field: " << QString::fromStdString(field_name);
    if (parts_.empty())
    {
        std::cerr << "[TimeStepData::updateVectorBuffer] No parts available." << std::endl;
        return;
    }
    
    // 查找矢量字段
    const Field* vector_field = nullptr;
    for (const auto& field : parts_[0].point_fields_)
    {
        if (field.name_ == field_name && field.type_ == Type::VECTOR)
        {
            vector_field = &field;
            break;
        }
    }
    
    // 如果在点场中没找到，尝试在单元场中查找
    if (!vector_field)
    {
        for (const auto& field : parts_[0].cell_fields_)
        {
            if (field.name_ == field_name && field.type_ == Type::VECTOR)
            {
                vector_field = &field;
                break;
            }
        }
    }
    
    if (!vector_field)
    {
        std::cerr << "[TimeStepData::updateVectorBuffer] Vector field not found: " << field_name << std::endl;
        return;
    }
    
    // 清空旧数据
    gpu_data_.vector_field_positions_.clear();
    gpu_data_.vector_field_directions_.clear();
    gpu_data_.vector_field_magnitudes_.clear();
    
    // 获取表面网格顶点作为向量起始点（使用surface_vertices_而不是原始vertices_）
    const auto& surface_vertices = gpu_data_.surface_vertices_;
    const auto& vertex_map = (vector_field->location_ == Location::POINT) ? 
                             parts_[0].vertex_to_point_map_ : 
                             parts_[0].vertex_to_cell_map_;
    
    gpu_data_.vector_field_positions_.reserve(surface_vertices.size());
    gpu_data_.vector_field_directions_.reserve(surface_vertices.size());
    gpu_data_.vector_field_magnitudes_.reserve(vertex_map.size());
    
    float min_magnitude = std::numeric_limits<float>::max();
    float max_magnitude = -std::numeric_limits<float>::max();
    
    // 为每个顶点提取矢量值
    for (size_t vertex_idx = 0; vertex_idx < vertex_map.size(); ++vertex_idx)
    {
        size_t data_idx = vertex_map[vertex_idx];
        
        if (data_idx >= vector_field->num_tuples_)
        {
            std::cerr << "[TimeStepData::updateVectorBuffer] Index out of range: " 
                      << data_idx << std::endl;
            continue;
        }
        
        // 获取顶点位置（从surface_vertices_获取）
        if (vertex_idx * 3 + 2 < surface_vertices.size())
        {
            gpu_data_.vector_field_positions_.push_back(surface_vertices[vertex_idx * 3]);
            gpu_data_.vector_field_positions_.push_back(surface_vertices[vertex_idx * 3 + 1]);
            gpu_data_.vector_field_positions_.push_back(surface_vertices[vertex_idx * 3 + 2]);
        }
        else
        {
            std::cerr << "[TimeStepData::updateVectorBuffer] Vertex index out of surface_vertices range: " 
                      << vertex_idx << std::endl;
            continue;
        }
        
        // 获取矢量方向 (取前三个分量)
        float vx = vector_field->data[data_idx * vector_field->num_components_];
        float vy = (vector_field->num_components_ > 1) ? vector_field->data[data_idx * vector_field->num_components_ + 1] : 0.0f;
        float vz = (vector_field->num_components_ > 2) ? vector_field->data[data_idx * vector_field->num_components_ + 2] : 0.0f;
        
        // 计算幅值
        float magnitude = std::sqrt(vx * vx + vy * vy + vz * vz);
        min_magnitude = std::min(min_magnitude, magnitude);
        max_magnitude = std::max(max_magnitude, magnitude);
        
        // 归一化方向
        if (magnitude > 1e-6f)
        {
            vx /= magnitude;
            vy /= magnitude;
            vz /= magnitude;
        }
        
        gpu_data_.vector_field_directions_.push_back(vx);
        gpu_data_.vector_field_directions_.push_back(vy);
        gpu_data_.vector_field_directions_.push_back(vz);
        gpu_data_.vector_field_magnitudes_.push_back(magnitude);
    }
    
    gpu_data_.vector_magnitude_min_ = min_magnitude;
    gpu_data_.vector_magnitude_max_ = max_magnitude;
    
    std::string location_str = (vector_field->location_ == Location::POINT) ? "POINT" : "CELL";
    std::cout << "[TimeStepData::updateVectorBuffer] Generated " 
              << gpu_data_.vector_field_positions_.size() / 3 << " vectors from " 
              << location_str << " field: " << field_name
              << " (magnitude range: [" << min_magnitude << ", " << max_magnitude << "])" << std::endl;
    
    if (vector_field->location_ == Location::POINT)
    {
        std::cout << "  → POINT data: 每个顶点从对应的点获取矢量值，三角形的三个顶点可能有不同的矢量方向" << std::endl;
    }
    else if (vector_field->location_ == Location::CELL)
    {
        std::cout << "  → CELL data: 每个顶点从所属单元获取矢量值，三角形的三个顶点将显示相同的矢量方向" << std::endl;
    }
}

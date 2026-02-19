#pragma once
#include <vector>
class GPUData
{
    // TODO: GPU数据结构
public:
    std::vector<float> surface_vertices_; //TODO: 面提取->表面提取->三角化
    std::vector<float> scalar_fields_; //TODO:  标量数据数组提取
    std::vector<float> normals_; //TODO: 法线计算
    std::vector<size_t> indices_; //TODO: 索引提取
};
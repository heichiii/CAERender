#pragma once
#include <vector>
class GPUData
{
public:
    std::vector<float> surface_vertices_;
    // std::vector<float> scalar_fields_; //TODO:  标量数据数组提取
    std::vector<float> normals_;
    std::vector<size_t> indices_;
};
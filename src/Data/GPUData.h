#pragma once
#include <vector>
#include <cstdint>
class GPUData
{
public:
    std::vector<float> surface_vertices_;
    std::vector<float> scalar_fields_;
    std::vector<float> normals_;
    std::vector<uint32_t> indices_;
    float scalar_min_;
    float scalar_max_;
};
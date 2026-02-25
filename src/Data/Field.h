#pragma once

#include <string>
#include <vector>


enum class Location
{
    POINT,
    CELL
};
enum class Type
{
    SCALAR,
    VECTOR,
    TENSOR,
    OTHER
};
class Field
{
public:
    std::string name_;
    Location location_=Location::POINT;
    Type type_=Type::SCALAR;
    int num_components_;
    int num_tuples_;
    std::vector<float> data;
    
    // 统计信息
    float min_value;
    float max_value;
    
    void computeRange();

};
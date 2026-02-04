#pragma once

#include <string>
#include <vtkDataArray.h>
#include <vtkSmartPointer.h>

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
    Location location_;
    Type type_;
    int num_components_;
    vtkIdType num_tuples_;
    vtkSmartPointer<vtkDataArray> pdata_; // 实际数据指针
};
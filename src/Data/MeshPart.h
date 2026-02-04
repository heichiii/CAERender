#pragma once

#include "Field.h"
#include <string>
#include <vector>
#include <vtkDataSet.h>
#include <vtkSmartPointer.h>
#include <vtkCellData.h>
#include <vtkPointData.h>

class MeshPart
{
public:
    std::string name_;    // 部件名或文件名
    bool visible_ = true; // 渲染是否可见
    vtkSmartPointer<vtkDataSet> dataset_;

    vtkPoints* points_ = nullptr;
    vtkPointData* point_data_ = nullptr;

    vtkIdType num_cells_ = 0;
    vtkCellData* cell_data_ = nullptr;


    std::vector<Field> point_fields_; // 点上物理量集合
    std::vector<Field> cell_fields_;  // 单元上物理量集合
};
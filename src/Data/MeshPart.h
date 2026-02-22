#pragma once

#include "Field.h"
#include "Face.h"
#include <string>
#include <vector>

class MeshPart
{
public:
    std::string name_;    // 部件名或文件名
    bool visible_ = true; // 渲染是否可见
    std::vector<float> vertices_; // 顶点坐标数组
    std::vector<Field> point_fields_; // 点上物理量集合
    std::vector<Field> cell_fields_;  // 单元上物理量集合
    std::vector<Face> faces_; // 面集合（可选，根据需要提取）
};
#pragma once

#include "Field.h"
#include "Face.h"
#include <array>
#include <string>
#include <vector>

enum class CellKind
{
    UNKNOWN = 0,
    TETRA = 1,
    HEXAHEDRON = 2
};

struct CellData
{
    CellKind kind = CellKind::UNKNOWN;
    uint8_t num_points = 0;
    std::array<uint32_t, 8> point_ids{UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX,
                                      UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
};

class MeshPart
{
public:
    std::string name_;    // 部件名或文件名
    bool visible_ = true; // 渲染是否可见
    std::vector<float> vertices_; // 顶点坐标数组
    std::vector<Field> point_fields_; // 点上物理量集合
    std::vector<Field> cell_fields_;  // 单元上物理量集合
    std::vector<CellData> cells_; // 体单元连通性（用于流线单元内插值）
    std::vector<Face> faces_; // 面集合
    std::vector<size_t> vertex_to_point_map_; // 顶点索引到点索引的映射
    std::vector<size_t> vertex_to_cell_map_;  // 顶点索引到单元索引的映射
    Field* active_field_ = nullptr; // 当前激活的物理量指针
};
#pragma once
#include <cstdint>
#include <algorithm>

class Face
{
public:
    uint32_t sorted[4]; // 排序后的顶点索引
    uint32_t original[4]; // 原始顶点索引
    uint8_t num_vertices; // 面的顶点数量
    uint32_t cell_id; // 面所属单元ID

    void set3(uint32_t a,uint32_t b,uint32_t c)
    {
        sorted[0] = a;
        sorted[1] = b;
        sorted[2] = c;
        sorted[3] = 0; // 占位
        original[0] = a;
        original[1] = b;
        original[2] = c;
        original[3] = UINT32_MAX; // 占位
        num_vertices = 3;
        std::sort(sorted, sorted + 3);
    }
    void set4(uint32_t a,uint32_t b,uint32_t c,uint32_t d)
    {
        sorted[0] = a;
        sorted[1] = b;
        sorted[2] = c;
        sorted[3] = d;
        original[0] = a;
        original[1] = b;
        original[2] = c;
        original[3] = d;
        num_vertices = 4;
        std::sort(sorted, sorted + 4);
    }

    bool operator==(const Face& other) const
    {
        if (num_vertices != other.num_vertices)
            return false;
        for (uint8_t i = 0; i < num_vertices; ++i)
        {
            if (sorted[i] != other.sorted[i])
                return false;
        }
        return true;
    }

    bool operator<(const Face& other) const
    {
        if (num_vertices != other.num_vertices)
            return num_vertices < other.num_vertices;
        for (uint8_t i = 0; i < num_vertices; ++i)
        {
            if (sorted[i] != other.sorted[i])
                return sorted[i] < other.sorted[i];
        }
        return false; // 完全相同
    }

};
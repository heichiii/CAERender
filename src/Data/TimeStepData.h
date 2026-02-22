#pragma once
#include "MeshPart.h"
#include "GPUData.h"

class TimeStepData
{
public:
    double time_ = -1;           // 时间值, -1表示单文件
    std::vector<MeshPart> parts_; // 网格部件集合
    GPUData gpu_data_; // GPU数据结构，包含顶点缓冲区、物理量缓冲区等
    void generateGPUData();

};

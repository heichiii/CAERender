#pragma once
#include "MeshPart.h"
#include "GPUData.h"
#include <QVector3D>

class TimeStepData
{
public:

    TimeStepData() = default;
    double time_ = -1.0;           // 时间值, -1表示单文件
    std::vector<MeshPart> parts_; // 网格部件集合
    GPUData gpu_data_; // GPU数据结构，包含顶点缓冲区、物理量缓冲区等
    
    void generateGPUData();
    Type activateField(const std::string& field_name);
    void updateScalarBuffer();
    void updateVectorBuffer();
    void updateStreamlineBuffer(int num_seeds);
    void updateStreamlineBufferFromSphere(const QVector3D& center, float radius, int num_seeds);

};

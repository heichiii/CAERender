#pragma once

#include "MeshPart.h"

#include <vector>

class TimeStepData
{
public:
    double time_ = -1;           // 时间值, -1表示单文件
    std::vector<MeshPart> parts_; // 网格部件集合
};


class CaseData
{
public:
    std::vector<TimeStepData> timeSteps_; // 时间步集合
};
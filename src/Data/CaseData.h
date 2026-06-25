#pragma once

#include "TimeStepData.h"
#include <vector>



class CaseData
{
public:
    std::vector<TimeStepData> steps_; // 时间步集合
    int current_step_index_ = 0; // 当前时间步索引
};
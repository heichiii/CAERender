#pragma once

#include "TimeStepData.h"
#include <vector>



class CaseData
{
public:
    std::vector<TimeStepData> timeSteps_; // 时间步集合
};
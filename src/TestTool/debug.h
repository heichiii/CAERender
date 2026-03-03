#pragma once
#define DEBUG_MODE 1

// GPU 选择宏：定义使用哪种显卡
// 使用方式：
//   #define PREFER_DISCRETE_GPU     - 使用独立显卡（NVIDIA/AMD）
//   #define PREFER_INTEGRATED_GPU   - 使用集成显卡（Intel）
//   都不定义                        - 系统自动选择
#define PREFER_DISCRETE_GPU
// #define PREFER_INTEGRATED_GPU

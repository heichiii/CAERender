#pragma once

#include "Data/Field.h"
#include "Data/Octree.h"
#include <QVector3D>
#include <vector>

namespace Streamline
{

    // 流线参数配置
    struct StreamlineParams
    {
        float dt = 0.05f;            // 时间步长
        float max_length = 100.0f;   // 最大流线长度
        float min_velocity = 0.001f; // 最小速度阈值（停止条件）
        int max_iterations = 2000;   // 最大迭代次数
        int num_threads = 4;         // 并行线程数
    };

    // 流线点结构
    struct StreamlinePoint
    {
        float x, y, z;   // 空间位置
        float magnitude; // 矢量幅值
    };

    // 完整流线结构
    struct Streamline
    {
        std::vector<StreamlinePoint> points; // 流线上的点序列
        float total_length = 0.0f;           // 总长度
        bool valid = true;                   // 是否有效
    };

    // 流线生成算法类（使用四阶龙格库塔法）
    class StreamlineGenerator
    {
    public:
        StreamlineGenerator();
        ~StreamlineGenerator();

        /**
         * @brief 生成流线（主接口 - 支持并行）
         * @param seed_positions 种子点位置列表
         * @param vector_field 矢量场数据
         * @param mesh_vertices 网格顶点坐标
         * @param params 生成参数
         * @return 生成的流线列表
         */
        std::vector<Streamline> generate(const std::vector<QVector3D>& seed_positions,
                                         const Field* vector_field,
                                         const std::vector<float>& mesh_vertices,
                                         const StreamlineParams& params = StreamlineParams());

        /**
         * @brief 生成单条流线（内部使用）
         * @param seed 种子点
         * @param vector_field 矢量场数据
         * @param mesh_vertices 网格顶点
         * @param params 生成参数
         * @return 单条流线
         */
        Streamline generateSingleStreamline(const QVector3D& seed, const Field* vector_field,
                                            const std::vector<float>& mesh_vertices,
                                            const StreamlineParams& params);

    private:
        // ==================== RK4核心算法 ====================

        /**
         * @brief 四阶龙格库塔法单步积分
         * @param current_pos 当前位置
         * @param dt 时间步长
         * @param vector_field 矢量场
         * @param mesh_vertices 网格顶点
         * @return 新的位置
         */
        QVector3D rk4Step(const QVector3D& current_pos, float dt, const Field* vector_field,
                          const std::vector<float>& mesh_vertices);

        /**
         * @brief 在指定位置进行矢量插值（三线性插值）
         * @param pos 查询位置
         * @param vector_field 矢量场
         * @param mesh_vertices 网格顶点
         * @return 插值得到的矢量
         */
        QVector3D interpolateVector(const QVector3D& pos, const Field* vector_field,
                                    const std::vector<float>& mesh_vertices);

        // ==================== 辅助方法 ====================

        /**
         * @brief 检查点是否在网格边界内
         * @param pos 点位置
         * @return 是否在边界内
         */
        bool isWithinBounds(const QVector3D& pos);

        // ==================== 八叉树索引（加速最近邻查询）====================

        // 缓存网格边界用于快速边界检查
        QVector3D mesh_min_;
        QVector3D mesh_max_;
        bool bounds_initialized_ = false;

        // 八叉树空间索引
        Data::Octree octree_;
    };

} // namespace Streamline

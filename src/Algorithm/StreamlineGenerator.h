#pragma once

#include "Data/Field.h"
#include "Data/MeshPart.h"
#include <QVector3D>
#include <memory>
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
         * @brief 查找包含指定点的网格单元
         * @param pos 点位置
         * @param mesh_vertices 网格顶点
         * @return 包含该点的单元索引，不存在返回-1
         */
        int findContainingCell(const QVector3D& pos, const std::vector<float>& mesh_vertices);

        /**
         * @brief 检查点是否在网格边界内
         * @param pos 点位置
         * @param mesh_vertices 网格顶点
         * @return 是否在边界内
         */
        bool isWithinBounds(const QVector3D& pos, const std::vector<float>& mesh_vertices);

        /**
         * @brief 三线性插值（在网格单元内）
         * @param local_coords 单元局部坐标 [0,1]^3
         * @param values 8个顶点的矢量值
         * @return 插值结果
         */
        QVector3D trilinearInterpolate(const QVector3D& local_coords,
                                       const std::vector<QVector3D>& values);

        /**
         * @brief 计算点在单元中的局部坐标
         * @param point 全局坐标
         * @param cell_vertices 单元8个顶点坐标
         * @return 局部坐标 [0,1]^3，超出范围返回(-1,-1,-1)
         */
        QVector3D getLocalCoordinates(const QVector3D& point,
                                      const std::vector<QVector3D>& cell_vertices);

        // ==================== 网格索引（加速最近邻查询）====================

        /**
         * @brief 构建统一网格索引用于加速最近邻查询
         * @param mesh_vertices 网格顶点坐标
         * @param spacing 网格单元大小（建议为平均顶点间距的2-3倍）
         */
        void buildSpatialGrid(const std::vector<float>& mesh_vertices, float spacing = -1.0f);

        /**
         * @brief 使用网格索引查询最近的K个顶点
         * @param pos 查询位置
         * @param k 返回的最近邻数量
         * @param mesh_vertices 网格顶点坐标
         * @return (距离, 顶点索引)对的向量，按距离排序
         */
        std::vector<std::pair<float, int>> getNearestVertices(
            const QVector3D& pos, int k, const std::vector<float>& mesh_vertices) const;

        // 缓存网格边界用于快速检查
        QVector3D mesh_min_;
        QVector3D mesh_max_;
        bool bounds_initialized_ = false;

        // 网格索引相关成员变量
        std::vector<std::vector<int>> grid_cells_;      // 网格单元 -> 顶点索引列表
        int grid_dims_[3]{0, 0, 0};                     // 网格维度 [nx, ny, nz]
        float grid_spacing_ = 0.0f;                     // 网格单元大小
        QVector3D grid_origin_;                         // 网格原点（最小角）
        bool grid_built_ = false;                       // 网格是否已构建
    };

} // namespace Streamline

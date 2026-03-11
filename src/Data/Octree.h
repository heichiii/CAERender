#pragma once

#include <array>
#include <memory>
#include <queue>
#include <vector>

namespace Data
{

// ---------------------------------------------------------------------------
// Axis-aligned bounding box in 3-D
// ---------------------------------------------------------------------------
struct Bounds3
{
    float min[3]{0.f, 0.f, 0.f};
    float max[3]{0.f, 0.f, 0.f};

    /// Expand the box to include point @p p.
    void expand(const float p[3])
    {
        for (int i = 0; i < 3; ++i)
        {
            if (p[i] < min[i]) min[i] = p[i];
            if (p[i] > max[i]) max[i] = p[i];
        }
    }

    /// Write the center into @p c.
    void center(float c[3]) const
    {
        for (int i = 0; i < 3; ++i)
            c[i] = (min[i] + max[i]) * 0.5f;
    }

    /// True when p is inside or on the surface of the box.
    bool contains(const float p[3]) const
    {
        return p[0] >= min[0] && p[0] <= max[0] &&
               p[1] >= min[1] && p[1] <= max[1] &&
               p[2] >= min[2] && p[2] <= max[2];
    }

    /// Minimum squared distance from point @p p to this box (0 when inside).
    float sqDistanceTo(const float p[3]) const
    {
        float d = 0.f;
        for (int i = 0; i < 3; ++i)
        {
            if      (p[i] < min[i]) d += (min[i] - p[i]) * (min[i] - p[i]);
            else if (p[i] > max[i]) d += (p[i] - max[i]) * (p[i] - max[i]);
        }
        return d;
    }

    /**
     * Return the child sub-box for the given octant (0–7).
     * Bit layout: bit 0 = x-axis, bit 1 = y-axis, bit 2 = z-axis.
     *   0 = lower half,  1 = upper half.
     */
    Bounds3 child(int octant) const
    {
        float c[3];
        center(c);
        Bounds3 b;
        for (int i = 0; i < 3; ++i)
        {
            if ((octant >> i) & 1) { b.min[i] = c[i]; b.max[i] = max[i]; }
            else                   { b.min[i] = min[i]; b.max[i] = c[i]; }
        }
        return b;
    }
};

// ---------------------------------------------------------------------------
// Octree node
// ---------------------------------------------------------------------------
struct OctreeNode
{
    Bounds3 bounds;
    std::array<std::unique_ptr<OctreeNode>, 8> children{};
    std::vector<int> indices; ///< point indices (non-empty only in leaf nodes)
    bool is_leaf = true;
};

// ---------------------------------------------------------------------------
// Octree – point-cloud spatial index over a flat float array [x0,y0,z0, ...]
//
// Typical usage:
//   Data::Octree tree;
//   tree.build(mesh_vertices);
//   auto nearest = tree.findNearestK(pos, 8);   // (sq_dist, vertex_index) pairs
//   auto inArea  = tree.findInRadius(pos, 0.5f);
// ---------------------------------------------------------------------------
class Octree
{
public:
    static constexpr int kDefaultMaxDepth = 12;
    static constexpr int kDefaultMinLeaf  = 16;

    Octree() = default;

    /**
     * Build (or rebuild) the octree.
     * @param vertices      Flat vertex array: [x0,y0,z0, x1,y1,z1, ...]
     * @param max_depth     Maximum tree depth.
     * @param min_leaf      Stop splitting a node that has ≤ this many points.
     */
    void build(const std::vector<float>& vertices,
               int max_depth = kDefaultMaxDepth,
               int min_leaf  = kDefaultMinLeaf);

    bool isBuilt() const { return root_ != nullptr; }

    /// Reset and free all memory.
    void clear() { root_.reset(); vertices_ = nullptr; }

    /**
     * Find the K nearest points to @p pos.
     * @param pos  Query position (float[3]).
     * @param k    Number of results requested.
     * @return     Pairs of (squared_distance, vertex_index), sorted ascending by distance.
     */
    std::vector<std::pair<float, int>> findNearestK(const float pos[3], int k) const;

    /**
     * Find all points within @p radius of @p pos.
     * @return Unsorted list of vertex indices.
     */
    std::vector<int> findInRadius(const float pos[3], float radius) const;

    int totalPoints() const { return vertices_ ? static_cast<int>(vertices_->size() / 3) : 0; }

private:
    std::unique_ptr<OctreeNode> root_;
    const std::vector<float>*   vertices_  = nullptr;
    int max_depth_ = kDefaultMaxDepth;
    int min_leaf_  = kDefaultMinLeaf;

    void buildNode(OctreeNode* node, std::vector<int>& indices, int depth);

    // max-heap: pair<sq_dist, index>, largest distance at top for O(1) worst-dist access
    using MaxHeap = std::priority_queue<std::pair<float, int>>;
    void queryKNearest(const OctreeNode* node, const float pos[3], int k, MaxHeap& heap) const;
    void queryRadius(const OctreeNode* node, const float pos[3],
                     float sq_radius, std::vector<int>& result) const;

    const float* vertexPtr(int idx) const { return vertices_->data() + idx * 3; }

    static float sqDist3(const float* a, const float* b)
    {
        float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
        return dx * dx + dy * dy + dz * dz;
    }
};

} // namespace Data

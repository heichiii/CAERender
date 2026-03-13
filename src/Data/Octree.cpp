#include "Octree.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace Data
{

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

void Octree::build(const std::vector<float>& vertices, int max_depth, int min_leaf)
{
    root_.reset();
    if (vertices.empty() || vertices.size() % 3 != 0)
        return;

    vertices_  = &vertices;
    max_depth_ = max_depth;
    min_leaf_  = min_leaf;

    const int n = static_cast<int>(vertices.size() / 3);

    // Compute root AABB from all vertices
    Bounds3 root_bounds;
    const float* v0 = vertices.data();
    root_bounds.min[0] = root_bounds.max[0] = v0[0];
    root_bounds.min[1] = root_bounds.max[1] = v0[1];
    root_bounds.min[2] = root_bounds.max[2] = v0[2];
    for (int i = 0; i < n; ++i)
        root_bounds.expand(vertices.data() + i * 3);

    // Small epsilon prevents boundary ambiguity
    for (int i = 0; i < 3; ++i)
    {
        root_bounds.min[i] -= 1e-4f;
        root_bounds.max[i] += 1e-4f;
    }

    std::vector<int> all(n);
    for (int i = 0; i < n; ++i) all[i] = i;

    root_          = std::make_unique<OctreeNode>();
    root_->bounds  = root_bounds;
    buildNode(root_.get(), all, 0);
}

void Octree::buildNode(OctreeNode* node, std::vector<int>& indices, int depth)
{
    // TODO: 可以考虑在这里进行一些优化，例如当节点内点数较多但分布非常不均匀时，强制继续细分以避免过深的树和不平衡的分布导致查询效率下降。

    // Leaf condition: too deep or too few points
    if (depth >= max_depth_ || static_cast<int>(indices.size()) <= min_leaf_)
    {
        node->indices = std::move(indices);
        node->is_leaf = true;
        return;
    }

    node->is_leaf = false;

    //TODO:?
    float center[3];
    node->bounds.center(center);

    // Partition points into 8 octants
    // Bit layout: bit0=x, bit1=y, bit2=z (0=lower, 1=upper)
    std::vector<int> groups[8];
    for (int idx : indices)    // TODO: 并行？
    {
        const float* p = vertexPtr(idx);
        int octant = 0;
        if (p[0] >= center[0]) octant |= 1;
        if (p[1] >= center[1]) octant |= 2;
        if (p[2] >= center[2]) octant |= 4;
        groups[octant].push_back(idx);
    }
    // Release the input vector's memory now that we've partitioned
    { std::vector<int> tmp; tmp.swap(indices); }

    for (int o = 0; o < 8; ++o)
    {
        if (groups[o].empty()) continue;
        node->children[o]         = std::make_unique<OctreeNode>();
        node->children[o]->bounds = node->bounds.child(o);
        buildNode(node->children[o].get(), groups[o], depth + 1);
    }
}

// ---------------------------------------------------------------------------
// K-nearest query
// ---------------------------------------------------------------------------

std::vector<std::pair<float, int>> Octree::findNearestK(const float pos[3], int k) const
{
    if (!root_ || k <= 0)
        return {};

    MaxHeap heap; // max-heap: worst (largest) distance at the top
    queryKNearest(root_.get(), pos, k, heap);

    // Drain heap and reverse so result is sorted ascending
    std::vector<std::pair<float, int>> result;
    result.reserve(heap.size());
    while (!heap.empty())
    {
        result.push_back(heap.top());
        heap.pop();
    }
    std::reverse(result.begin(), result.end());
    return result;
}

void Octree::queryKNearest(const OctreeNode* node, const float pos[3],
                            int k, MaxHeap& heap) const
{
    // Prune: if the entire node bounding box is farther than the current k-th
    // nearest distance, there is nothing useful inside.
    float worst_sq = (static_cast<int>(heap.size()) == k)
                         ? heap.top().first
                         : std::numeric_limits<float>::max();

    if (node->bounds.sqDistanceTo(pos) >= worst_sq)
        return;

    if (node->is_leaf)
    {
        for (int idx : node->indices)
        {
            float d = sqDist3(vertexPtr(idx), pos);
            if (static_cast<int>(heap.size()) < k)
            {
                heap.push({d, idx});
            }
            else if (d < heap.top().first)
            {
                heap.pop();
                heap.push({d, idx});
            }
        }
        return;
    }

    // Visit the child whose sub-box contains pos first (best pruning heuristic)
    float center[3];
    node->bounds.center(center);
    int primary = 0;
    if (pos[0] >= center[0]) primary |= 1;
    if (pos[1] >= center[1]) primary |= 2;
    if (pos[2] >= center[2]) primary |= 4;

    if (node->children[primary])
        queryKNearest(node->children[primary].get(), pos, k, heap);

    for (int o = 0; o < 8; ++o)
    {
        if (o == primary || !node->children[o]) continue;
        queryKNearest(node->children[o].get(), pos, k, heap);
    }
}

// ---------------------------------------------------------------------------
// Radius query
// ---------------------------------------------------------------------------

std::vector<int> Octree::findInRadius(const float pos[3], float radius) const
{
    if (!root_ || radius <= 0.f)
        return {};

    std::vector<int> result;
    queryRadius(root_.get(), pos, radius * radius, result);
    return result;
}

std::vector<int> Octree::findRayCandidates(const float origin[3],
                                           const float dir_normalized[3],
                                           float radius,
                                           float t_max,
                                           int max_candidates) const
{
    if (!root_ || radius <= 0.f || t_max <= 0.f || max_candidates <= 0)
        return {};

    const float inv_dir[3] = {
        std::abs(dir_normalized[0]) > 1e-8f ? (1.f / dir_normalized[0]) : 0.f,
        std::abs(dir_normalized[1]) > 1e-8f ? (1.f / dir_normalized[1]) : 0.f,
        std::abs(dir_normalized[2]) > 1e-8f ? (1.f / dir_normalized[2]) : 0.f,
    };

    std::vector<std::pair<float, int>> hit_pairs;
    hit_pairs.reserve(static_cast<size_t>(max_candidates) * 2);

    queryRayCandidates(root_.get(), origin, dir_normalized, inv_dir,
                       radius * radius, t_max, hit_pairs);

    if (hit_pairs.empty())
        return {};

    std::sort(hit_pairs.begin(), hit_pairs.end(),
              [](const auto& a, const auto& b)
              {
                  if (a.first == b.first)
                      return a.second < b.second;
                  return a.first < b.first;
              });

    std::vector<int> result;
    result.reserve(static_cast<size_t>(max_candidates));
    int last_idx = -1;
    for (const auto& [t, idx] : hit_pairs)
    {
        (void)t;
        if (idx == last_idx)
            continue;
        result.push_back(idx);
        last_idx = idx;
        if (static_cast<int>(result.size()) >= max_candidates)
            break;
    }

    return result;
}

void Octree::queryRadius(const OctreeNode* node, const float pos[3],
                         float sq_radius, std::vector<int>& result) const
{
    // Prune nodes whose AABB is completely outside the sphere
    if (node->bounds.sqDistanceTo(pos) > sq_radius)
        return;

    if (node->is_leaf)
    {
        for (int idx : node->indices)
        {
            if (sqDist3(vertexPtr(idx), pos) <= sq_radius)
                result.push_back(idx);
        }
        return;
    }

    for (int o = 0; o < 8; ++o)
    {
        if (node->children[o])
            queryRadius(node->children[o].get(), pos, sq_radius, result);
    }
}

bool Octree::intersectRayAABB(const Bounds3& bounds,
                              const float origin[3],
                              const float dir_normalized[3],
                              const float inv_dir[3],
                              float t_max)
{
    float t_min = 0.f;
    float t_hit_max = t_max;

    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::abs(dir_normalized[axis]) < 1e-8f)
        {
            if (origin[axis] < bounds.min[axis] || origin[axis] > bounds.max[axis])
                return false;
            continue;
        }

        float t1 = (bounds.min[axis] - origin[axis]) * inv_dir[axis];
        float t2 = (bounds.max[axis] - origin[axis]) * inv_dir[axis];
        if (t1 > t2) std::swap(t1, t2);

        t_min = std::max(t_min, t1);
        t_hit_max = std::min(t_hit_max, t2);
        if (t_min > t_hit_max)
            return false;
    }

    return t_hit_max >= 0.f;
}

void Octree::queryRayCandidates(const OctreeNode* node,
                                const float origin[3],
                                const float dir_normalized[3],
                                const float inv_dir[3],
                                float sq_radius,
                                float t_max,
                                std::vector<std::pair<float, int>>& result) const
{
    if (!node)
        return;

    if (!intersectRayAABB(node->bounds, origin, dir_normalized, inv_dir, t_max))
        return;

    if (node->is_leaf)
    {
        for (int idx : node->indices)
        {
            const float* p = vertexPtr(idx);
            const float vx = p[0] - origin[0];
            const float vy = p[1] - origin[1];
            const float vz = p[2] - origin[2];
            const float t = vx * dir_normalized[0] + vy * dir_normalized[1] + vz * dir_normalized[2];

            if (t < 0.f || t > t_max)
                continue;

            const float cx = origin[0] + dir_normalized[0] * t;
            const float cy = origin[1] + dir_normalized[1] * t;
            const float cz = origin[2] + dir_normalized[2] * t;
            const float dx = p[0] - cx;
            const float dy = p[1] - cy;
            const float dz = p[2] - cz;
            const float sq_perp = dx * dx + dy * dy + dz * dz;

            if (sq_perp <= sq_radius)
                result.push_back({t, idx});
        }
        return;
    }

    for (int o = 0; o < 8; ++o)
    {
        if (node->children[o])
            queryRayCandidates(node->children[o].get(), origin, dir_normalized,
                               inv_dir, sq_radius, t_max, result);
    }
}

} // namespace Data

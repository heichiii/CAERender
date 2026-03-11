#include "Octree.h"

#include <algorithm>
#include <cassert>
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
    // Leaf condition: too deep or too few points
    if (depth >= max_depth_ || static_cast<int>(indices.size()) <= min_leaf_)
    {
        node->indices = std::move(indices);
        node->is_leaf = true;
        return;
    }

    node->is_leaf = false;

    float center[3];
    node->bounds.center(center);

    // Partition points into 8 octants
    // Bit layout: bit0=x, bit1=y, bit2=z (0=lower, 1=upper)
    std::vector<int> groups[8];
    for (int idx : indices)
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

} // namespace Data

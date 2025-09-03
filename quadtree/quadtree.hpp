#include "quadtree.h"
#include <cmath>
#include <limits>

template <typename T>
QuadTree<T>::QuadTree(const BoundingBox &bounds, size_t maxPoints, size_t maxDepth)
    : maxPoints(maxPoints), maxDepth(maxDepth)
{
    root = std::make_unique<QuadTreeNode<T>>(bounds);
}

template <typename T>
void QuadTree<T>::insert(double x, double y, const T &data)
{
    if (!root->bounds.contains(x, y))
    {
        return; // Point is outside the quadtree bounds
    }

    insertRecursive(root.get(), Point<T>(x, y, data), 0);
}

template <typename T>
void QuadTree<T>::insertRecursive(QuadTreeNode<T> *node, const Point<T> &point, size_t depth)
{
    if (node->isLeaf)
    {
        if (node->points.size() < maxPoints || depth >= maxDepth)
        {
            node->points.push_back(point);
        }
        else
        {
            subdivide(node);
            insertIntoChildren(node, point);
        }
    }
    else
    {
        insertIntoChildren(node, point);
    }
}

template <typename T>
void QuadTree<T>::subdivide(QuadTreeNode<T> *node)
{
    double halfWidth = node->bounds.width / 2.0;
    double halfHeight = node->bounds.height / 2.0;

    // Create four child nodes
    node->children[0] = std::make_unique<QuadTreeNode<T>>(
        BoundingBox(node->bounds.x, node->bounds.y, halfWidth, halfHeight));
    node->children[1] = std::make_unique<QuadTreeNode<T>>(
        BoundingBox(node->bounds.x + halfWidth, node->bounds.y, halfWidth, halfHeight));
    node->children[2] = std::make_unique<QuadTreeNode<T>>(
        BoundingBox(node->bounds.x, node->bounds.y + halfHeight, halfWidth, halfHeight));
    node->children[3] = std::make_unique<QuadTreeNode<T>>(
        BoundingBox(node->bounds.x + halfWidth, node->bounds.y + halfHeight, halfWidth, halfHeight));

    node->isLeaf = false;

    // Redistribute existing points to children
    for (const auto &existingPoint : node->points)
    {
        insertIntoChildren(node, existingPoint);
    }
    node->points.clear();
}

template <typename T>
void QuadTree<T>::insertIntoChildren(QuadTreeNode<T> *node, const Point<T> &point)
{
    double midX = node->bounds.x + node->bounds.width / 2.0;
    double midY = node->bounds.y + node->bounds.height / 2.0;

    // Determine which child to insert into
    int childIndex = 0;
    if (point.x >= midX)
        childIndex |= 1;
    if (point.y >= midY)
        childIndex |= 2;

    insertRecursive(node->children[childIndex].get(), point, 0);
}

template <typename T>
std::vector<Point<T>> QuadTree<T>::queryRange(const BoundingBox &range) const
{
    std::vector<Point<T>> result;
    queryRangeRecursive(root.get(), range, result);
    return result;
}

template <typename T>
void QuadTree<T>::queryRangeRecursive(QuadTreeNode<T> *node, const BoundingBox &range,
                                      std::vector<Point<T>> &result) const
{
    if (!node->bounds.intersects(range))
    {
        return; // No intersection, skip this node
    }

    // Add points from this node that are within the range
    for (const auto &point : node->points)
    {
        if (range.contains(point.x, point.y))
        {
            result.push_back(point);
        }
    }

    // Recursively search children
    if (!node->isLeaf)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (node->children[i])
            {
                queryRangeRecursive(node->children[i].get(), range, result);
            }
        }
    }
}

template <typename T>
bool QuadTree<T>::findNearest(double x, double y, Point<T> &nearest) const
{
    double minDist = std::numeric_limits<double>::max();
    return findNearestRecursive(root.get(), x, y, nearest, minDist);
}

template <typename T>
bool QuadTree<T>::findNearestRecursive(QuadTreeNode<T> *node, double x, double y,
                                       Point<T> &nearest, double &minDist) const
{
    if (!node)
        return false;

    bool found = false;

    // Check points in this node
    for (const auto &point : node->points)
    {
        double dist = distance(x, y, point.x, point.y);
        if (dist < minDist)
        {
            minDist = dist;
            nearest = point;
            found = true;
        }
    }

    // Recursively search children
    if (!node->isLeaf)
    {
        // Determine which child is closest to the query point
        double midX = node->bounds.x + node->bounds.width / 2.0;
        double midY = node->bounds.y + node->bounds.height / 2.0;

        int childIndex = 0;
        if (x >= midX)
            childIndex |= 1;
        if (y >= midY)
            childIndex |= 2;

        // Search the closest child first
        if (node->children[childIndex])
        {
            found |= findNearestRecursive(node->children[childIndex].get(), x, y, nearest, minDist);
        }

        // Search other children if they might contain closer points
        for (int i = 0; i < 4; ++i)
        {
            if (i != childIndex && node->children[i])
            {
                // Check if this child's bounds could contain a closer point
                double childDist = distanceToBounds(x, y, node->children[i]->bounds);
                if (childDist < minDist)
                {
                    found |= findNearestRecursive(node->children[i].get(), x, y, nearest, minDist);
                }
            }
        }
    }

    return found;
}

template <typename T>
double QuadTree<T>::distance(double x1, double y1, double x2, double y2)
{
    double dx = x2 - x1;
    double dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

template <typename T>
double QuadTree<T>::distanceToBounds(double x, double y, const BoundingBox &bounds)
{
    double dx = std::max(0.0, std::max(bounds.x - x, x - (bounds.x + bounds.width)));
    double dy = std::max(0.0, std::max(bounds.y - y, y - (bounds.y + bounds.height)));
    return std::sqrt(dx * dx + dy * dy);
}

template <typename T>
std::vector<Point<T>> QuadTree<T>::getAllPoints() const
{
    std::vector<Point<T>> result;
    getAllPointsRecursive(root.get(), result);
    return result;
}

template <typename T>
void QuadTree<T>::getAllPointsRecursive(QuadTreeNode<T> *node, std::vector<Point<T>> &result) const
{
    if (!node)
        return;

    // Add points from this node
    result.insert(result.end(), node->points.begin(), node->points.end());

    // Recursively get points from children
    if (!node->isLeaf)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (node->children[i])
            {
                getAllPointsRecursive(node->children[i].get(), result);
            }
        }
    }
}

template <typename T>
void QuadTree<T>::clear()
{
    root = std::make_unique<QuadTreeNode<T>>(root->bounds);
}

template <typename T>
size_t QuadTree<T>::size() const
{
    return countPointsRecursive(root.get());
}

template <typename T>
size_t QuadTree<T>::countPointsRecursive(QuadTreeNode<T> *node) const
{
    if (!node)
        return 0;

    size_t count = node->points.size();

    if (!node->isLeaf)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (node->children[i])
            {
                count += countPointsRecursive(node->children[i].get());
            }
        }
    }

    return count;
}

template <typename T>
bool QuadTree<T>::empty() const
{
    return size() == 0;
}

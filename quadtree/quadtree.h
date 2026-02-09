#ifndef QUADTREE_H
#define QUADTREE_H

#include <vector>
#include <memory>
#include <algorithm>

// Forward declaration
template <typename T>
class QuadTree;

/**
 * Represents a 2D point with associated data
 */
template <typename T>
struct Point
{
    double x, y;
    T data;

    Point(double x, double y, const T &data) : x(x), y(y), data(data) {}
};

/**
 * Represents a 2D bounding box
 */
struct BoundingBox
{
    double x, y, width, height;

    BoundingBox(double x, double y, double width, double height)
        : x(x), y(y), width(width), height(height) {}

    /**
     * Check if a point is within this bounding box
     */
    bool contains(double px, double py) const
    {
        return px >= x && px <= x + width && py >= y && py <= y + height;
    }

    /**
     * Check if this bounding box intersects with another
     */
    bool intersects(const BoundingBox &other) const
    {
        return !(x + width < other.x || other.x + other.width < x ||
                 y + height < other.y || other.y + other.height < y);
    }
};

/**
 * QuadTree node structure
 */
template <typename T>
struct QuadTreeNode
{
    BoundingBox bounds;
    std::vector<Point<T>> points;
    std::unique_ptr<QuadTreeNode<T>> children[4];
    bool isLeaf;

    QuadTreeNode(const BoundingBox &bounds)
        : bounds(bounds), isLeaf(true) {}
};

/**
 * QuadTree implementation for efficient 2D spatial queries
 *
 * A quadtree recursively subdivides 2D space into four quadrants
 * when the number of points in a node exceeds a threshold.
 */
template <typename T>
class QuadTree
{
private:
    std::unique_ptr<QuadTreeNode<T>> root;
    size_t maxPoints;
    size_t maxDepth;

    /**
     * Subdivide a node into four children
     */
    void subdivide(QuadTreeNode<T> *node);

    /**
     * Insert a point recursively
     */
    void insertRecursive(QuadTreeNode<T> *node, const Point<T> &point, size_t depth);

    /**
     * Insert a point into the appropriate child node
     */
    void insertIntoChildren(QuadTreeNode<T> *node, const Point<T> &point);

    /**
     * Query points within a bounding box recursively
     */
    void queryRangeRecursive(QuadTreeNode<T> *node, const BoundingBox &range,
                             std::vector<Point<T>> &result) const;

    /**
     * Find the nearest point to a given location recursively
     */
    bool findNearestRecursive(QuadTreeNode<T> *node, double x, double y,
                              Point<T> &nearest, double &minDist) const;

    /**
     * Calculate distance between two points
     */
    static double distance(double x1, double y1, double x2, double y2);

    /**
     * Calculate distance from a point to a bounding box
     */
    static double distanceToBounds(double x, double y, const BoundingBox &bounds);

    /**
     * Get all points recursively
     */
    void getAllPointsRecursive(QuadTreeNode<T> *node, std::vector<Point<T>> &result) const;

    /**
     * Count points recursively
     */
    size_t countPointsRecursive(QuadTreeNode<T> *node) const;

public:
    /**
     * Constructor
     * @param bounds The bounding box for the entire quadtree
     * @param maxPoints Maximum points per node before subdivision (default: 10)
     * @param maxDepth Maximum depth of the tree (default: 10)
     */
    QuadTree(const BoundingBox &bounds, size_t maxPoints = 10, size_t maxDepth = 10);

    /**
     * Insert a point into the quadtree
     * @param x X coordinate
     * @param y Y coordinate
     * @param data Associated data
     */
    void insert(double x, double y, const T &data);

    /**
     * Query all points within a given bounding box
     * @param range The bounding box to search within
     * @return Vector of points within the range
     */
    std::vector<Point<T>> queryRange(const BoundingBox &range) const;

    /**
     * Find the nearest point to a given location
     * @param x X coordinate
     * @param y Y coordinate
     * @param nearest Output parameter for the nearest point
     * @return True if a point was found, false otherwise
     */
    bool findNearest(double x, double y, Point<T> &nearest) const;

    /**
     * Get all points in the quadtree
     * @return Vector of all points
     */
    std::vector<Point<T>> getAllPoints() const;

    /**
     * Clear all points from the quadtree
     */
    void clear();

    /**
     * Get the total number of points in the quadtree
     * @return Number of points
     */
    size_t size() const;

    /**
     * Check if the quadtree is empty
     * @return True if empty, false otherwise
     */
    bool empty() const;
};

#include "quadtree.hpp"

#endif // QUADTREE_H

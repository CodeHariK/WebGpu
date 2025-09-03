#include "quadtree.h"
#include <iostream>
#include <string>
#include <random>
#include <chrono>

// Example data structure
struct City
{
    std::string name;
    int population;

    City(const std::string &name, int population) : name(name), population(population) {}
};

// Print a point
template <typename T>
void printPoint(const Point<T> &point)
{
    std::cout << "(" << point.x << ", " << point.y << ") - ";
    if constexpr (std::is_same_v<T, City>)
    {
        std::cout << "City: " << point.data.name << " (pop: " << point.data.population << ")";
    }
    else
    {
        std::cout << "Data: " << point.data;
    }
    std::cout << std::endl;
}

// Print all points in a vector
template <typename T>
void printPoints(const std::vector<Point<T>> &points)
{
    for (const auto &point : points)
    {
        printPoint(point);
    }
}

int main()
{
    std::cout << "=== QuadTree Implementation in C++ ===" << std::endl
              << std::endl;

    // Example 1: Simple integer data
    std::cout << "Example 1: Integer data" << std::endl;
    std::cout << "------------------------" << std::endl;

    // Create a quadtree covering the area from (0,0) to (100,100)
    QuadTree<int> intTree(BoundingBox(0, 0, 100, 100));

    // Insert some points
    intTree.insert(10, 20, 100);
    intTree.insert(30, 40, 200);
    intTree.insert(50, 60, 300);
    intTree.insert(70, 80, 400);
    intTree.insert(25, 35, 250);

    std::cout << "Total points: " << intTree.size() << std::endl;

    // Query a range
    BoundingBox queryRange(0, 0, 40, 50);
    auto pointsInRange = intTree.queryRange(queryRange);
    std::cout << "Points in range (0,0) to (40,50): " << pointsInRange.size() << std::endl;
    printPoints(pointsInRange);

    // Find nearest point
    Point<int> nearest;
    if (intTree.findNearest(15, 25, nearest))
    {
        std::cout << "Nearest point to (15,25): ";
        printPoint(nearest);
    }

    std::cout << std::endl;

    // Example 2: Custom data structure (City)
    std::cout << "Example 2: City data" << std::endl;
    std::cout << "---------------------" << std::endl;

    QuadTree<City> cityTree(BoundingBox(-100, -100, 200, 200));

    // Insert cities
    cityTree.insert(-50, -30, City("New York", 8336817));
    cityTree.insert(20, 40, City("London", 8982000));
    cityTree.insert(80, -20, City("Tokyo", 13929286));
    cityTree.insert(-20, 60, City("Paris", 2161000));
    cityTree.insert(0, 0, City("Origin City", 1000));

    std::cout << "Total cities: " << cityTree.size() << std::endl;

    // Query cities in a specific region
    BoundingBox europeRegion(-30, 30, 60, 40);
    auto europeanCities = cityTree.queryRange(europeRegion);
    std::cout << "Cities in Europe region: " << europeanCities.size() << std::endl;
    printPoints(europeanCities);

    // Find nearest city to a location
    Point<City> nearestCity;
    if (cityTree.findNearest(10, 10, nearestCity))
    {
        std::cout << "Nearest city to (10,10): ";
        printPoint(nearestCity);
    }

    std::cout << std::endl;

    // Example 3: Performance test with many points
    std::cout << "Example 3: Performance test" << std::endl;
    std::cout << "----------------------------" << std::endl;

    QuadTree<int> perfTree(BoundingBox(0, 0, 1000, 1000), 8, 15);

    // Generate random points
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1000.0);

    const int numPoints = 10000;
    std::cout << "Inserting " << numPoints << " random points..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numPoints; ++i)
    {
        double x = dis(gen);
        double y = dis(gen);
        perfTree.insert(x, y, i);
    }

    auto insertEnd = std::chrono::high_resolution_clock::now();
    auto insertTime = std::chrono::duration_cast<std::chrono::milliseconds>(insertEnd - start);

    std::cout << "Insertion time: " << insertTime.count() << " ms" << std::endl;
    std::cout << "Final tree size: " << perfTree.size() << std::endl;

    // Test range query performance
    start = std::chrono::high_resolution_clock::now();

    BoundingBox testRange(100, 100, 200, 200);
    auto rangeResults = perfTree.queryRange(testRange);

    auto queryEnd = std::chrono::high_resolution_clock::now();
    auto queryTime = std::chrono::duration_cast<std::chrono::microseconds>(queryEnd - start);

    std::cout << "Range query time: " << queryTime.count() << " μs" << std::endl;
    std::cout << "Points in range: " << rangeResults.size() << std::endl;

    // Test nearest neighbor performance
    start = std::chrono::high_resolution_clock::now();

    Point<int> testNearest;
    perfTree.findNearest(500, 500, testNearest);

    auto nearestEnd = std::chrono::high_resolution_clock::now();
    auto nearestTime = std::chrono::duration_cast<std::chrono::microseconds>(nearestEnd - start);

    std::cout << "Nearest neighbor time: " << nearestTime.count() << " μs" << std::endl;

    std::cout << std::endl;

    // Example 4: Tree operations
    std::cout << "Example 4: Tree operations" << std::endl;
    std::cout << "---------------------------" << std::endl;

    QuadTree<int> opTree(BoundingBox(0, 0, 100, 100));

    std::cout << "Empty tree size: " << opTree.size() << std::endl;
    std::cout << "Is empty: " << (opTree.empty() ? "Yes" : "No") << std::endl;

    opTree.insert(10, 10, 1);
    opTree.insert(20, 20, 2);
    opTree.insert(30, 30, 3);

    std::cout << "After insertion - Size: " << opTree.size() << std::endl;
    std::cout << "Is empty: " << (opTree.empty() ? "Yes" : "No") << std::endl;

    // Get all points
    auto allPoints = opTree.getAllPoints();
    std::cout << "All points in tree:" << std::endl;
    printPoints(allPoints);

    // Clear the tree
    opTree.clear();
    std::cout << "After clearing - Size: " << opTree.size() << std::endl;
    std::cout << "Is empty: " << (opTree.empty() ? "Yes" : "No") << std::endl;

    std::cout << std::endl;
    std::cout << "=== QuadTree Demo Complete ===" << std::endl;

    return 0;
}

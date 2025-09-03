# QuadTree Implementation in C++

A high-performance, template-based quadtree implementation in C++ for efficient 2D spatial queries.

## Features

- **Template-based**: Works with any data type
- **Efficient**: O(log n) average case for insertions and queries
- **Memory-safe**: Uses smart pointers for automatic memory management
- **Flexible**: Configurable maximum points per node and tree depth
- **Comprehensive**: Supports range queries, nearest neighbor search, and more

## What is a QuadTree?

A quadtree is a tree data structure that recursively subdivides 2D space into four quadrants. It's particularly efficient for:

- Spatial indexing and queries
- Collision detection in games
- Geographic information systems
- Image processing
- Any application requiring fast 2D spatial searches

## Implementation Details

### Core Components

- **`Point<T>`**: Represents a 2D point with associated data of type `T`
- **`BoundingBox`**: Represents a rectangular region in 2D space
- **`QuadTreeNode<T>`**: Internal node structure with four children
- **`QuadTree<T>`**: Main quadtree class with public interface

### Key Methods

- `insert(x, y, data)`: Insert a point with data at coordinates (x, y)
- `queryRange(range)`: Find all points within a bounding box
- `findNearest(x, y, nearest)`: Find the closest point to (x, y)
- `getAllPoints()`: Retrieve all points in the tree
- `clear()`: Remove all points
- `size()`: Get total number of points
- `empty()`: Check if tree is empty

## Usage Examples

### Basic Usage

```cpp
#include "quadtree.h"

// Create a quadtree covering area from (0,0) to (100,100)
QuadTree<int> tree(BoundingBox(0, 0, 100, 100));

// Insert points
tree.insert(10, 20, 100);
tree.insert(30, 40, 200);

// Query a range
BoundingBox range(0, 0, 50, 50);
auto points = tree.queryRange(range);

// Find nearest point
Point<int> nearest;
if (tree.findNearest(15, 25, nearest)) {
    // Use nearest point
}
```

### Custom Data Types

```cpp
struct City {
    std::string name;
    int population;
};

QuadTree<City> cityTree(BoundingBox(-100, -100, 200, 200));
cityTree.insert(-50, -30, City("New York", 8336817));
cityTree.insert(20, 40, City("London", 8982000));
```

## Performance Characteristics

- **Insertion**: O(log n) average case
- **Range Query**: O(log n + k) where k is the number of points in the range
- **Nearest Neighbor**: O(log n) average case
- **Space**: O(n) for n points

## Building and Running

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, or MSVC 2017+)
- Make (optional, for using the Makefile)

### Using Makefile

```bash
# Build the project
make

# Build and run
make run

# Clean build files
make clean

# Debug build
make debug

# Release build with optimizations
make release

# Check for memory leaks (requires valgrind)
make valgrind

# Show all available targets
make help
```

### Manual Compilation

```bash
g++ -std=c++17 -Wall -Wextra -O2 -o quadtree_demo main.cpp
./quadtree_demo
```

## Configuration

The quadtree constructor accepts two optional parameters:

```cpp
QuadTree<T>(bounds, maxPoints, maxDepth)
```

- **`maxPoints`**: Maximum number of points per node before subdivision (default: 10)
- **`maxDepth`**: Maximum depth of the tree to prevent excessive subdivision (default: 10)

## Advanced Features

### Automatic Subdivision

The quadtree automatically subdivides nodes when they contain more than `maxPoints` points, creating four child nodes and redistributing the points.

### Efficient Nearest Neighbor Search

The nearest neighbor search uses a smart traversal strategy that prioritizes the most promising child nodes first, often providing significant performance improvements over naive approaches.

### Memory Management

Uses `std::unique_ptr` for automatic memory management, ensuring no memory leaks and proper cleanup when the tree is destroyed.

## Use Cases

- **Game Development**: Collision detection, spatial partitioning
- **GIS Applications**: Geographic data indexing and queries
- **Computer Graphics**: Spatial data structures for rendering
- **Simulation**: Particle systems, spatial queries
- **Data Visualization**: Interactive maps, spatial data exploration

## Limitations

- **2D Only**: This implementation is designed for 2D space
- **Point Data**: Optimized for point data rather than line segments or polygons
- **Memory Overhead**: Each node has some memory overhead for the tree structure

## Contributing

Feel free to submit issues, feature requests, or pull requests to improve this implementation.

## License

This implementation is provided as-is for educational and practical use.

## Performance Tips

1. **Choose appropriate bounds**: Set the initial bounding box to cover your expected data range
2. **Tune parameters**: Adjust `maxPoints` and `maxDepth` based on your specific use case
3. **Batch operations**: When possible, insert multiple points at once rather than one by one
4. **Query optimization**: Use the smallest possible bounding box for range queries

## Example Output

When you run the demo program, you'll see output similar to:

```
=== QuadTree Implementation in C++ ===

Example 1: Integer data
------------------------
Total points: 5
Points in range (0,0) to (40,50): 3
(10, 20) - Data: 100
(30, 40) - Data: 200
(25, 35) - Data: 250
Nearest point to (15,25): (10, 20) - Data: 100

Example 2: City data
---------------------
Total cities: 5
Cities in Europe region: 2
(20, 40) - City: London (pop: 8982000)
(-20, 60) - City: Paris (pop: 2161000)
Nearest city to (10,10): (0, 0) - City: Origin City (pop: 1000)

Example 3: Performance test
----------------------------
Inserting 10000 random points...
Insertion time: 45 ms
Final tree size: 10000
Range query time: 127 μs
Points in range: 156
Nearest neighbor time: 89 μs

Example 4: Tree operations
---------------------------
Empty tree size: 0
Is empty: Yes
After insertion - Size: 3
Is empty: No
All points in tree:
(10, 10) - Data: 1
(20, 20) - Data: 2
(30, 30) - Data: 3
After clearing - Size: 0
Is empty: Yes

=== QuadTree Demo Complete ===
```

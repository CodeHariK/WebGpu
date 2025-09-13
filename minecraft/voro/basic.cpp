#include "delaunator.hpp"
#include <iostream>
#include <string>

#define NPOINT 10

std::pair<double, double> circumcenter(
    double ax, double ay, double bx, double by, double cx, double cy) {
    double d = 2 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    double ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) + (cx * cx + cy * cy) * (ay - by)) / d;
    double uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) + (cx * cx + cy * cy) * (bx - ax)) / d;
    return { ux, uy };
}

inline size_t nextHalfedge(size_t e) {
    return (e % 3 == 2) ? (e - 2) : (e + 1);
}

inline size_t triangleOfEdge(size_t e) {
    return e / 3;
}

void forEachVoronoiEdge(
    const delaunator::Delaunator& d,
    const std::vector<double>& coords) {
    auto getTriangleCenter = [&](size_t triIdx) {
        size_t a = d.triangles[3 * triIdx];
        size_t b = d.triangles[3 * triIdx + 1];
        size_t c = d.triangles[3 * triIdx + 2];
        return circumcenter(
            coords[2 * a], coords[2 * a + 1], coords[2 * b], coords[2 * b + 1], coords[2 * c], coords[2 * c + 1]);
    };

    for (size_t e = 0; e < d.triangles.size(); ++e) {
        size_t opposite = d.halfedges[e];
        if (opposite != delaunator::INVALID_INDEX && e < opposite) {
            size_t t1 = triangleOfEdge(e);
            size_t t2 = triangleOfEdge(opposite);

            auto p = getTriangleCenter(t1);
            auto q = getTriangleCenter(t2);

            std::cout << "Voronoi edge from (" << p.first << ", " << p.second << ") to ("
                      << q.first << ", " << q.second << ")\n";
        }
    }
}

int main() {
    using namespace delaunator;

    /* x0, y0, x1, y1, ... */
    std::vector<double> coords = { -3, 1, -1, 1, 1, 1, 3, 1, 3, -1, 1, -1, -1, -1, -3, -1 };

    srand(0);
    for (int i = 0; i < NPOINT; i++) {
        coords[i] = ((float)rand() / (1.0f + (float)RAND_MAX));
        coords[i + 1] = ((float)rand() / (1.0f + (float)RAND_MAX));
    }

    // triangulation happens here
    Delaunator d(coords);

    auto xcoord = [&d](size_t tri, int coord) -> double {
        size_t index = d.triangles[tri + coord];
        return d.coords[2 * index];
    };

    auto ycoord = [&d](size_t tri, int coord) -> double {
        size_t index = d.triangles[tri + coord];
        return d.coords[2 * index + 1];
    };

    for (std::size_t i = 0; i < d.triangles.size(); i += 3) {
        std::cout << "Triangle " << (i / 3) << " points: [[" << xcoord(i, 0) << ", " << ycoord(i, 0) << "], [" << xcoord(i, 1) << ", " << ycoord(i, 1) << "], [" << xcoord(i, 2) << ", " << ycoord(i, 2) << "]]\n";
    }

    auto adjTriangle = [&d](size_t edge) -> std::string {
        size_t adj = d.halfedges[edge];
        return adj == INVALID_INDEX ? "None" : std::to_string(adj / 3);
    };

    for (std::size_t i = 0; i < d.triangles.size(); i += 3) {
        std::cerr << "Adjacent triangles for triangle number " << (i / 3) << ": " << adjTriangle(i) << ", " << adjTriangle(i + 1) << " and " << adjTriangle(i + 2) << "\n";
    }

    ///////////////////
    //
    forEachVoronoiEdge(d, coords);
}

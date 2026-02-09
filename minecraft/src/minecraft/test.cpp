#include "minecraft.h"

#include "godot_cpp/variant/string.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstddef>
#include <cstdlib>
#include <string>
#include <vector>

#include "../include/min_heap.hpp"

#include "../include/delaunator.hpp"

#define JC_VORONOI_IMPLEMENTATION
#include "../include/jc_voronoi.h"

void MinecraftNode::minHeapTest() {
	MinHeap<std::string> pq;

	pq.insert("apple", 5);
	pq.insert("banana", 2);
	pq.insert("cherry", 8);

	godot::UtilityFunctions::print(String("Peek: ") + pq.peek().value().c_str()); // banana

	if (!pq.updatePriority("apple", 1)) {
		godot::UtilityFunctions::print("Item not found in priority queue");
	}

	for (const auto &node : pq.getElements()) {
		godot::UtilityFunctions::print(String("item=") + node.item.c_str() + " priority=" + String::num_real(node.priority));
	}

	godot::UtilityFunctions::print(String("ExtractMin: ") + pq.extractMin().value().c_str()); // apple
	godot::UtilityFunctions::print(String("ExtractMin: ") + pq.extractMin().value().c_str()); // banana
	godot::UtilityFunctions::print(String("ExtractMin: ") + pq.extractMin().value().c_str()); // cherry
}

#define NPOINT 20

Vector2 getTriangleCenter(const std::vector<size_t> &triangles, const std::vector<float> &coords, size_t triIdx) {
	size_t a = triangles[3 * triIdx];
	size_t b = triangles[3 * triIdx + 1];
	size_t c = triangles[3 * triIdx + 2];

	float ax = coords[2 * a];
	float ay = coords[2 * a + 1];
	float bx = coords[2 * b];
	float by = coords[2 * b + 1];
	float cx = coords[2 * c];
	float cy = coords[2 * c + 1];

	float denom = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
	float ux = ((ax * ax + ay * ay) * (by - cy) +
					   (bx * bx + by * by) * (cy - ay) +
					   (cx * cx + cy * cy) * (ay - by)) /
			denom;
	float uy = ((ax * ax + ay * ay) * (cx - bx) +
					   (bx * bx + by * by) * (ax - cx) +
					   (cx * cx + cy * cy) * (bx - ax)) /
			denom;

	return Vector2(ux, uy);
}

godot::Dictionary MinecraftNode::DelaunatorTest() {
	using namespace delaunator;

	// prepare 2*NPOINT coordinates: x0,y0,x1,y1,...
	std::vector<float> coords;
	coords.resize(2 * NPOINT); // <-- important: allocate space

	srand(0);
	for (int i = 0; i < NPOINT; i++) {
		coords[2 * i] = ((float)rand() / (1.0f + (float)RAND_MAX));
		coords[2 * i + 1] = ((float)rand() / (1.0f + (float)RAND_MAX));
	}

	// triangulation happens here
	Delaunator delTris(coords);

	Array delaunay_edges;
	Array voronoi_verts;
	Array voronoi_edges;

	// Step 1: Compute triangle centers (circumcenters)
	std::vector<Vector2> centers;
	size_t triangleCount = delTris.triangles.size() / 3;
	centers.reserve(triangleCount);

	for (size_t i = 0; i < triangleCount; ++i) {
		Vector2 center = getTriangleCenter(delTris.triangles, coords, i);
		centers.push_back(center);
		voronoi_verts.append(center);
	}

	// Step 2: Generate Delaunay edges (as before)
	for (size_t i = 0; i < delTris.triangles.size(); i += 3) {
		size_t i0 = delTris.triangles[i];
		size_t i1 = delTris.triangles[i + 1];
		size_t i2 = delTris.triangles[i + 2];

		Vector2 p0(coords[2 * i0], coords[2 * i0 + 1]);
		Vector2 p1(coords[2 * i1], coords[2 * i1 + 1]);
		Vector2 p2(coords[2 * i2], coords[2 * i2 + 1]);

		delaunay_edges.append(p0);
		delaunay_edges.append(p1);
		delaunay_edges.append(p1);
		delaunay_edges.append(p2);
		delaunay_edges.append(p2);
		delaunay_edges.append(p0);
	}

	// Step 3: Loop over halfedges and construct Voronoi edges
	for (size_t e = 0; e < delTris.halfedges.size(); ++e) {
		size_t opposite = delTris.halfedges[e];

		// Only process each pair once
		if (opposite != delaunator::INVALID_INDEX && e < opposite) {
			size_t t1 = e / 3;
			size_t t2 = opposite / 3;

			const Vector2 &p = centers[t1];
			const Vector2 &q = centers[t2];

			voronoi_edges.append(p);
			voronoi_edges.append(q);
		}
	}

	// Step 4: Return all in a dictionary
	godot::Dictionary result;
	result["delaunay_edges"] = delaunay_edges;
	result["delaunay_voronoi_verts"] = voronoi_verts;
	result["delaunay_voronoi_edges"] = voronoi_edges;
	return result;
}

godot::Array MinecraftNode::VoronoiTest() {
	jcv_rect bounding_box = { { 0.0f, 0.0f }, { 1.0f, 1.0f } };
	jcv_diagram diagram = {};

	jcv_point coords[NPOINT];
	srand(0);
	for (int i = 0; i < NPOINT; i++) {
		coords[i].x = ((float)rand() / (1.0f + (float)RAND_MAX));
		coords[i].y = ((float)rand() / (1.0f + (float)RAND_MAX));
	}

	jcv_diagram_generate(NPOINT, (const jcv_point *)coords, &bounding_box, 0, &diagram);

	Array result;

	const jcv_site *sites = jcv_diagram_get_sites(&diagram);
	for (int i = 0; i < diagram.numsites; i++) {
		const jcv_site *site = &sites[i];
		const jcv_graphedge *edge = site->edges;

		while (edge) {
			// Each edge has 'pos[0]' and 'pos[1]' — the start and end points
			Vector2 p0(edge->pos[0].x, edge->pos[0].y);
			Vector2 p1(edge->pos[1].x, edge->pos[1].y);

			result.append(p0);
			result.append(p1);

			edge = edge->next;
		}
	}

	jcv_diagram_free(&diagram);

	return result;
}

#include "minecraft.h"

#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/ref.hpp"

#include "godot_cpp/core/memory.hpp"
#include "godot_cpp/variant/string.hpp"
#include <cstddef>
#include <cstdlib>
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

#include <godot_cpp/classes/file_access.hpp>

#include "../include/celebi_parse.hpp"
#include "../include/json.hpp"
#include "../include/min_heap.hpp"

#include "../include/delaunator.hpp"

#define JC_VORONOI_IMPLEMENTATION
#include "../include/jc_voronoi.h"

#include <string>

using njson = nlohmann::json;

void MinecraftNode::blendTest() {
	// Load the entire .blend file as a PackedScene and print mesh/object/material names
	Ref<Resource> blend_resource = ResourceLoader::get_singleton()->load("res://assets/Voxel.blend");
	if (blend_resource.is_valid() && blend_resource->is_class("PackedScene")) {
		Ref<PackedScene> packed_scene = blend_resource;
		Node *scene_root = packed_scene->instantiate();
		if (scene_root) {
			// Recursive lambda for traversing children
			int mesh_instance_counter = 0;
			std::function<void(Node *)> traverse = [&](Node *node) {
				if (Object::cast_to<Node3D>(node)) {
					UtilityFunctions::print(String("Node3D: ") + node->get_name());
				}
				MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(node);
				if (mesh_instance) {
					Ref<Mesh> m = mesh_instance->get_mesh();
					if (m.is_valid()) {
						UtilityFunctions::print(String("  Mesh: ") + m->get_name());
						int surf_count = m->get_surface_count();
						for (int i = 0; i < surf_count; ++i) {
							Ref<Material> mat = m->surface_get_material(i);
							if (mat.is_valid()) {
								UtilityFunctions::print(String("    Material: ") + mat->get_name());
							}
						}
						// Instantiate a new MeshInstance3D for each mesh and add it as child to this HelloNode
						MeshInstance3D *new_mesh_instance = memnew(MeshInstance3D);
						new_mesh_instance->set_mesh(m);
						new_mesh_instance->set_name(String("ImportedMeshInstance") + String::num(mesh_instance_counter++));
						new_mesh_instance->set_transform(mesh_instance->get_transform());
						this->add_child(new_mesh_instance);
					}
				}
				int child_count = node->get_child_count();
				for (int i = 0; i < child_count; ++i) {
					Node *child = node->get_child(i);
					traverse(child);
				}
			};
			traverse(scene_root);
			scene_root->queue_free();
		}
	}
}

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

void MinecraftNode::jsonTest() {
	std::string planetJsonStr = R"(
		{
			"planets": ["Mercury", "Venus", "Earth", "Mars",
						"Jupiter", "Uranus", "Neptune"]
		}
	)";

	// parse without exceptions
	njson j = njson::parse(planetJsonStr, nullptr, false);

	if (j.is_discarded()) {
		godot::UtilityFunctions::print("Failed to parse JSON!");
	} else {
		godot::UtilityFunctions::print(String("Parsed planets: ") + j.dump(2).c_str());
	}

	// ---- Load and parse library.celebi without exceptions ----
	Ref<FileAccess> f = FileAccess::open("res://data/library.json", FileAccess::READ);
	if (f.is_null()) {
		UtilityFunctions::print("Could not open library.json");
		return;
	}

	String content = f->get_as_text();
	njson libraryJson = njson::parse(std::string(content.utf8().get_data()), nullptr, false);

	if (libraryJson.is_discarded()) {
		UtilityFunctions::print("Parse error: invalid JSON in library.json");
		return;
	}

	Celebi::Data d1 = libraryJson.get<Celebi::Data>();
	UtilityFunctions::print(String("library.json item count: ") + String::num_int64(d1.items.size()));
	for (const auto &item : d1.items) {
		UtilityFunctions::print(String(item.obj.c_str()));
	}
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
	Delaunator d(coords);

	Array delaunay_edges;
	Array voronoi_verts;
	Array voronoi_edges;

	// Step 1: Compute triangle centers (circumcenters)
	std::vector<Vector2> centers;
	size_t triangleCount = d.triangles.size() / 3;
	centers.reserve(triangleCount);

	for (size_t i = 0; i < triangleCount; ++i) {
		Vector2 center = getTriangleCenter(d.triangles, coords, i);
		centers.push_back(center);
		voronoi_verts.append(center);
	}

	// Step 2: Generate Delaunay edges (as before)
	for (size_t i = 0; i < d.triangles.size(); i += 3) {
		size_t i0 = d.triangles[i];
		size_t i1 = d.triangles[i + 1];
		size_t i2 = d.triangles[i + 2];

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
	for (size_t e = 0; e < d.halfedges.size(); ++e) {
		size_t opposite = d.halfedges[e];

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

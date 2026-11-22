/**
 * @file city_kit.h
 * @brief The building kit: ten buildings of Quaternius's "Ultimate Textured Building Pack" (CC0), one
 * .glb each in KIT_PATH (made by Claude outputs/building_kit/export_glb.py), each with its own
 * colours (a palette texture) and its front facing −z.
 *
 * The city's houses get one model each, picked by their floors; a downtown block gets a row of
 * models side by side along its road, as tall as its floors ask (as near as the kit allows). The
 * models are scaled whole (never stretched) to fit their box, their fronts on the box's front (its
 * road side, local −z). All the placed copies of a model are drawn by one MultiMesh.
 */
#ifndef CITY_KIT_H
#define CITY_KIT_H

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <map>
#include <vector>

namespace godot::city {

// A box building from the export: its middle (x, z), its angle, its width (along its road) and
// depth, in metres.
struct Box {
	float x = 0.0f;
	float z = 0.0f;
	float angle = 0.0f;
	float width = 0.0f;
	float depth = 0.0f;
};

class BuildingKit {
public:
	// The models loaded; false (and nothing placed later) if the kit is missing.
	bool load();

	// One house model on p_box, for p_floors floors, picked by p_seed; a box collider into p_body.
	// False if no model fits (the caller then draws its placeholder).
	bool place_house(
			const Box &p_box,
			int p_floors,
			uint32_t p_seed,
			StaticBody3D *p_body
	);

	// A row of downtown models along p_box's front, as near p_floors tall as fits. False if none fits.
	bool place_downtown(
			const Box &p_box,
			int p_floors,
			uint32_t p_seed,
			StaticBody3D *p_body
	);

	// The MultiMeshes of everything placed, under p_parent.
	void build(Node3D *p_parent) const;

private:
	struct Model {
		String name;
		Ref<Mesh> mesh;
		Ref<StandardMaterial3D> paint; // its own material, a little brightened
		AABB bounds; // in the model's own units, front at bounds.position.z
		int floors = 1;
		bool house = false;
		bool downtown = false;
	};

	std::vector<Model> models;
	std::map<int, std::vector<Transform3D>> placed; // model → its copies' transforms

	void _put(
			const Box &p_box,
			int p_model,
			float p_scale,
			float p_across,
			StaticBody3D *p_body
	);
};

} // namespace godot::city

#endif // CITY_KIT_H

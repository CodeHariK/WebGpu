/**
 * @file city_kit.cpp
 * @brief Loading the kit's meshes (with their own materials), choosing and fitting models to the city's boxes,
 * and drawing them as MultiMeshes.
 */
#include "city_kit.h"
#include <algorithm>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

namespace godot::city {

namespace {

const char *KIT_PATH = "res://assets/city/buildings/";
constexpr float FLOOR_METRES = 5.0f; // the export's floor height
constexpr float KIT_FLOOR = 1.3f; // the kit's floor height, in its units
constexpr float BASE = FLOOR_METRES / KIT_FLOOR; // metres a kit unit, floors matching the export's
constexpr float HOUSE_GROW = 1.6f; // a house may be scaled up to this times BASE to fill its box
constexpr float DOWNTOWN_GROW = 2.0f; // and a downtown block's models to this
constexpr int CHOICES = 3; // a downtown slot picks at random among the models nearest its height

struct Entry {
	const char *name;
	int floors;
	bool house;
	bool downtown;
};

const Entry ENTRIES[] = {
	{ "1Story_GableRoof", 1, true, false },
	{ "1Story_RoundRoof", 1, true, false },
	{ "2Story_GableRoof", 2, true, false },
	{ "2Story_Balcony", 2, true, false },
	{ "2Story_Stairs", 2, true, false },
	{ "3Story_Small", 3, false, true },
	{ "3Story_Balcony", 3, false, true },
	{ "4Story", 4, false, true },
	{ "4Story_Wide_2Doors_Roof", 4, false, true },
	{ "6Story_Stack", 6, false, true },
};

constexpr float BRIGHTEN = 1.3f; // the kit's colours are a little dark for the city's sunny pastels

uint32_t mix(uint32_t p_seed) {
	p_seed ^= p_seed >> 16;
	p_seed *= 0x7feb352du;
	p_seed ^= p_seed >> 15;
	p_seed *= 0x846ca68bu;
	p_seed ^= p_seed >> 16;
	return p_seed;
}

// The first MeshInstance3D under p_node.
MeshInstance3D *find_mesh(Node *p_node) {
	if (MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(p_node)) {
		return mesh;
	}
	for (int64_t i = 0; i < p_node->get_child_count(); i++) {
		if (MeshInstance3D *mesh = find_mesh(p_node->get_child(i))) {
			return mesh;
		}
	}
	return nullptr;
}

} // namespace

bool BuildingKit::load() {
	ResourceLoader *loader = ResourceLoader::get_singleton();
	for (const Entry &e : ENTRIES) {
		const String path = String(KIT_PATH) + e.name + ".glb";
		if (!loader->exists(path)) {
			continue;
		}
		const Ref<PackedScene> scene = loader->load(path);
		if (scene.is_null()) {
			continue;
		}
		Node *root = scene->instantiate();
		const MeshInstance3D *found = find_mesh(root);
		if (found && found->get_mesh().is_valid()) {
			Model model;
			model.name = e.name;
			model.mesh = found->get_mesh();
			const Ref<StandardMaterial3D> own = model.mesh->surface_get_material(0);
			model.paint = own.is_valid() ? Ref<StandardMaterial3D>(own->duplicate()) : Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
			model.paint->set_albedo(Color(BRIGHTEN, BRIGHTEN, BRIGHTEN));
			model.paint->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST); // a palette: no blending between its cells
			model.bounds = model.mesh->get_aabb();
			model.floors = e.floors;
			model.house = e.house;
			model.downtown = e.downtown;
			models.push_back(model);
		}
		memdelete(root);
	}
	return !models.empty();
}

// Model p_model, scaled by p_scale, its front on p_box's front, its middle p_across metres along
// the box's width from the box's middle; and its collider.
void BuildingKit::_put(
		const Box &p_box,
		int p_model,
		float p_scale,
		float p_across,
		StaticBody3D *p_body
) {
	const AABB &b = models[p_model].bounds;
	const Transform3D box(Basis(Vector3(0, 1, 0), -p_box.angle), Vector3(p_box.x, 0.0f, p_box.z));
	const Vector3 offset(
			p_across - (b.position.x + b.size.x * 0.5f) * p_scale,
			-b.position.y * p_scale,
			-p_box.depth * 0.5f - b.position.z * p_scale);
	placed[p_model].push_back(box * Transform3D(Basis().scaled(Vector3(p_scale, p_scale, p_scale)), offset));
	Ref<BoxShape3D> shape;
	shape.instantiate();
	shape->set_size(b.size * p_scale);
	CollisionShape3D *collider = memnew(CollisionShape3D);
	collider->set_shape(shape);
	collider->set_transform(box * Transform3D(Basis(), Vector3(p_across, b.size.y * p_scale * 0.5f, -p_box.depth * 0.5f + b.size.z * p_scale * 0.5f)));
	p_body->add_child(collider);
}

bool BuildingKit::place_house(
		const Box &p_box,
		int p_floors,
		uint32_t p_seed,
		StaticBody3D *p_body
) {
	std::vector<int> fits;
	for (size_t m = 0; m < models.size(); m++) {
		if (models[m].house && models[m].floors == p_floors) {
			fits.push_back(int(m));
		}
	}
	if (fits.empty()) {
		return false;
	}
	const uint32_t r = mix(p_seed);
	const int model = fits[r % fits.size()];
	const AABB &b = models[model].bounds;
	const float scale = MIN(BASE * HOUSE_GROW, MIN(p_box.width / b.size.x, p_box.depth / b.size.z));
	_put(p_box, model, scale, 0.0f, p_body);
	return true;
}

bool BuildingKit::place_downtown(
		const Box &p_box,
		int p_floors,
		uint32_t p_seed,
		StaticBody3D *p_body
) {
	float deepest = 0.0f;
	for (const Model &m : models) {
		deepest = m.downtown ? MAX(deepest, m.bounds.size.z) : deepest;
	}
	if (deepest <= 0.0f) {
		return false;
	}
	const float scale = MIN(BASE * DOWNTOWN_GROW, p_box.depth / deepest);
	const float wanted = float(p_floors) * FLOOR_METRES;
	struct Slot {
		int model;
		float start;
		float width;
	};
	std::vector<Slot> row;
	float used = 0.0f;
	uint32_t r = mix(p_seed);
	while (true) { // fill along the width: each slot one of the CHOICES models nearest the height wanted that still fit
		std::vector<std::pair<float, int>> near;
		for (size_t m = 0; m < models.size(); m++) {
			const Model &model = models[m];
			if (model.downtown && used + model.bounds.size.x * scale <= p_box.width) {
				near.push_back({ Math::abs(model.bounds.size.y * scale - wanted), int(m) });
			}
		}
		if (near.empty()) {
			break;
		}
		std::sort(near.begin(), near.end());
		r = mix(r + 1);
		const int model = near[r % MIN(size_t(CHOICES), near.size())].second;
		const float width = models[model].bounds.size.x * scale;
		row.push_back({ model, used, width });
		used += width;
	}
	const float left = -p_box.width * 0.5f + (p_box.width - used) * 0.5f; // the row centred on the box
	for (const Slot &s : row) {
		_put(p_box, s.model, scale, left + s.start + s.width * 0.5f, p_body);
	}
	return !row.empty();
}

void BuildingKit::build(Node3D *p_parent) const {
	for (const auto &[model, transforms] : placed) {
		Ref<MultiMesh> multi;
		multi.instantiate();
		multi->set_transform_format(MultiMesh::TRANSFORM_3D);
		multi->set_mesh(models[model].mesh);
		multi->set_instance_count(int32_t(transforms.size()));
		for (size_t k = 0; k < transforms.size(); k++) {
			multi->set_instance_transform(int32_t(k), transforms[k]);
		}
		MultiMeshInstance3D *node = memnew(MultiMeshInstance3D);
		node->set_name(models[model].name);
		node->set_multimesh(multi);
		node->set_material_override(models[model].paint);
		p_parent->add_child(node);
	}
}

} // namespace godot::city

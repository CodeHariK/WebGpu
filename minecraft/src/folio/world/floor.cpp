#include "floor.h"

#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/height_map_shape3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>

using namespace godot;

FolioFloor::FolioFloor() {}

FolioFloor::~FolioFloor() {}

void FolioFloor::_ready() {
	_build();
	_build_collision();
	_build_bounds();
}

void FolioFloor::_build() {
	// Plane in the XZ plane (normal up), subdivided for displacement.
	Ref<PlaneMesh> plane;
	plane.instantiate();
	plane->set_size(Vector2((float)plane_size, (float)plane_size));
	plane->set_subdivide_width(subdivisions);
	plane->set_subdivide_depth(subdivisions);

	// Material.
	material.instantiate();
	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://material/shaders/folio/mesh_floor.gdshader");
	if (shader.is_valid()) {
		material->set_shader(shader);
	}
	// folio Floor params: no light bounce, no water flatten.
	material->set_shader_parameter("has_light_bounce", false);
	material->set_shader_parameter("has_water", false);

	// Slab detail texture (raw Image so it stays linear data, not sRGB-decoded).
	Ref<Image> slabs = Image::load_from_file("res://material/textures/folio/floor_slabs.png");
	if (slabs.is_valid()) {
		material->set_shader_parameter("floor_slabs", ImageTexture::create_from_image(slabs));
	}

	mesh = memnew(MeshInstance3D);
	mesh->set_name("FloorMesh");
	mesh->set_mesh(plane);
	mesh->set_material_override(material);
	// A displaced plane's flat AABB under-reports; widen the cull margin.
	mesh->set_extra_cull_margin(50.0);
	add_child(mesh);
}

// Bilinear sample of the terrain-data blue channel (height) at UV in [0,1].
static double sample_blue(const Ref<Image> &img, double u, double v) {
	const int w = img->get_width();
	const int h = img->get_height();
	u = Math::clamp(u, 0.0, 1.0) * (double)(w - 1);
	v = Math::clamp(v, 0.0, 1.0) * (double)(h - 1);
	const int x0 = (int)Math::floor(u);
	const int y0 = (int)Math::floor(v);
	const int x1 = Math::min(x0 + 1, w - 1);
	const int y1 = Math::min(y0 + 1, h - 1);
	const double fx = u - (double)x0;
	const double fy = v - (double)y0;
	const double b00 = img->get_pixel(x0, y0).b;
	const double b10 = img->get_pixel(x1, y0).b;
	const double b01 = img->get_pixel(x0, y1).b;
	const double b11 = img->get_pixel(x1, y1).b;
	const double bx0 = b00 + (b10 - b00) * fx;
	const double bx1 = b01 + (b11 - b01) * fx;
	return bx0 + (bx1 - bx0) * fy;
}

// Build a HeightMapShape3D collider that mirrors mesh_floor.gdshader's vertex
// displacement (height = terrain_data.b * displacement_amount * edge_fade), so the
// car drives over the same hills it sees instead of a flat plane. Static, one-time.
void FolioFloor::_build_collision() {
	Ref<Image> data = Image::load_from_file("res://material/textures/folio/terrain_data.png");
	if (data.is_null()) {
		return;
	}

	const int N = collision_samples;
	const double half = plane_size * 0.5;
	const double cell = plane_size / (double)(N - 1); // world units between samples

	PackedFloat32Array heights;
	heights.resize(N * N);
	for (int zi = 0; zi < N; zi++) {
		for (int xi = 0; xi < N; xi++) {
			const double wx = -half + (double)xi * cell;
			const double wz = -half + (double)zi * cell;
			// Shader: tuv = wp.xz / subdivision / 1.5 + 0.5  ==  wp.xz / plane_size + 0.5.
			const double u = wx / plane_size + 0.5;
			const double v = wz / plane_size + 0.5;
			const double b = sample_blue(data, u, v);
			// Shader edge fade: min(min(UV.x, UV.y) * 20, 1), UV = (wp + half) / plane_size.
			const double uvx = (wx + half) / plane_size;
			const double uvy = (wz + half) / plane_size;
			const double edge = Math::min(Math::min(uvx, uvy) * 20.0, 1.0);
			heights[zi * N + xi] = (float)(b * displacement_amount * edge);
		}
	}

	Ref<HeightMapShape3D> shape;
	shape.instantiate();
	shape->set_map_width(N);
	shape->set_map_depth(N);
	shape->set_map_data(heights);

	StaticBody3D *body = memnew(StaticBody3D);
	body->set_name("FloorBody");
	CollisionShape3D *col = memnew(CollisionShape3D);
	col->set_name("FloorCollision");
	col->set_shape(shape);
	// HeightMapShape3D uses 1-unit cells; scale X/Z so (N-1) cells span plane_size.
	col->set_scale(Vector3((float)cell, 1.0f, (float)cell));
	body->add_child(col);
	add_child(body);
}

// Invisible perimeter walls at the island edge (stand-in for folio's bedrock) so the
// vehicle can't drive off the heightfield into the void. Recentring the world for an
// endless island is the deferred camera-follow work.
void FolioFloor::_build_bounds() {
	const float half = (float)(plane_size * 0.5);
	const float th = 4.0f;   // wall thickness
	const float ht = 20.0f;  // wall height (well above the drivable range)
	const float len = (float)plane_size + th * 2.0f;
	const float cy = ht * 0.5f - 5.0f; // sink the base below the terrain minimum

	struct Wall {
		Vector3 pos;
		Vector3 size;
	};
	const Wall walls[4] = {
		{ Vector3(half + th * 0.5f, cy, 0.0f), Vector3(th, ht, len) }, // +X
		{ Vector3(-half - th * 0.5f, cy, 0.0f), Vector3(th, ht, len) }, // -X
		{ Vector3(0.0f, cy, half + th * 0.5f), Vector3(len, ht, th) }, // +Z
		{ Vector3(0.0f, cy, -half - th * 0.5f), Vector3(len, ht, th) }, // -Z
	};

	StaticBody3D *body = memnew(StaticBody3D);
	body->set_name("Bounds");
	add_child(body);
	for (int i = 0; i < 4; i++) {
		Ref<BoxShape3D> box;
		box.instantiate();
		box->set_size(walls[i].size);
		CollisionShape3D *col = memnew(CollisionShape3D);
		col->set_shape(box);
		col->set_position(walls[i].pos);
		body->add_child(col);
	}
}

void FolioFloor::_bind_methods() {}

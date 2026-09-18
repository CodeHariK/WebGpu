#include "floor.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

FolioFloor::FolioFloor() {}

FolioFloor::~FolioFloor() {}

void FolioFloor::_ready() { _build(); }

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

void FolioFloor::_bind_methods() {}

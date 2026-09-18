#include "water_surface.h"

#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

FolioWaterSurface::FolioWaterSurface() {}

FolioWaterSurface::~FolioWaterSurface() {}

void FolioWaterSurface::_ready() { _build(); }

void FolioWaterSurface::_build() {
	Ref<PlaneMesh> plane;
	plane.instantiate();
	plane->set_size(Vector2((float)plane_size, (float)plane_size));

	material.instantiate();
	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://material/shaders/folio/water_surface.gdshader");
	if (shader.is_valid()) {
		material->set_shader(shader);
	}
	// folio water params: no bounce / no water-flatten / no core shadow / no reveal.
	material->set_shader_parameter("has_light_bounce", false);
	material->set_shader_parameter("has_water", false);
	material->set_shader_parameter("has_core_shadows", false);
	material->set_shader_parameter("has_reveal", false);

	mesh = memnew(MeshInstance3D);
	mesh->set_name("WaterMesh");
	mesh->set_mesh(plane);
	mesh->set_material_override(material);
	// The shader pins verts to the water elevation, so the flat AABB is off — widen.
	mesh->set_extra_cull_margin(200.0);
	add_child(mesh);
}

void FolioWaterSurface::_bind_methods() {}

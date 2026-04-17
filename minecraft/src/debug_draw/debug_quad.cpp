#include "debug_quad.h"
#include <godot_cpp/classes/engine.hpp>

namespace godot {

void DebugLineQuad::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_line", "start", "end", "thickness"), &DebugLineQuad::set_line);
	ClassDB::bind_method(D_METHOD("set_shader", "shader"), &DebugLineQuad::set_shader);

	ClassDB::bind_method(D_METHOD("set_start_pos", "pos"), &DebugLineQuad::set_start_pos);
	ClassDB::bind_method(D_METHOD("get_start_pos"), &DebugLineQuad::get_start_pos);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "start_pos"), "set_start_pos", "get_start_pos");

	ClassDB::bind_method(D_METHOD("set_end_pos", "pos"), &DebugLineQuad::set_end_pos);
	ClassDB::bind_method(D_METHOD("get_end_pos"), &DebugLineQuad::get_end_pos);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "end_pos"), "set_end_pos", "get_end_pos");

	ClassDB::bind_method(D_METHOD("set_thickness", "thickness"), &DebugLineQuad::set_thickness);
	ClassDB::bind_method(D_METHOD("get_thickness"), &DebugLineQuad::get_thickness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "thickness"), "set_thickness", "get_thickness");

	ClassDB::bind_method(D_METHOD("set_color", "color"), &DebugLineQuad::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &DebugLineQuad::get_color);
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
}

DebugLineQuad::DebugLineQuad() {
}

DebugLineQuad::~DebugLineQuad() {
}

void DebugLineQuad::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) return;

	// Setup Mesh
	quad_mesh.instantiate();
	quad_mesh->set_size(Vector2(1.0f, 1.0f)); // Unit size, we will scale it
	set_mesh(quad_mesh);

	// Setup Material
	shader_material.instantiate();
	set_material_override(shader_material);

	// Default Shader (flat color)
	Ref<Shader> default_shader;
	default_shader.instantiate();
	default_shader->set_code("shader_type spatial; render_mode unshaded, cull_disabled; uniform vec4 color: source_color; void fragment() { ALBEDO = color.rgb; ALPHA = color.a; }");
	shader_material->set_shader(default_shader);
	shader_material->set_shader_parameter("color", color);

	_update_geometry();
}

void DebugLineQuad::set_line(const Vector3 &p_start, const Vector3 &p_end, float p_thickness) {
	start_pos = p_start;
	end_pos = p_end;
	thickness = p_thickness;
	_update_geometry();
}

void DebugLineQuad::set_shader(const Ref<Shader> &p_shader) {
	if (shader_material.is_null()) {
		shader_material.instantiate();
		set_material_override(shader_material);
	}
	shader_material->set_shader(p_shader);
}

void DebugLineQuad::set_color(const Color &p_color) {
	color = p_color;
	if (shader_material.is_valid()) {
		shader_material->set_shader_parameter("color", color);
	}
}

Color DebugLineQuad::get_color() const { return color; }

void DebugLineQuad::set_start_pos(const Vector3 &p_pos) {
	start_pos = p_pos;
	_update_geometry();
}

Vector3 DebugLineQuad::get_start_pos() const { return start_pos; }

void DebugLineQuad::set_end_pos(const Vector3 &p_pos) {
	end_pos = p_pos;
	_update_geometry();
}

Vector3 DebugLineQuad::get_end_pos() const { return end_pos; }

void DebugLineQuad::set_thickness(float p_thickness) {
	thickness = p_thickness;
	_update_geometry();
}

float DebugLineQuad::get_thickness() const { return thickness; }

void DebugLineQuad::_update_geometry() {
	Vector3 dir = end_pos - start_pos;
	float length = dir.length();
	if (length < 0.0001f) {
		set_visible(false);
		return;
	}
	set_visible(true);

	Vector3 midpoint = start_pos + (dir * 0.5f);
	Vector3 center_to_end = dir.normalized();

	// We want the quad's Y-axis to point along the line.
	// We want the quad's X-axis to represent thickness.
	// We want the quad's Z-axis to be the 'normal' (standard for QuadMesh).

	Transform3D trans;
	
	// Create a basis where Y is the direction of the line
	Vector3 y_axis = center_to_end;
	Vector3 x_axis;
	Vector3 z_axis;

	if (Math::abs(y_axis.dot(Vector3(0, 1, 0))) < 0.99f) {
		x_axis = y_axis.cross(Vector3(0, 1, 0)).normalized();
	} else {
		x_axis = y_axis.cross(Vector3(1, 0, 0)).normalized();
	}
	z_axis = x_axis.cross(y_axis).normalized();

	trans.basis.set_column(0, x_axis * thickness);
	trans.basis.set_column(1, y_axis * length);
	trans.basis.set_column(2, z_axis);
	trans.origin = midpoint;

	set_transform(trans);
}

} // namespace godot

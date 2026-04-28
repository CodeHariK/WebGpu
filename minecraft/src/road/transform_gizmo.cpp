#include "transform_gizmo.h"
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "../debug_draw/debug_manager.h"

namespace godot {

void TransformGizmo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_manipulation_mode", "mode"), &TransformGizmo::set_manipulation_mode);
	ClassDB::bind_method(D_METHOD("get_manipulation_mode"), &TransformGizmo::get_manipulation_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "manipulation_mode", PROPERTY_HINT_ENUM, "None,Move,MovePlanar,MoveOrRotate,MoveOrScale,Rotate,Scale"), "set_manipulation_mode", "get_manipulation_mode");

	ClassDB::bind_method(D_METHOD("set_update_callback", "callback"), &TransformGizmo::set_update_callback);
	ClassDB::bind_method(D_METHOD("get_update_callback"), &TransformGizmo::get_update_callback);
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "update_callback"), "set_update_callback", "get_update_callback");

	ClassDB::bind_method(D_METHOD("set_move_snap", "snap"), &TransformGizmo::set_move_snap);
	ClassDB::bind_method(D_METHOD("get_move_snap"), &TransformGizmo::get_move_snap);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "move_snap"), "set_move_snap", "get_move_snap");

	ClassDB::bind_method(D_METHOD("set_rotate_snap", "snap"), &TransformGizmo::set_rotate_snap);
	ClassDB::bind_method(D_METHOD("get_rotate_snap"), &TransformGizmo::get_rotate_snap);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rotate_snap"), "set_rotate_snap", "get_rotate_snap");

	ClassDB::bind_method(D_METHOD("set_scale_snap", "snap"), &TransformGizmo::set_scale_snap);
	ClassDB::bind_method(D_METHOD("get_scale_snap"), &TransformGizmo::get_scale_snap);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale_snap"), "set_scale_snap", "get_scale_snap");

	ClassDB::bind_method(D_METHOD("_on_gizmo_input_start"), &TransformGizmo::_on_gizmo_input_start);

	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(MOVE);
	BIND_ENUM_CONSTANT(MOVE_PLANAR);
	BIND_ENUM_CONSTANT(MOVE_OR_ROTATE);
	BIND_ENUM_CONSTANT(MOVE_OR_SCALE);
	BIND_ENUM_CONSTANT(ROTATE);
	BIND_ENUM_CONSTANT(SCALE);
}

TransformGizmo::TransformGizmo() {
	set_process_mode(PROCESS_MODE_PAUSABLE);
}

TransformGizmo::~TransformGizmo() {
}

void TransformGizmo::_ready() {
	ResourceLoader *rl = ResourceLoader::get_singleton();
	circle_shader = rl->load("res://scene/road/circle_gizmo.gdshader");

	_create_box_gizmo("MoveX", Color(1, 0.2f, 0.2f, 1), Transform3D(Basis(Vector3(0, 0, -1), Math_PI / 2.0f)),
			MOVE, Vector3(0, MOVE_GIZMO_LEN / 2 + SCALE_GIZMO_LEN / 2, 0), Vector2(0.06f, MOVE_GIZMO_LEN));
	_create_box_gizmo("MoveY", Color(0.6f, 1, 0.2f, 1), Transform3D(),
			MOVE, Vector3(0, MOVE_GIZMO_LEN / 2 + SCALE_GIZMO_LEN / 2, 0), Vector2(0.06f, MOVE_GIZMO_LEN));
	_create_box_gizmo("MoveZ", Color(0.5f, 0.5f, 1, 1), Transform3D(Basis(Vector3(1, 0, 0), Math_PI / 2.0f)),
			MOVE, Vector3(0, MOVE_GIZMO_LEN / 2 + SCALE_GIZMO_LEN / 2, 0), Vector2(0.06f, MOVE_GIZMO_LEN));

	_create_plane_gizmo("RotateX", Color(1, 0.2f, 0.2f, 1), Transform3D(Basis(Vector3(0, 1, 0), Math_PI / 2.0f)));
	_create_plane_gizmo("RotateY", Color(0.6f, 1, 0.2f, 1), Transform3D(Basis(Vector3(1, 0, 0), Math_PI / 2.0f)));
	_create_plane_gizmo("RotateZ", Color(0.5f, 0.5f, 1, 1), Transform3D());

	grid_mesh_instance = memnew(MeshInstance3D);
	Ref<QuadMesh> grid_mesh;
	grid_mesh.instantiate();
	grid_mesh->set_size(Vector2(5, 5));
	grid_shader = rl->load("res://scene/road/grid_shader.gdshader");
	Ref<ShaderMaterial> grid_mat;
	grid_mat.instantiate();
	grid_mat->set_shader(grid_shader);
	grid_mesh_instance->set_mesh(grid_mesh);
	grid_mesh_instance->set_material_override(grid_mat);
	grid_mesh_instance->set_visible(false);
	add_child(grid_mesh_instance);
}

void TransformGizmo::_input(const Ref<InputEvent> &event) {
	Ref<InputEventMouseButton> mb = event;
	if (mb.is_valid() && mb->get_button_index() == MOUSE_BUTTON_LEFT) {
		if (!mb->is_pressed()) {
			manipulation_mode = NONE;
			if (grid_mesh_instance)
				grid_mesh_instance->set_visible(false);
		}
	}

	Ref<InputEventMouseMotion> mm = event;
	if (mm.is_valid() && manipulation_mode != NONE) {
		Viewport *vp = get_viewport();
		if (!vp)
			return;
		Camera3D *camera = vp->get_camera_3d();
		if (!camera)
			return;

		Vector2 mouse_pos = mm->get_position();
		Vector3 ray_origin = camera->project_ray_origin(mouse_pos);
		Vector3 ray_dir = camera->project_ray_normal(mouse_pos);

		Vector3 intersection_point;
		if (drag_plane.intersects_ray(ray_origin, ray_dir, &intersection_point)) {
			Transform3D new_transform = _calculate_new_transform(intersection_point);
			DebugManager::get_singleton()->draw_line("gizmo", get_global_transform().origin, intersection_point, .05f, Color(1, 1, 0));
			set_gizmo_transform(new_transform);
		}
	}
}

void TransformGizmo::_process(double delta) {
	if (get_global_transform() != last_transform) {
		last_transform = get_global_transform();
		if (update_callback.is_valid()) {
			update_callback.call(get_global_transform());
		}
	}
}

void TransformGizmo::set_gizmo_transform(const Transform3D &p_transform) {
	set_global_transform(p_transform);
	last_transform = p_transform;
}

void TransformGizmo::_create_box_gizmo(const String &p_name, const Color &p_color, const Transform3D &p_transform,
		ManipulationMode p_mode, const Vector3 &p_offset, const Vector2 &size) {
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	Ref<CylinderMesh> cylinder;
	cylinder.instantiate();
	cylinder->set_height(size.y);
	cylinder->set_radial_segments(8);
	cylinder->set_top_radius(size.x * 0.5f);
	cylinder->set_bottom_radius(size.x * 0.5f);

	Transform3D cyl_transform = Transform3D().translated(p_offset);
	st->append_from(cylinder, 0, cyl_transform);
	Ref<Mesh> mesh = st->commit();

	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_albedo(p_color);
	mesh->surface_set_material(0, mat);

	MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_name(p_name);
	mesh_instance->set_mesh(mesh);
	mesh_instance->set_transform(p_transform);

	Area3D *area = memnew(Area3D);
	area->connect("input_event", callable_mp(this, &TransformGizmo::_on_gizmo_input_start).bind(mesh_instance, MOVE_OR_SCALE));
	mesh_instance->add_child(area);

	CollisionShape3D *collision_shape = memnew(CollisionShape3D);
	Ref<BoxShape3D> box_shape;
	box_shape.instantiate();
	box_shape->set_size(Vector3(size.x, size.y, size.x));
	collision_shape->set_shape(box_shape);
	collision_shape->set_transform(Transform3D().translated(p_offset));
	area->add_child(collision_shape);

	add_child(mesh_instance);
}

void TransformGizmo::_create_plane_gizmo(const String &p_name, const Color &p_color, const Transform3D &p_transform) {
	Ref<QuadMesh> disc;
	disc.instantiate();
	disc->set_size(Vector2(SCALE_GIZMO_LEN, SCALE_GIZMO_LEN));

	Ref<ShaderMaterial> mat;
	mat.instantiate();
	mat->set_shader(circle_shader);
	mat->set_shader_parameter("rotation_color", p_color);

	MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_name(p_name);
	mesh_instance->set_mesh(disc);
	mesh_instance->set_transform(p_transform);
	mesh_instance->set_material_override(mat);

	Area3D *area = memnew(Area3D);
	area->connect("input_event", callable_mp(this, &TransformGizmo::_on_gizmo_input_start).bind(mesh_instance, MOVE_OR_ROTATE));
	mesh_instance->add_child(area);

	CollisionShape3D *collision_shape = memnew(CollisionShape3D);
	Ref<BoxShape3D> box_shape;
	box_shape.instantiate();
	box_shape->set_size(Vector3(SCALE_GIZMO_LEN, SCALE_GIZMO_LEN, 0.02f));
	collision_shape->set_shape(box_shape);
	area->add_child(collision_shape);

	add_child(mesh_instance);
}

void TransformGizmo::_on_gizmo_input_start(Node *p_camera, Ref<InputEvent> p_event, Vector3 position, Vector3 normal, int shape_idx, MeshInstance3D *mesh_node, int mode) {
	Camera3D *camera = Object::cast_to<Camera3D>(p_camera);
	Ref<InputEventMouseButton> mb = p_event;

	if (camera && mb.is_valid() && mb->get_button_index() == MOUSE_BUTTON_LEFT && mb->is_pressed()) {
		Vector3 plane_normal;
		Basis global_basis = get_global_transform().basis;

		String name = mesh_node->get_name();
		if (name.contains("X"))
			drag_axis = global_basis.get_column(0);
		else if (name.contains("Y"))
			drag_axis = global_basis.get_column(1);
		else if (name.contains("Z"))
			drag_axis = global_basis.get_column(2);

		if (mode == MOVE_OR_ROTATE) {
			float distance_from_center = (position - get_global_transform().origin).length();
			float inner_radius = SCALE_GIZMO_LEN * 0.45f;
			if (distance_from_center <= inner_radius) {
				manipulation_mode = ROTATE;
			} else {
				manipulation_mode = MOVE_PLANAR;
			}
			plane_normal = drag_axis;
		}

		if (mode == MOVE_OR_SCALE) {
			Vector3 camera_forward = -camera->get_global_transform().basis.get_column(2);
			plane_normal = drag_axis.cross(camera_forward).cross(drag_axis).normalized();

			if (plane_normal.is_zero_approx()) {
				plane_normal = drag_axis.cross(camera->get_global_transform().basis.get_column(0)).cross(drag_axis).normalized();
			}

			Vector3 click_offset_vec = position - get_global_transform().origin;
			float distance_along_axis = click_offset_vec.dot(drag_axis);
			float scale_threshold = MOVE_GIZMO_LEN * 0.5f + SCALE_GIZMO_LEN * 0.6f;

			if (distance_along_axis > scale_threshold) {
				manipulation_mode = SCALE;
			} else {
				manipulation_mode = MOVE;
			}
		}

		drag_plane = Plane(plane_normal, get_global_transform().origin);
		Vector3 intersection_point;
		if (drag_plane.intersects_ray(camera->project_ray_origin(mb->get_position()), camera->project_ray_normal(mb->get_position()), &intersection_point)) {
			drag_start_position = intersection_point;
			drag_start_transform = get_global_transform();
			drag_start_vector = (drag_start_position - get_global_transform().origin).normalized();

			Vector3 perp = drag_axis;
			if (manipulation_mode == MOVE) {
				if (drag_axis.is_equal_approx(global_basis.get_column(1))) {
					perp = global_basis.get_column(0);
				} else {
					perp = global_basis.get_column(1);
				}
			}

			if (grid_mesh_instance) {
				grid_mesh_instance->set_global_transform(Transform3D(Basis::looking_at(perp), get_global_transform().origin));
				grid_mesh_instance->set_visible(true);
			}
		}
	}
}

Transform3D TransformGizmo::_calculate_new_transform(const Vector3 &p_intersection_point) {
	Transform3D new_transform = drag_start_transform;

	switch (manipulation_mode) {
		case MOVE: {
			Vector3 motion_vector = p_intersection_point - drag_start_position;
			float projected_motion_length = motion_vector.dot(drag_axis);
			if (move_snap > 0) {
				projected_motion_length = Math::snapped(projected_motion_length, move_snap);
			}
			new_transform.origin = drag_start_transform.origin + drag_axis * projected_motion_length;
			break;
		}
		case MOVE_PLANAR: {
			Vector3 motion_vector = p_intersection_point - drag_start_position;
			Vector3 new_pos = drag_start_transform.origin + motion_vector;
			if (move_snap > 0) {
				new_pos.x = Math::snapped(new_pos.x, move_snap);
				new_pos.y = Math::snapped(new_pos.y, move_snap);
				new_pos.z = Math::snapped(new_pos.z, move_snap);
			}
			new_transform.origin = new_pos;
			break;
		}
		case ROTATE: {
			if (!drag_start_vector.is_zero_approx()) {
				Vector3 rotation_axis = drag_axis.normalized();
				Vector3 current_vector = (p_intersection_point - new_transform.origin).normalized();

				Vector3 v_start_proj = (drag_start_vector - drag_start_vector.project(rotation_axis)).normalized();
				Vector3 v_current_proj = (current_vector - current_vector.project(rotation_axis)).normalized();

				if (!v_start_proj.is_zero_approx() && !v_current_proj.is_zero_approx()) {
					float angle = v_start_proj.signed_angle_to(v_current_proj, rotation_axis);
					if (rotate_snap > 0) {
						angle = Math::snapped(angle, (float)Math::deg_to_rad(rotate_snap));
					}
					new_transform.basis = drag_start_transform.basis.rotated(rotation_axis, angle);
				}
			}
			break;
		}
		case SCALE: {
			Vector3 motion_vector = p_intersection_point - drag_start_position;
			float projected_motion_length = motion_vector.dot(drag_axis);
			if (scale_snap > 0) {
				projected_motion_length = Math::snapped(projected_motion_length, scale_snap);
			}

			float scale_factor = 1.0f + projected_motion_length;
			if (scale_factor > 0.001f) {
				Vector3 scale_vector = drag_axis.abs() * (scale_factor - 1.0f) + Vector3(1, 1, 1);
				new_transform = drag_start_transform.scaled_local(scale_vector);
			}
			break;
		}
		default:
			break;
	}

	return new_transform;
}

// Property Accessors
void TransformGizmo::set_manipulation_mode(ManipulationMode p_mode) { manipulation_mode = p_mode; }
TransformGizmo::ManipulationMode TransformGizmo::get_manipulation_mode() const { return manipulation_mode; }
void TransformGizmo::set_update_callback(const Callable &p_callback) { update_callback = p_callback; }
Callable TransformGizmo::get_update_callback() const { return update_callback; }
void TransformGizmo::set_move_snap(float p_snap) { move_snap = p_snap; }
float TransformGizmo::get_move_snap() const { return move_snap; }
void TransformGizmo::set_rotate_snap(float p_snap) { rotate_snap = p_snap; }
float TransformGizmo::get_rotate_snap() const { return rotate_snap; }
void TransformGizmo::set_scale_snap(float p_snap) { scale_snap = p_snap; }
float TransformGizmo::get_scale_snap() const { return scale_snap; }

} // namespace godot

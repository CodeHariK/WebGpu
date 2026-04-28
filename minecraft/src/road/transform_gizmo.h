#ifndef TRANSFORM_GIZMO_H
#define TRANSFORM_GIZMO_H

#include <godot_cpp/classes/area3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/shader.hpp>

namespace godot {

class TransformGizmo : public Node3D {
	GDCLASS(TransformGizmo, Node3D)

public:
	enum ManipulationMode : uint8_t {
		NONE,
		MOVE,
		MOVE_PLANAR,
		MOVE_OR_ROTATE,
		MOVE_OR_SCALE,
		ROTATE,
		SCALE
	};

private:
	Callable update_callback;
	ManipulationMode manipulation_mode = NONE;

	float move_snap = 0.0f;
	float rotate_snap = 0.0f;
	float scale_snap = 0.0f;

	float MOVE_GIZMO_LEN = 0.5f;
	float SCALE_GIZMO_LEN = 1.25f;

	Vector3 drag_axis;
	Plane drag_plane;
	Vector3 drag_start_position;
	Vector3 drag_start_vector;
	Transform3D last_transform;
	Transform3D drag_start_transform;

	MeshInstance3D *grid_mesh_instance = nullptr;
	Ref<Shader> circle_shader;
	Ref<Shader> grid_shader;

	void _create_box_gizmo(const String &p_name, const Color &p_color, const Transform3D &p_transform,
			ManipulationMode p_mode, const Vector3 &p_offset, const Vector2 &size);
	void _create_plane_gizmo(const String &p_name, const Color &p_color, const Transform3D &p_transform);

	void _on_gizmo_input_start(Node *camera, Ref<InputEvent> event, Vector3 position, Vector3 normal, int shape_idx, MeshInstance3D *mesh_node, int mode);
	Transform3D _calculate_new_transform(const Vector3 &p_intersection_point);

protected:
	static void _bind_methods();

public:
	TransformGizmo();
	~TransformGizmo();

	void _ready() override;
	void _input(const Ref<InputEvent> &event) override;
	void _process(double delta) override;

	void set_gizmo_transform(const Transform3D &p_transform);

	// Getters/Setters for properties
	void set_manipulation_mode(ManipulationMode p_mode);
	ManipulationMode get_manipulation_mode() const;

	void set_update_callback(const Callable &p_callback);
	Callable get_update_callback() const;

	void set_move_snap(float p_snap);
	float get_move_snap() const;

	void set_rotate_snap(float p_snap);
	float get_rotate_snap() const;

	void set_scale_snap(float p_snap);
	float get_scale_snap() const;
};

} // namespace godot

VARIANT_ENUM_CAST(TransformGizmo::ManipulationMode);

#endif // TRANSFORM_GIZMO_H

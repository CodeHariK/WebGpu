#ifndef ACNHNODE_CLASS
#define ACNHNODE_CLASS

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

class ACNHNode : public Node3D {
	GDCLASS(ACNHNode, Node3D);

private:
	NodePath ground_mesh_path;
	Node3D *ground_mesh = nullptr;
	Camera3D *camera = nullptr;

	void _perform_raycast(const Vector2 &p_mouse_pos);

protected:
	static void _bind_methods();

public:
	ACNHNode();
	~ACNHNode();

	void _ready() override;
	void _input(const Ref<InputEvent> &p_event) override;

	void set_ground_mesh_path(const NodePath &p_path);
	NodePath get_ground_mesh_path() const;
};

#endif

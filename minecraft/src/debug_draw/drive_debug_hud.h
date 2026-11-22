/**
 * @file drive_debug_hud.h
 * @brief DriveDebugHud: a small debug overlay for driving scenes. At the top of the screen it shows
 * the name of what the player controls (GameManager's active target: a vehicle or a character),
 * and a "Collision shapes" check box. Checked, every collision shape of every vehicle and character,
 * and every vehicle wheel's collider (the sphere its suspension casts), is drawn as a wireframe (on
 * top of everything); unchecked (the default) they are hidden.
 */
#ifndef DRIVE_DEBUG_HUD_H
#define DRIVE_DEBUG_HUD_H

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <unordered_set>
#include <vector>

namespace godot {

class DriveDebugHud : public CanvasLayer {
	GDCLASS(DriveDebugHud,
			CanvasLayer)

private:
	Label *name_label = nullptr;
	CheckBox *wire_box = nullptr;
	Ref<StandardMaterial3D> wire_material;
	std::unordered_set<uint64_t> wired; // the collision shapes that already have a wireframe
	std::vector<ObjectID> wires; // the wireframes, to show or hide
	bool wires_shown = false;
	double rescan = 0.0; // seconds to the next look for new collision shapes

	void _build();
	void _wire_shapes_under(Node *p_node);
	void _wire_wheels_of(Node *p_vehicle);
	MeshInstance3D *_make_wire(const Ref<Shape3D> &p_shape);
	void _scan();
	void _show_wires(bool p_shown);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void _on_wire_toggled(bool p_on);
	void _process(double p_delta) override;
};

} // namespace godot

#endif // DRIVE_DEBUG_HUD_H

/**
 * @file track_model.h
 * @brief TrackModel: a race track model (any glTF/glb scene) made drivable. On ready it instances
 * the model scaled by model_scale, moves it so the middle of its start mesh (the first mesh whose
 * name contains start_part) is at this node's origin with the road surface at y = 0, gives every
 * mesh a trimesh collider (so the road, kerbs, walls and stands can be driven on and hit), and adds
 * a safety floor FLOOR metres below the road.
 */
#ifndef TRACK_MODEL_H
#define TRACK_MODEL_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>

namespace godot {

class TrackModel : public Node3D {
	GDCLASS(TrackModel,
			Node3D)

private:
	Ref<PackedScene> model;
	float model_scale = 1.0f;
	String start_part = "start";

	void _build();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_model(const Ref<PackedScene> &p_model);
	Ref<PackedScene> get_model() const;
	void set_model_scale(float p_scale);
	float get_model_scale() const;
	void set_start_part(const String &p_part);
	String get_start_part() const;
};

} // namespace godot

#endif // TRACK_MODEL_H

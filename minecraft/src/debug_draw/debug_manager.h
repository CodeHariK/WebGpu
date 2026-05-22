#ifndef DEBUG_MANAGER_H
#define DEBUG_MANAGER_H

#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <vector>

namespace godot {

class DebugLineQuad;

class DebugManager : public Node {
	GDCLASS(DebugManager, Node)

private:
	static DebugManager *singleton;

	struct DebugLine {
		DebugLineQuad *quad = nullptr;
		float duration = -1.0f;
	};

	struct DebugText {
		Label3D *label = nullptr;
		float duration = -1.0f;
	};

	struct Trajectory {
		std::vector<Vector3> points;
		float timer = 0.0f;
	};

	struct DebugSphere {
		MeshInstance3D *mesh_instance = nullptr;
		float duration = -1.0f;
	};

	HashMap<String, DebugLine> lines;
	HashMap<String, DebugText> texts;
	HashMap<String, Trajectory> trajectories;
	HashMap<String, DebugSphere> spheres;

protected:
	static void _bind_methods();

public:
	DebugManager();
	~DebugManager();

	static DebugManager *get_singleton();

	void _enter_tree() override;
	void _exit_tree() override;
	void _physics_process(double delta) override;

	void draw_line(const String &p_id, const Vector3 &p_start, const Vector3 &p_end, float p_thickness = 0.05f, const Color &p_color = Color(1, 1, 1), float p_duration = -1.0f);
	void clear_line(const String &p_id);

	void draw_text(const String &p_id, const String &p_text, const Vector3 &p_pos, float p_size = 0.002f, const Color &p_color = Color(1, 1, 1), float p_duration = -1.0f);
	void clear_text(const String &p_id);

	void draw_sphere(const String &p_id, const Vector3 &p_pos, float p_radius = 0.5f, const Color &p_color = Color(1, 1, 1), float p_duration = -1.0f);
	void clear_sphere(const String &p_id);

	void draw_trajectory(const String &p_id, const Vector3 &p_point, float p_delta, float p_interval = 0.1f, int p_max_points = 200, const Color &p_color = Color(1, 1, 1));
	void clear_trajectory(const String &p_id);

	void clear_all();
};

} // namespace godot

#endif // DEBUG_MANAGER_H

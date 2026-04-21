#ifndef DEBUG_MANAGER_H
#define DEBUG_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

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

	HashMap<String, DebugLine> lines;
	HashMap<String, DebugText> texts;

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

	void draw_text(const String &p_id, const String &p_text, const Vector3 &p_pos, float p_size = 0.05f, const Color &p_color = Color(1, 1, 1), float p_duration = -1.0f);
	void clear_text(const String &p_id);

	void clear_all();
};

} // namespace godot

#endif // DEBUG_MANAGER_H

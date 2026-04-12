#ifndef MC_MANAGER_H
#define MC_MANAGER_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/node_path.hpp>

namespace godot {

class MCNode;
class MCTerrain;
class CUI;
class VBoxContainer;
class Panel;
class AcceptDialog;
class Label;
class InputEvent;
class MeshInstance3D;
class StandardMaterial3D;
class Camera3D;
class QuadMesh;

class MCManager : public Node3D {
	GDCLASS(MCManager, Node3D)

public:
	struct MCUI {
		CUI *manager = nullptr;
		VBoxContainer *stats_vbox = nullptr;
		VBoxContainer *variant_stats_vbox = nullptr;
		Panel *side_panel = nullptr;
		AcceptDialog *help_dialog = nullptr;
	};

	struct TerrainStats {
		NodePath path;
		Label *stats_label = nullptr;
	};

	struct PerfStats {
		Label *fps_label = nullptr;
		Label *draw_calls_label = nullptr;
		Label *meshes_label = nullptr;
		Label *collision_label = nullptr;
		Label *memory_label = nullptr;

		double update_timer = 0.0;
	};

private:
	NodePath mc_node_path;
	
	MCUI ui;
	TerrainStats terrain;
	PerfStats perf;
	Node3D *hover_root = nullptr;
	MeshInstance3D *hover_quads[3] = {nullptr, nullptr, nullptr};
	Ref<StandardMaterial3D> hover_mat_yellow;
	Ref<StandardMaterial3D> hover_mat_white;

	// Debug Cursor
	Vector3i debug_cursor_pos;
	MeshInstance3D *debug_cursor_node = nullptr;
	Ref<StandardMaterial3D> debug_cursor_mat;
	Label *debug_cursor_label = nullptr;

protected:
	static void _bind_methods();

public:
	MCManager();
	~MCManager() override;

	void _ready() override;

	void set_mc_node_path(const NodePath &p_path);
	NodePath get_mc_node_path() const;

	void set_terrain_path(const NodePath &p_path);
	NodePath get_terrain_path() const;

	void initialize_all();

	// UI & Performance
	void setup_ui();
	void update_ui();
	void _process(double p_delta) override;
	void _on_toggle_ui();
	void _on_toggle_collision_debug();
	void _on_toggle_visual_corners();
	void _input(const Ref<InputEvent> &p_event) override;
	void _on_gui_input(const Ref<InputEvent> &p_event);
	void _on_show_help();
	void _on_save_terrain();
	void _on_load_terrain();
	void _update_hover_preview(const Vector3 &p_corner_pos, const Vector3 &p_hit_normal, Camera3D *p_camera);
};

} // namespace godot

#endif // MC_MANAGER_H

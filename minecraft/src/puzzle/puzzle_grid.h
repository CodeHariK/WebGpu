#ifndef PUZZLE_GRID_H
#define PUZZLE_GRID_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <vector>

namespace godot {

class PushBox;
class StaticBody3D;

/**
 * PuzzleGrid — Sokoban-style puzzle rooms authored as an ASCII layout (Dino and Aliens).
 * ------------------------------------------------------------------------------------
 * Each character of `layout` is one `cell_size` cell (column = +X, row = +Z, origin = this
 * node, floor on y = 0):
 *
 *   #  wall          .  floor        (space)  nothing          =  bridge
 *   B  crate         X  box bomb     K  key        A  alien     E  exit door     P  floor (start)
 *   ~  water (box pushed in sinks → floor)    ^  lava (box pushed in burns)    I  ice (boxes slide)
 *
 * Islands: floors are raised slabs (FLOOR_TOP), liquids lie below them behind invisible fences.
 * Builds floor tiles, bridges, walls, liquids (MultiMesh, merged collision runs), PushBoxes and
 * prop visuals,
 * in the editor too (generated nodes are not saved: only the layout string is).
 *
 * Walking into the grid switches to puzzle play: no jumping, and with `use_topdown_camera`
 * (off for now) a top-down camera (`topdown_offset`) with camera-relative movement;
 * walking out restores the previous setup.
 *
 * `procedural` replaces the authored layout with a SokobanGenerator level from `seed`
 * (rooms, crates in doorways, key / exit / aliens / box bombs), verified solvable by
 * SokobanSolver; `generation_info` reports pushes needed and attempts.
 *
 * The only mechanic for now is PUSHING: walk into a box along a grid axis for `push_delay`
 * seconds and it slides one cell (further across ice) onto free ground; into water it
 * sinks and becomes a walkable raft, into lava it burns. No pulling, so a box in a corner
 * is stuck — that's the puzzle.
 * Aliens, keys and the door are visuals only (bob / spin), blocking box moves.
 */
class PuzzleGrid : public Node3D {
	GDCLASS(PuzzleGrid,
			Node3D)

private:
	// --- Properties ---
	String layout;
	// Procedural (SokobanGenerator): when on, the level comes from the seed instead of `layout`.
	bool procedural = false;
	int seed = 1;
	Vector2i gen_size = Vector2i(21, 13);
	int gen_rooms = 4;
	int gen_extra_crates = 3;
	int gen_bombs = 2;
	int gen_aliens = 2;
	int gen_min_pushes = 3;
	int gen_ring = 1; ///< outer ring: 0 walls, 1 water (islands), 2 lava
	float gen_gap_chance = 0.5f;
	int gen_pools = 2;
	int gen_ice_patches = 1;
	String generation_info; ///< read-only report of the last generation
	String _source_layout(); ///< generated or authored layout text
	float cell_size = 1.5f;
	float wall_height = 2.5f;
	float push_delay = 0.12f; ///< Seconds of walking into a box before it moves.
	float slide_time = 0.18f;

	// --- Built state ---
	Node3D *content = nullptr;
	PackedStringArray rows;
	int width = 0;
	int depth = 0;
	std::vector<char> cells; ///< PuzzleCell kinds (puzzle_cells.h)
	std::vector<PushBox *> boxes; ///< pushable boxes
	std::vector<PushBox *> settling; ///< sliding into a liquid, resolved on arrival
	std::vector<PushBox *> burning; ///< sinking into lava, freed when gone
	StaticBody3D *dynamic_body = nullptr; ///< ground + liquid fences, rebuilt when water fills
	Ref<ShaderMaterial> water_mat;
	Ref<ShaderMaterial> lava_mat;
	std::vector<Node3D *> spinners; ///< key bodies
	std::vector<Node3D *> bobbers; ///< alien bodies
	float anim_time = 0.0f;

	// --- Room mode (player inside the grid: top-down camera, no jumping) ---
	bool use_topdown_camera = false; ///< off for now: keep the normal camera (top-down to be fixed later)
	Vector3 topdown_offset = Vector3(0, 14, 7);
	bool player_inside = false;
	uint64_t inside_id = 0; ///< instance id of the player inside (safe if it gets freed)
	int saved_scheme = -1;

	// --- Push state ---
	float push_timer = 0.0f;
	PushBox *push_target = nullptr;

	void _build();
	void _clear();
	void _parse();
	// Terrain (puzzle_grid_terrain.cpp)
	void _build_floor_tiles();
	void _build_bridges();
	void _build_walls();
	void _build_liquids();
	void _build_static_collision();
	void _rebuild_dynamic_collision();
	void _build_collision_runs(
			StaticBody3D *p_body,
			bool (*p_pred)(char),
			float p_height,
			float p_y
	);
	int _count_cells(bool (*p_pred)(char)) const;
	void _build_items();
	void _animate(float p_dt);
	void _update_push(float p_dt);
	void _update_room_mode(Node3D *p_player);
	void _enter_room(Node3D *p_player);
	void _leave_room();
	bool _contains(const Vector3 &p_local) const; ///< grid rectangle + leave margin
	bool _on_room_floor(const Vector3 &p_local) const; ///< standing on a floor cell

	char _cell(
			int p_x,
			int p_z
	) const;
	// Pushing (puzzle_grid_push.cpp)
	enum Landing {
		LAND_NONE, ///< blocked
		LAND_REST, ///< on ground (after any ice slide)
		LAND_SINK, ///< into water: fills it
		LAND_BURN, ///< into lava: lost
	};
	Landing _resolve_push(
			int p_x,
			int p_z,
			int p_dx,
			int p_dz,
			int &r_x,
			int &r_z
	) const;
	void _settle_boxes();
	PushBox *_box_at(
			int p_x,
			int p_z
	) const;
	Vector3 _cell_center(
			int p_x,
			int p_z
	) const;
	Vector3 _wish_local(Node3D *p_player) const;

protected:
	static void _bind_methods();

public:
	PuzzleGrid();

	void _ready() override;
	void _physics_process(double p_delta) override;
	void _exit_tree() override;

	void set_layout(const String &p_layout);
	String get_layout() const { return layout; }
	void set_cell_size(float p_v);
	float get_cell_size() const { return cell_size; }
	void set_wall_height(float p_v);
	float get_wall_height() const { return wall_height; }
	void set_push_delay(float p_v) { push_delay = p_v; }
	float get_push_delay() const { return push_delay; }
	void set_procedural(bool p_v);
	bool get_procedural() const { return procedural; }
	void set_seed(int p_v);
	int get_seed() const { return seed; }
	void set_gen_size(const Vector2i &p_v);
	Vector2i get_gen_size() const { return gen_size; }
	void set_gen_rooms(int p_v);
	int get_gen_rooms() const { return gen_rooms; }
	void set_gen_extra_crates(int p_v);
	int get_gen_extra_crates() const { return gen_extra_crates; }
	void set_gen_bombs(int p_v);
	int get_gen_bombs() const { return gen_bombs; }
	void set_gen_aliens(int p_v);
	int get_gen_aliens() const { return gen_aliens; }
	void set_gen_min_pushes(int p_v);
	int get_gen_min_pushes() const { return gen_min_pushes; }
	void set_gen_ring(int p_v);
	int get_gen_ring() const { return gen_ring; }
	void set_gen_gap_chance(float p_v);
	float get_gen_gap_chance() const { return gen_gap_chance; }
	void set_gen_pools(int p_v);
	int get_gen_pools() const { return gen_pools; }
	void set_gen_ice_patches(int p_v);
	int get_gen_ice_patches() const { return gen_ice_patches; }
	String get_generation_info() const { return generation_info; }
	void set_generation_info(const String &) {} // read-only in the inspector
	void set_use_topdown_camera(bool p_v) { use_topdown_camera = p_v; }
	bool get_use_topdown_camera() const { return use_topdown_camera; }
	void set_topdown_offset(const Vector3 &p_v) { topdown_offset = p_v; }
	Vector3 get_topdown_offset() const { return topdown_offset; }
	void set_slide_time(float p_v) { slide_time = p_v; }
	float get_slide_time() const { return slide_time; }
};

} // namespace godot

#endif // PUZZLE_GRID_H

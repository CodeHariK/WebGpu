#include "puzzle_grid.h"

#include "push_box.h"
#include "puzzle_cells.h"
#include "puzzle_props.h"
#include "sokoban_generator.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

static const char *DEFAULT_LAYOUT = "##.##############\n"
									"#.....#.........#\n"
									"#.P...B...A..A..#\n"
									"#.X...#.........#\n"
									"#.....#....X....#\n"
									"###.#############\n"
									"#.....#.........#\n"
									"#..B..B....K....#\n"
									"#.....#.........#\n"
									"#.....#.........#\n"
									"#####E###########\n";

using namespace PuzzleCell;

void PuzzleGrid::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_layout", "layout"), &PuzzleGrid::set_layout);
	ClassDB::bind_method(D_METHOD("get_layout"), &PuzzleGrid::get_layout);
	ClassDB::bind_method(D_METHOD("set_cell_size", "v"), &PuzzleGrid::set_cell_size);
	ClassDB::bind_method(D_METHOD("get_cell_size"), &PuzzleGrid::get_cell_size);
	ClassDB::bind_method(D_METHOD("set_wall_height", "v"), &PuzzleGrid::set_wall_height);
	ClassDB::bind_method(D_METHOD("get_wall_height"), &PuzzleGrid::get_wall_height);
	ClassDB::bind_method(D_METHOD("set_push_delay", "v"), &PuzzleGrid::set_push_delay);
	ClassDB::bind_method(D_METHOD("get_push_delay"), &PuzzleGrid::get_push_delay);
	ClassDB::bind_method(D_METHOD("set_slide_time", "v"), &PuzzleGrid::set_slide_time);
	ClassDB::bind_method(D_METHOD("get_slide_time"), &PuzzleGrid::get_slide_time);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "layout", PROPERTY_HINT_MULTILINE_TEXT), "set_layout", "get_layout");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_size", PROPERTY_HINT_RANGE, "0.5,5,0.05,suffix:m"), "set_cell_size",
			"get_cell_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wall_height", PROPERTY_HINT_RANGE, "0.2,10,0.1,suffix:m"),
			"set_wall_height", "get_wall_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "push_delay", PROPERTY_HINT_RANGE, "0,1,0.01,suffix:s"), "set_push_delay",
			"get_push_delay");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "slide_time", PROPERTY_HINT_RANGE, "0.02,1,0.01,suffix:s"),
			"set_slide_time", "get_slide_time");
	ADD_GROUP("Procedural", "");
#define PUZZLE_BIND(m_name, m_variant, m_hint, m_hint_text)                                              \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "v"), &PuzzleGrid::set_##m_name);                    \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &PuzzleGrid::get_##m_name);                          \
	ADD_PROPERTY(PropertyInfo(Variant::m_variant, #m_name, m_hint, m_hint_text), "set_" #m_name, "get_" #m_name);
	PUZZLE_BIND(procedural, BOOL, PROPERTY_HINT_NONE, "")
	PUZZLE_BIND(seed, INT, PROPERTY_HINT_RANGE, "0,1000000,1")
	PUZZLE_BIND(gen_size, VECTOR2I, PROPERTY_HINT_NONE, "suffix:cells")
	PUZZLE_BIND(gen_rooms, INT, PROPERTY_HINT_RANGE, "1,12,1")
	PUZZLE_BIND(gen_extra_crates, INT, PROPERTY_HINT_RANGE, "0,20,1")
	PUZZLE_BIND(gen_bombs, INT, PROPERTY_HINT_RANGE, "0,8,1")
	PUZZLE_BIND(gen_aliens, INT, PROPERTY_HINT_RANGE, "0,8,1")
	PUZZLE_BIND(gen_min_pushes, INT, PROPERTY_HINT_RANGE, "0,50,1")
	PUZZLE_BIND(gen_ring, INT, PROPERTY_HINT_ENUM, "Walls,Water (islands),Lava")
	PUZZLE_BIND(gen_gap_chance, FLOAT, PROPERTY_HINT_RANGE, "0,1,0.05")
	PUZZLE_BIND(gen_pools, INT, PROPERTY_HINT_RANGE, "0,12,1")
	PUZZLE_BIND(gen_ice_patches, INT, PROPERTY_HINT_RANGE, "0,6,1")
#undef PUZZLE_BIND
	ClassDB::bind_method(D_METHOD("get_generation_info"), &PuzzleGrid::get_generation_info);
	ClassDB::bind_method(D_METHOD("set_generation_info", "v"), &PuzzleGrid::set_generation_info);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "generation_info", PROPERTY_HINT_MULTILINE_TEXT, "",
						 PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY),
			"set_generation_info", "get_generation_info");
	ADD_GROUP("", "");
	ClassDB::bind_method(D_METHOD("set_use_topdown_camera", "v"), &PuzzleGrid::set_use_topdown_camera);
	ClassDB::bind_method(D_METHOD("get_use_topdown_camera"), &PuzzleGrid::get_use_topdown_camera);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_topdown_camera"), "set_use_topdown_camera", "get_use_topdown_camera");
	ClassDB::bind_method(D_METHOD("set_topdown_offset", "v"), &PuzzleGrid::set_topdown_offset);
	ClassDB::bind_method(D_METHOD("get_topdown_offset"), &PuzzleGrid::get_topdown_offset);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "topdown_offset", PROPERTY_HINT_NONE, "suffix:m"), "set_topdown_offset",
			"get_topdown_offset");
}

PuzzleGrid::PuzzleGrid() { layout = DEFAULT_LAYOUT; }

void PuzzleGrid::_ready() { _build(); }

void PuzzleGrid::set_layout(const String &p_layout) {
	layout = p_layout;
	if (is_inside_tree()) {
		_build();
	}
}

void PuzzleGrid::set_cell_size(float p_v) {
	cell_size = MAX(p_v, 0.1f);
	if (is_inside_tree()) {
		_build();
	}
}

void PuzzleGrid::set_wall_height(float p_v) {
	wall_height = MAX(p_v, 0.1f);
	if (is_inside_tree()) {
		_build();
	}
}

#define PUZZLE_REBUILD_SETTER(m_name, m_type, m_expr) \
	void PuzzleGrid::set_##m_name(m_type p_v) {          \
		m_name = m_expr;                                 \
		if (is_inside_tree()) {                          \
			_build();                                    \
		}                                                \
	}
PUZZLE_REBUILD_SETTER(procedural, bool, p_v)
PUZZLE_REBUILD_SETTER(seed, int, p_v)
PUZZLE_REBUILD_SETTER(gen_size, const Vector2i &, Vector2i(MAX(p_v.x, 9), MAX(p_v.y, 5)))
PUZZLE_REBUILD_SETTER(gen_rooms, int, CLAMP(p_v, 1, 12))
PUZZLE_REBUILD_SETTER(gen_extra_crates, int, CLAMP(p_v, 0, 20))
PUZZLE_REBUILD_SETTER(gen_bombs, int, CLAMP(p_v, 0, 8))
PUZZLE_REBUILD_SETTER(gen_aliens, int, CLAMP(p_v, 0, 8))
PUZZLE_REBUILD_SETTER(gen_min_pushes, int, CLAMP(p_v, 0, 50))
PUZZLE_REBUILD_SETTER(gen_ring, int, CLAMP(p_v, 0, 2))
PUZZLE_REBUILD_SETTER(gen_gap_chance, float, CLAMP(p_v, 0.0f, 1.0f))
PUZZLE_REBUILD_SETTER(gen_pools, int, CLAMP(p_v, 0, 12))
PUZZLE_REBUILD_SETTER(gen_ice_patches, int, CLAMP(p_v, 0, 6))
#undef PUZZLE_REBUILD_SETTER

// --- Build -------------------------------------------------------------------

void PuzzleGrid::_clear() {
	if (content) {
		remove_child(content);
		content->queue_free();
		content = nullptr;
	}
	boxes.clear();
	settling.clear();
	burning.clear();
	dynamic_body = nullptr; // freed with `content`
	spinners.clear();
	bobbers.clear();
	push_target = nullptr;
	push_timer = 0.0f;
}

void PuzzleGrid::_build() {
	_clear();
	_parse();
	content = memnew(Node3D);
	content->set_name("Built"); // no owner: regenerated from `layout`, never saved
	add_child(content);
	_build_floor_tiles();
	_build_bridges();
	_build_walls();
	_build_liquids();
	_build_static_collision();
	_rebuild_dynamic_collision();
	_build_items();
}

// The level text: the seeded generator's output when `procedural`, else the authored layout.
String PuzzleGrid::_source_layout() {
	if (!procedural) {
		generation_info = "authored layout";
		return layout;
	}
	SokobanGenerator::Params p;
	p.seed = (uint32_t)seed;
	p.width = gen_size.x;
	p.height = gen_size.y;
	p.rooms = gen_rooms;
	p.extra_crates = gen_extra_crates;
	p.bombs = gen_bombs;
	p.aliens = gen_aliens;
	p.min_pushes = gen_min_pushes;
	p.ring = gen_ring;
	p.gap_chance = gen_gap_chance;
	p.pools = gen_pools;
	p.ice_patches = gen_ice_patches;
	SokobanGenerator::Output out = SokobanGenerator::generate(p);
	if (!out.ok) {
		generation_info = vformat("seed %d: no solvable level in %d attempts (loosen the settings)", seed, out.attempts);
		UtilityFunctions::push_warning("PuzzleGrid: ", generation_info);
		return layout;
	}
	String text;
	for (const std::string &r : out.rows) {
		text += String(r.c_str()) + "\n";
	}
	generation_info = vformat("seed %d: key + exit in %d pushes, bomb to alien in %d, attempt %d, %d solver states", seed,
			out.pushes, out.bomb_pushes, out.attempts, out.states);
	return text;
}

void PuzzleGrid::_parse() {
	rows = _source_layout().replace("\r", "").split("\n", false);
	depth = rows.size();
	width = 0;
	for (const String &r : rows) {
		width = MAX(width, (int)r.length());
	}
	cells.assign((size_t)width * depth, ' ');
	for (int z = 0; z < depth; ++z) {
		const String &r = rows[z];
		for (int x = 0; x < (int)r.length(); ++x) {
			cells[(size_t)z * width + x] = from_layout(r[x]); // boxes are tracked separately
		}
	}
}

Vector3 PuzzleGrid::_cell_center(int p_x, int p_z) const {
	return Vector3((p_x + 0.5f) * cell_size, 0.0f, (p_z + 0.5f) * cell_size);
}

char PuzzleGrid::_cell(int p_x, int p_z) const {
	if (p_x < 0 || p_z < 0 || p_x >= width || p_z >= depth) {
		return ' ';
	}
	return cells[(size_t)p_z * width + p_x];
}

void PuzzleGrid::_build_items() {
	for (int z = 0; z < depth; ++z) {
		const String &r = rows[z];
		for (int x = 0; x < (int)r.length(); ++x) {
			const char32_t c = r[x];
			const Vector3 p = _cell_center(x, z) + Vector3(0, FLOOR_TOP, 0);
			if (c == 'B' || c == 'X') {
				PushBox *box = memnew(PushBox);
				box->set_name(vformat("%s_%d_%d", c == 'X' ? "BoxBomb" : "Box", x, z));
				box->setup(cell_size, c == 'X');
				box->cell_x = x;
				box->cell_z = z;
				content->add_child(box);
				box->set_position(p);
				boxes.push_back(box);
			} else if (c == 'K') {
				Node3D *k = PuzzleProps::key(content, cell_size);
				k->set_position(p);
				spinners.push_back(Object::cast_to<Node3D>(k->get_node_or_null("Body")));
			} else if (c == 'A') {
				Node3D *a = PuzzleProps::alien(content, cell_size);
				a->set_position(p);
				a->set_rotation(Vector3(0, Math::PI * 0.25f * ((x * 3 + z) % 8), 0)); // varied facing
				bobbers.push_back(Object::cast_to<Node3D>(a->get_node_or_null("Body")));
			} else if (c == 'E') {
				Node3D *d = PuzzleProps::exit_door(content, cell_size, wall_height);
				d->set_position(p);
				// Face along its wall / water ring: rotate when that runs along Z.
				const char n0 = _cell(x, z - 1);
				const char n1 = _cell(x, z + 1);
				if (is_solid(n0) || is_liquid(n0) || is_solid(n1) || is_liquid(n1)) {
					d->set_rotation(Vector3(0, Math::PI * 0.5f, 0));
				}
			}
		}
	}
}

// --- Runtime -----------------------------------------------------------------

void PuzzleGrid::_physics_process(double p_delta) {
	const float dt = (float)p_delta;
	_animate(dt);
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	_update_push(dt);
	_settle_boxes();
}

void PuzzleGrid::_animate(float p_dt) {
	anim_time += p_dt;
	for (size_t i = 0; i < spinners.size(); ++i) {
		Node3D *k = spinners[i];
		if (k) {
			k->set_rotation(Vector3(0, anim_time * 2.0f, 0));
			k->set_position(Vector3(0, cell_size * 0.55f + 0.08f * Math::sin(anim_time * 3.0f + i), 0));
		}
	}
	for (size_t i = 0; i < bobbers.size(); ++i) {
		Node3D *b = bobbers[i];
		if (b) {
			// Squash-and-stretch idle bounce, phase-shifted per alien.
			const float s = Math::sin(anim_time * 4.0f + i * 1.7f);
			b->set_scale(Vector3(1.0f + 0.06f * s, 1.0f - 0.08f * s, 1.0f + 0.06f * s));
		}
	}
}

} // namespace godot

#include "puzzle_props.h"

#include "../utils/fx/toy_mesh.h"

#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/core/math.hpp>

namespace godot {

namespace PuzzleProps {

static const Color WOOD = Color(0.78f, 0.55f, 0.32f);
static const Color WOOD_DARK = Color(0.5f, 0.32f, 0.18f);
static const Color BOMB_RED = Color(0.88f, 0.2f, 0.18f);
static const Color BOMB_BLACK = Color(0.12f, 0.12f, 0.15f);
static const Color GOLD = Color(1.0f, 0.8f, 0.2f);
static const Color ALIEN_GREEN = Color(0.45f, 0.9f, 0.35f);
static const Color WHITE = Color(0.97f, 0.97f, 0.95f);
static const Color STONE = Color(0.62f, 0.6f, 0.66f);

static Node3D *_root(Node3D *p_parent, const String &p_name) {
	Node3D *n = memnew(Node3D);
	n->set_name(p_name);
	p_parent->add_child(n);
	return n;
}

// Edge frame: 4 vertical posts + top and bottom rims, slightly proud of the faces.
static void _frame(Node3D *p_n, float p_s, const Ref<Material> &p_mat) {
	const float h = p_s * 0.5f;
	const float t = p_s * 0.1f;
	for (int i = 0; i < 4; ++i) {
		const float x = (i & 1) ? h : -h;
		const float z = (i & 2) ? h : -h;
		ToyMesh::add(p_n, ToyMesh::box(Vector3(t, p_s, t)), p_mat, Vector3(x, h, z));
	}
	for (int y = 0; y < 2; ++y) {
		const float yy = y ? p_s : 0.0f;
		ToyMesh::add(p_n, ToyMesh::box(Vector3(p_s + t, t, t)), p_mat, Vector3(0, yy, h));
		ToyMesh::add(p_n, ToyMesh::box(Vector3(p_s + t, t, t)), p_mat, Vector3(0, yy, -h));
		ToyMesh::add(p_n, ToyMesh::box(Vector3(t, t, p_s + t)), p_mat, Vector3(h, yy, 0));
		ToyMesh::add(p_n, ToyMesh::box(Vector3(t, t, p_s + t)), p_mat, Vector3(-h, yy, 0));
	}
}

Node3D *crate(Node3D *p_parent, float p_cell) {
	Node3D *n = _root(p_parent, "Crate");
	const float s = p_cell * 0.9f;
	ToyMesh::add(n, ToyMesh::box(Vector3(s, s, s) * 0.96f), ToyMesh::toon(WOOD), Vector3(0, s * 0.5f, 0));
	// Diagonal brace on each side face reads as "crate" from any angle.
	Ref<Material> dark = ToyMesh::toon(WOOD_DARK);
	const float brace = s * 1.25f;
	for (int i = 0; i < 4; ++i) {
		const float a = Math::PI * 0.5f * i;
		const Vector3 out = Vector3(Math::sin(a), 0, Math::cos(a)) * (s * 0.49f);
		ToyMesh::add(n, ToyMesh::box(Vector3(brace, s * 0.1f, s * 0.04f)), dark, out + Vector3(0, s * 0.5f, 0),
				Vector3(0, a, Math::PI * 0.25f));
	}
	_frame(n, s, dark);
	return n;
}

Node3D *bomb_crate(Node3D *p_parent, float p_cell) {
	Node3D *n = _root(p_parent, "BombCrate");
	const float s = p_cell * 0.9f;
	ToyMesh::add(n, ToyMesh::box(Vector3(s, s, s) * 0.96f), ToyMesh::toon(BOMB_RED), Vector3(0, s * 0.5f, 0));
	// Hazard band around the middle.
	ToyMesh::add(n, ToyMesh::box(Vector3(s * 0.99f, s * 0.18f, s * 0.99f)), ToyMesh::toon(Color(1.0f, 0.85f, 0.2f)),
			Vector3(0, s * 0.5f, 0));
	_frame(n, s, ToyMesh::toon(BOMB_BLACK));
	// Round cartoon bomb sitting on top, fuse + glowing spark.
	const float r = s * 0.28f;
	ToyMesh::add(n, ToyMesh::sphere(r, 14), ToyMesh::toon(BOMB_BLACK), Vector3(0, s + r * 0.85f, 0));
	ToyMesh::add(n, ToyMesh::cylinder(r * 0.3f, r * 0.3f, r * 0.3f, 8), ToyMesh::toon(STONE),
			Vector3(0, s + r * 1.85f, 0));
	ToyMesh::add(n, ToyMesh::cylinder(r * 0.07f, r * 0.07f, r * 0.6f, 6), ToyMesh::toon(WOOD_DARK),
			Vector3(r * 0.15f, s + r * 2.2f, 0), Vector3(0, 0, -0.5f));
	ToyMesh::add(n, ToyMesh::sphere(r * 0.18f, 8), ToyMesh::glow(Color(1.0f, 0.75f, 0.2f, 0.95f)),
			Vector3(r * 0.32f, s + r * 2.5f, 0));
	return n;
}

Node3D *key(Node3D *p_parent, float p_cell) {
	Node3D *n = _root(p_parent, "Key");
	Node3D *k = _root(n, "Body"); // the part that spins / bobs
	k->set_position(Vector3(0, p_cell * 0.55f, 0));
	Ref<Material> gold = ToyMesh::toon(GOLD);
	const float s = p_cell * 0.5f;
	Ref<TorusMesh> ring;
	ring.instantiate();
	ring->set_inner_radius(s * 0.18f);
	ring->set_outer_radius(s * 0.32f);
	ring->set_rings(12);
	ring->set_ring_segments(6);
	ToyMesh::add(k, ring, gold, Vector3(0, s * 0.45f, 0), Vector3(Math::PI * 0.5f, 0, 0));
	ToyMesh::add(k, ToyMesh::box(Vector3(s * 0.12f, s * 0.75f, s * 0.12f)), gold, Vector3(0, -s * 0.08f, 0));
	ToyMesh::add(k, ToyMesh::box(Vector3(s * 0.22f, s * 0.1f, s * 0.12f)), gold, Vector3(s * 0.12f, -s * 0.32f, 0));
	ToyMesh::add(k, ToyMesh::box(Vector3(s * 0.16f, s * 0.1f, s * 0.12f)), gold, Vector3(s * 0.1f, -s * 0.16f, 0));
	// Soft glow halo so it reads as a pickup from afar.
	ToyMesh::add(k, ToyMesh::sphere(s * 0.55f, 10), ToyMesh::glow(Color(1.0f, 0.9f, 0.4f, 0.18f)), Vector3(0, s * 0.2f, 0));
	return n;
}

Node3D *alien(Node3D *p_parent, float p_cell) {
	Node3D *n = _root(p_parent, "Alien");
	Node3D *b = _root(n, "Body"); // the part that bobs
	const float s = p_cell * 0.42f;
	Ref<Material> green = ToyMesh::toon(ALIEN_GREEN);
	Ref<Material> white = ToyMesh::toon(WHITE);
	Ref<Material> black = ToyMesh::toon(BOMB_BLACK);
	ToyMesh::add(b, ToyMesh::sphere(s, 14), green, Vector3(0, s, 0), Vector3(), Vector3(1.0f, 0.85f, 1.0f));
	ToyMesh::add(b, ToyMesh::sphere(s * 0.75f, 12), ToyMesh::toon(ALIEN_GREEN.darkened(0.15f)),
			Vector3(0, s * 0.35f, 0), Vector3(), Vector3(1.25f, 0.5f, 1.25f)); // squat foot skirt
	for (int side = -1; side <= 1; side += 2) {
		const float x = side * s * 0.38f;
		ToyMesh::add(b, ToyMesh::cylinder(s * 0.06f, s * 0.08f, s * 0.7f, 6), green, Vector3(x, s * 2.0f, 0),
				Vector3(0, 0, -side * 0.25f));
		const Vector3 eye(x * 1.35f, s * 2.4f, 0);
		ToyMesh::add(b, ToyMesh::sphere(s * 0.24f, 10), white, eye);
		ToyMesh::add(b, ToyMesh::sphere(s * 0.11f, 8), black, eye + Vector3(0, 0, s * 0.18f));
	}
	// Little grin.
	ToyMesh::add(b, ToyMesh::box(Vector3(s * 0.5f, s * 0.07f, s * 0.05f)), black, Vector3(0, s * 0.95f, s * 0.86f));
	return n;
}

Node3D *exit_door(Node3D *p_parent, float p_cell, float p_height) {
	Node3D *n = _root(p_parent, "ExitDoor");
	Ref<Material> stone = ToyMesh::toon(STONE);
	const float w = p_cell;
	const float h = MIN(p_height, p_cell * 1.6f);
	const float t = p_cell * 0.18f;
	ToyMesh::add(n, ToyMesh::box(Vector3(t, h, t * 1.4f)), stone, Vector3(-w * 0.5f + t * 0.5f, h * 0.5f, 0));
	ToyMesh::add(n, ToyMesh::box(Vector3(t, h, t * 1.4f)), stone, Vector3(w * 0.5f - t * 0.5f, h * 0.5f, 0));
	ToyMesh::add(n, ToyMesh::box(Vector3(w, t, t * 1.4f)), stone, Vector3(0, h - t * 0.5f, 0));
	ToyMesh::add(n, ToyMesh::box(Vector3(w - 2.0f * t, h - t, t * 0.5f)), ToyMesh::toon(WOOD_DARK),
			Vector3(0, (h - t) * 0.5f, 0));
	// Keyhole plate: says "needs the key".
	ToyMesh::add(n, ToyMesh::cylinder(t * 0.45f, t * 0.45f, t * 0.2f, 10), ToyMesh::toon(GOLD),
			Vector3(0, h * 0.45f, t * 0.3f), Vector3(Math::PI * 0.5f, 0, 0));
	return n;
}

} // namespace PuzzleProps

} // namespace godot

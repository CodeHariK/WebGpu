#include "procedural_rock.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/convex_polygon_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <cmath>

namespace godot {

struct SeededRand {
	uint32_t state;
	SeededRand(uint32_t s) : state(s) {}
	float rand_f() {
		state = state * 1664525 + 1013904223;
		return (float)(state & 0xFFFFFF) / 16777216.0f; // 0.0 to 1.0
	}
};

void ProceduralRock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_collision_type", "type"), &ProceduralRock::set_collision_type);
	ClassDB::bind_method(D_METHOD("get_collision_type"), &ProceduralRock::get_collision_type);

	ClassDB::bind_method(D_METHOD("set_rock_seed", "seed"), &ProceduralRock::set_rock_seed);
	ClassDB::bind_method(D_METHOD("get_rock_seed"), &ProceduralRock::get_rock_seed);

	ClassDB::bind_method(D_METHOD("set_rock_radius", "radius"), &ProceduralRock::set_rock_radius);
	ClassDB::bind_method(D_METHOD("get_rock_radius"), &ProceduralRock::get_rock_radius);

	ClassDB::bind_method(D_METHOD("set_rock_length", "length"), &ProceduralRock::set_rock_length);
	ClassDB::bind_method(D_METHOD("get_rock_length"), &ProceduralRock::get_rock_length);

	ClassDB::bind_method(D_METHOD("set_rock_noise", "noise"), &ProceduralRock::set_rock_noise);
	ClassDB::bind_method(D_METHOD("get_rock_noise"), &ProceduralRock::get_rock_noise);

	ClassDB::bind_method(D_METHOD("set_rock_slices", "slices"), &ProceduralRock::set_rock_slices);
	ClassDB::bind_method(D_METHOD("get_rock_slices"), &ProceduralRock::get_rock_slices);

	ClassDB::bind_method(D_METHOD("set_rock_points", "points"), &ProceduralRock::set_rock_points);
	ClassDB::bind_method(D_METHOD("get_rock_points"), &ProceduralRock::get_rock_points);

	ClassDB::bind_method(D_METHOD("generate_rock"), &ProceduralRock::generate_rock);
	ClassDB::bind_method(D_METHOD("update_collision"), &ProceduralRock::update_collision);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_type", PROPERTY_HINT_ENUM, "None,Convex,Concave"), "set_collision_type", "get_collision_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rock_seed"), "set_rock_seed", "get_rock_seed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rock_radius"), "set_rock_radius", "get_rock_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rock_length"), "set_rock_length", "get_rock_length");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rock_noise"), "set_rock_noise", "get_rock_noise");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rock_slices"), "set_rock_slices", "get_rock_slices");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rock_points"), "set_rock_points", "get_rock_points");

	BIND_ENUM_CONSTANT(COLLISION_NONE);
	BIND_ENUM_CONSTANT(COLLISION_CONVEX);
	BIND_ENUM_CONSTANT(COLLISION_CONCAVE);
}

ProceduralRock::ProceduralRock() {
	collision_type = COLLISION_CONVEX;
	rock_seed = 12345;
	rock_radius = 1.0f;
	rock_length = 2.0f;
	rock_noise = 0.2f;
	rock_slices = 5;
	rock_points = 8;
}

ProceduralRock::~ProceduralRock() {
}

void ProceduralRock::_ready() {
	ProceduralLofter::_ready();

	static_body = Object::cast_to<StaticBody3D>(get_node_or_null("StaticBody3D"));
	if (static_body) {
		collision_shape = Object::cast_to<CollisionShape3D>(static_body->get_node_or_null("CollisionShape3D"));
	}

	if (get_slices_array().size() == 0) {
		generate_rock();
	} else {
		update_collision();
	}
}

void ProceduralRock::update_loft() {
	ProceduralLofter::update_loft();
	update_collision();
}

void ProceduralRock::set_collision_type(CollisionType p_type) {
	if (collision_type != p_type) {
		collision_type = p_type;
		update_collision();
	}
}

ProceduralRock::CollisionType ProceduralRock::get_collision_type() const {
	return collision_type;
}

void ProceduralRock::set_rock_seed(int p_seed) {
	if (rock_seed != p_seed) {
		rock_seed = p_seed;
		generate_rock();
	}
}
int ProceduralRock::get_rock_seed() const { return rock_seed; }

void ProceduralRock::set_rock_radius(float p_radius) {
	if (rock_radius != p_radius) {
		rock_radius = p_radius;
		generate_rock();
	}
}
float ProceduralRock::get_rock_radius() const { return rock_radius; }

void ProceduralRock::set_rock_length(float p_length) {
	if (rock_length != p_length) {
		rock_length = p_length;
		generate_rock();
	}
}
float ProceduralRock::get_rock_length() const { return rock_length; }

void ProceduralRock::set_rock_noise(float p_noise) {
	if (rock_noise != p_noise) {
		rock_noise = p_noise;
		generate_rock();
	}
}
float ProceduralRock::get_rock_noise() const { return rock_noise; }

void ProceduralRock::set_rock_slices(int p_slices) {
	if (rock_slices != p_slices) {
		rock_slices = p_slices;
		if (rock_slices < 2) rock_slices = 2;
		generate_rock();
	}
}
int ProceduralRock::get_rock_slices() const { return rock_slices; }

void ProceduralRock::set_rock_points(int p_points) {
	if (rock_points != p_points) {
		rock_points = p_points;
		if (rock_points < 3) rock_points = 3;
		generate_rock();
	}
}
int ProceduralRock::get_rock_points() const { return rock_points; }

void ProceduralRock::generate_rock() {
	Ref<Curve3D> main_curve;
	main_curve.instantiate();

	SeededRand rng(rock_seed);

	// Generate path points along Z axis with slight random bends
	for (int i = 0; i < rock_slices; i++) {
		float pct = (rock_slices > 1) ? (float)i / (rock_slices - 1) : 0.5f;
		float z_pos = -pct * rock_length;
		float x_off = 0.0f;
		float y_off = 0.0f;

		// Add noise to the path for bent/organic rock shape (only for intermediate points)
		if (i > 0 && i < rock_slices - 1) {
			x_off = (rng.rand_f() * 2.0f - 1.0f) * rock_noise * rock_radius * 0.4f;
			y_off = (rng.rand_f() * 2.0f - 1.0f) * rock_noise * rock_radius * 0.4f;
		}

		main_curve->add_point(Vector3(x_off, y_off, z_pos));
	}

	set_curve(main_curve);

	TypedArray<Curve3D> new_slices;

	for (int i = 0; i < rock_slices; i++) {
		float pct = (rock_slices > 1) ? (float)i / (rock_slices - 1) : 0.5f;
		Ref<Curve3D> slice_curve;
		slice_curve.instantiate();

		// Taper the rock radius near the ends to form closed caps
		float taper = 0.15f + 0.85f * std::sin(3.14159265f * pct);

		for (int j = 0; j < rock_points; j++) {
			float angle = j * (2.0f * 3.14159265f / rock_points);
			float r = rock_radius * taper;

			// Add profile noise
			float noise = (rng.rand_f() * 2.0f - 1.0f) * rock_noise * rock_radius * taper;
			r += noise;
			if (r < 0.05f) r = 0.05f;

			// Point in X/Z plane (Y=0)
			slice_curve->add_point(Vector3(r * std::cos(angle), 0.0f, r * std::sin(angle)));
		}

		new_slices.push_back(slice_curve);
	}

	set_slices_array(new_slices);
}

void ProceduralRock::update_collision() {
	if (collision_type == COLLISION_NONE) {
		if (static_body) {
			static_body->queue_free();
			static_body = nullptr;
			collision_shape = nullptr;
		}
		return;
	}

	if (!static_body) {
		static_body = Object::cast_to<StaticBody3D>(get_node_or_null("StaticBody3D"));
		if (!static_body) {
			static_body = memnew(StaticBody3D);
			static_body->set_name("StaticBody3D");
			add_child(static_body);
			if (get_owner()) {
				static_body->set_owner(get_owner());
			}
		}
	}

	if (!collision_shape) {
		collision_shape = Object::cast_to<CollisionShape3D>(static_body->get_node_or_null("CollisionShape3D"));
		if (!collision_shape) {
			collision_shape = memnew(CollisionShape3D);
			collision_shape->set_name("CollisionShape3D");
			static_body->add_child(collision_shape);
			if (get_owner()) {
				collision_shape->set_owner(get_owner());
			}
		}
	}

	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(get_node_or_null("MeshInstance3D"));
	if (!mi) {
		collision_shape->set_shape(Ref<Shape3D>());
		return;
	}

	Ref<Mesh> mesh = mi->get_mesh();
	if (mesh.is_null()) {
		collision_shape->set_shape(Ref<Shape3D>());
		return;
	}

	if (collision_type == COLLISION_CONVEX) {
		Ref<ConvexPolygonShape3D> convex_shape = mesh->create_convex_shape(true, false);
		collision_shape->set_shape(convex_shape);
	} else if (collision_type == COLLISION_CONCAVE) {
		Ref<ConcavePolygonShape3D> concave_shape = mesh->create_trimesh_shape();
		collision_shape->set_shape(concave_shape);
	}
}

} // namespace godot

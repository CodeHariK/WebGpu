#ifndef PROCEDURAL_ROCK_H
#define PROCEDURAL_ROCK_H

#include "procedural_lofter.h"
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>

namespace godot {

class ProceduralRock : public ProceduralLofter {
	GDCLASS(ProceduralRock, ProceduralLofter)

public:
	enum CollisionType {
		COLLISION_NONE = 0,
		COLLISION_CONVEX = 1,
		COLLISION_CONCAVE = 2
	};

private:
	CollisionType collision_type = COLLISION_CONVEX;
	StaticBody3D *static_body = nullptr;
	CollisionShape3D *collision_shape = nullptr;

	int rock_seed = 12345;
	float rock_radius = 1.0f;
	float rock_length = 2.0f;
	float rock_noise = 0.2f;
	int rock_slices = 5;
	int rock_points = 8;

	void update_collision();

protected:
	static void _bind_methods();

public:
	ProceduralRock();
	~ProceduralRock();

	virtual void _ready() override;
	virtual void update_loft() override;

	void set_collision_type(CollisionType p_type);
	CollisionType get_collision_type() const;

	void set_rock_seed(int p_seed);
	int get_rock_seed() const;

	void set_rock_radius(float p_radius);
	float get_rock_radius() const;

	void set_rock_length(float p_length);
	float get_rock_length() const;

	void set_rock_noise(float p_noise);
	float get_rock_noise() const;

	void set_rock_slices(int p_slices);
	int get_rock_slices() const;

	void set_rock_points(int p_points);
	int get_rock_points() const;

	void generate_rock();
};

} // namespace godot

VARIANT_ENUM_CAST(godot::ProceduralRock::CollisionType);

#endif // PROCEDURAL_ROCK_H

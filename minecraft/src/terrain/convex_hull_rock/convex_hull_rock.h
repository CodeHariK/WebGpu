#ifndef CONVEX_HULL_ROCK_H
#define CONVEX_HULL_ROCK_H

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/material.hpp>

namespace godot {

class ConvexHullRock : public MeshInstance3D {
	GDCLASS(ConvexHullRock, MeshInstance3D)

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
	int num_points = 30;
	Vector3 size_scale = Vector3(1.0f, 1.0f, 1.0f);
	bool flat_shaded = true;

	void update_collision();

protected:
	static void _bind_methods();

public:
	ConvexHullRock();
	~ConvexHullRock();

	virtual void _ready() override;

	void set_collision_type(CollisionType p_type);
	CollisionType get_collision_type() const;

	void set_rock_seed(int p_seed);
	int get_rock_seed() const;

	void set_num_points(int p_points);
	int get_num_points() const;

	void set_size_scale(const Vector3 &p_scale);
	Vector3 get_size_scale() const;

	void set_flat_shaded(bool p_flat);
	bool get_flat_shaded() const;

	void generate_rock();
};

} // namespace godot

VARIANT_ENUM_CAST(godot::ConvexHullRock::CollisionType);

#endif // CONVEX_HULL_ROCK_H

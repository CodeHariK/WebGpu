#include "projectile_launcher.h"

#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/object.hpp>

namespace godot {

void ProjectileLauncher::_bind_methods() {
	ClassDB::bind_method(D_METHOD("fire", "target"), &ProjectileLauncher::fire);
	ClassDB::bind_method(D_METHOD("fire_velocity", "velocity"), &ProjectileLauncher::fire_velocity);
	ClassDB::bind_method(D_METHOD("set_profile", "profile"), &ProjectileLauncher::set_profile);
	ClassDB::bind_method(D_METHOD("get_profile"), &ProjectileLauncher::get_profile);
	ClassDB::bind_method(D_METHOD("set_max_alive", "max"), &ProjectileLauncher::set_max_alive);
	ClassDB::bind_method(D_METHOD("get_max_alive"), &ProjectileLauncher::get_max_alive);
	ClassDB::bind_method(D_METHOD("set_debug_draw", "on"), &ProjectileLauncher::set_debug_draw);
	ClassDB::bind_method(D_METHOD("get_debug_draw"), &ProjectileLauncher::get_debug_draw);
	ClassDB::bind_method(D_METHOD("get_alive_count"), &ProjectileLauncher::get_alive_count);

	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "profile", PROPERTY_HINT_RESOURCE_TYPE, "ProjectileProfile"),
			"set_profile",
			"get_profile"
	);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_alive", PROPERTY_HINT_RANGE, "1,20,1"), "set_max_alive", "get_max_alive");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_draw"), "set_debug_draw", "get_debug_draw");
}

int ProjectileLauncher::_prune_alive() {
	size_t w = 0;
	for (size_t i = 0; i < alive.size(); i++) {
		if (ObjectDB::get_instance(alive[i])) {
			alive[w++] = alive[i];
		}
	}
	alive.resize(w);
	return (int)alive.size();
}

Projectile *ProjectileLauncher::_spawn() {
	if (!is_inside_tree() || _prune_alive() >= max_alive) {
		return nullptr;
	}
	if (profile.is_null()) {
		profile.instantiate(); // defaults = boss missile
	}
	// Shots live in the scene, not under the launcher: they must outlive a dying shooter
	// and must not inherit its movement.
	SceneTree *tree = get_tree();
	Node *world = tree->get_current_scene() ? tree->get_current_scene() : (Node *)tree->get_root();

	Projectile *shot = memnew(Projectile);
	shot->set_profile(profile);
	shot->set_debug_draw(debug_draw);
	world->add_child(shot);
	alive.push_back(shot->get_instance_id());
	return shot;
}

Projectile *ProjectileLauncher::fire(Node3D *p_target) {
	Projectile *shot = _spawn();
	if (!shot) {
		return nullptr;
	}
	Vector3 origin = get_global_position();
	Vector3 dir = -get_global_basis().get_column(2);
	if (p_target) {
		dir = p_target->get_global_position() - origin;
	}
	dir = dir.normalized() + Vector3(0, profile->get_launch_lift(), 0);
	shot->fire(origin, dir, p_target, Object::cast_to<Node3D>(get_parent()));
	return shot;
}

Projectile *ProjectileLauncher::fire_velocity(const Vector3 &p_velocity) {
	Projectile *shot = _spawn();
	if (shot) {
		shot->fire_velocity(get_global_position(), p_velocity, Object::cast_to<Node3D>(get_parent()));
	}
	return shot;
}

} // namespace godot

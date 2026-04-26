#include "enemy_base.h"
#include "enemy_manager.h"
#include <godot_cpp/classes/engine.hpp>

namespace godot {

void EnemyBase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enemy_kind", "kind"), &EnemyBase::set_enemy_kind);
	ClassDB::bind_method(D_METHOD("get_enemy_kind"), &EnemyBase::get_enemy_kind);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "enemy_kind"), "set_enemy_kind", "get_enemy_kind");
}

EnemyBase::EnemyBase() {}
EnemyBase::~EnemyBase() {}

void EnemyBase::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		set_physics_process(false);
		set_process(false);
		return;
	}

	EnemyManager *em = EnemyManager::get_singleton();
	if (em) {
		em->register_enemy(this, enemy_kind);
	}
}

void EnemyBase::_exit_tree() {
	EnemyManager *em = EnemyManager::get_singleton();
	if (em) {
		em->unregister_enemy(this);
	}
}

void EnemyBase::take_damage(float p_amount) {
	if (is_dead)
		return;
	health -= p_amount;
	if (health <= 0) {
		die();
	}
}

void EnemyBase::die() {
	if (is_dead)
		return;
	is_dead = true;
	queue_free();
}

} // namespace godot

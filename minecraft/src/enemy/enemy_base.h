#ifndef ENEMY_BASE_H
#define ENEMY_BASE_H

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class EnemyBase : public CharacterBody3D {
	GDCLASS(EnemyBase, CharacterBody3D)

private:
	String enemy_kind = "Thug";

protected:
	static void _bind_methods();

public:
	EnemyBase();
	~EnemyBase();

	void _ready() override;
	void _exit_tree() override;

	void set_enemy_kind(const String &p_kind) { enemy_kind = p_kind; }
	String get_enemy_kind() const { return enemy_kind; }
};

} // namespace godot

#endif // ENEMY_BASE_H

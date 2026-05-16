#ifndef TENNIS_MANAGER_H
#define TENNIS_MANAGER_H

#include "tennis_types.h"
#include <godot_cpp/classes/node3d.hpp>

namespace godot {
class TennisBall;
class TennisPlayer;

class TennisManager : public Node3D {
	GDCLASS(TennisManager, Node3D)

private:
	void _spawn_court();
	void _spawn_walls();
	void _spawn_ball();
	void _spawn_human_player();
	void _spawn_ai_player();
	void _create_wall(Vector3 size, Vector3 position, String name);

	TennisBall *ball_ref = nullptr;
	TennisPlayer *player1 = nullptr;
	TennisPlayer *player2 = nullptr;

	// Court configuration
	float court_width = 30.0f;
	float court_length = 40.0f;
	float wall_height = 10.0f;
	float wall_thickness = 0.5f;

	// Gameplay configuration
	float serve_speed = 50.0f;

protected:
	static void _bind_methods();

public:
	TennisManager();
	~TennisManager();

	void _ready() override;
	void _process(double delta) override;

	TennisBall *get_ball() const { return ball_ref; }
	void register_player(TennisPlayer *p_player);

	// Getters/Setters for properties
	void set_court_width(float p_val) { court_width = p_val; }
	float get_court_width() const { return court_width; }

	void set_court_length(float p_val) { court_length = p_val; }
	float get_court_length() const { return court_length; }

	void set_wall_height(float p_val) { wall_height = p_val; }
	float get_wall_height() const { return wall_height; }

	void set_serve_speed(float p_val) { serve_speed = p_val; }
	float get_serve_speed() const { return serve_speed; }
};

} // namespace godot

#endif // TENNIS_MANAGER_H

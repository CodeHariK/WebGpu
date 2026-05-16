#ifndef TENNIS_AI_PLAYER_H
#define TENNIS_AI_PLAYER_H

#include "tennis_player.h"

namespace godot {

class TennisAIPlayer : public TennisPlayer {
	GDCLASS(TennisAIPlayer, TennisPlayer)

private:
	TennisBall *ball_ref = nullptr;
	float reaction_speed = 5.0f;

protected:
	static void _bind_methods();

public:
	TennisAIPlayer();
	~TennisAIPlayer();

	void _ready() override;
	void _physics_process(double delta) override;

	void set_ball(TennisBall *p_ball) { ball_ref = p_ball; }
};

} // namespace godot

#endif // TENNIS_AI_PLAYER_H

#ifndef TENNIS_AI_H
#define TENNIS_AI_H

#include "tennis_player.h"

namespace godot {

class TennisAI : public TennisPlayer {
	GDCLASS(TennisAI, TennisPlayer)

private:
	Node3D *ball_reference;
	Vector3 predicted_landing_spot;
	bool is_tracking_ball;

protected:
	static void _bind_methods();

public:
	TennisAI();
	~TennisAI();

	void _ready() override;
	void _physics_process(double delta) override;

	void set_ball(Node3D *p_ball);
	void predict_ball_landing();
};

} // namespace godot

#endif // TENNIS_AI_H

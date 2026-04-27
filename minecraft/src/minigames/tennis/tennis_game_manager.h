#ifndef TENNIS_GAME_MANAGER_H
#define TENNIS_GAME_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include "tennis_ball.h"
#include "tennis_player.h"

namespace godot {

class TennisGameManager : public Node {
	GDCLASS(TennisGameManager, Node)

public:
	enum GameState {
		WAITING_FOR_SERVE,
		RALLY,
		POINT_OVER
	};

private:
	GameState current_state;
	int player_score;
	int ai_score;
	
	TennisBall *ball;
	TennisPlayer *player;
	TennisPlayer *ai;

protected:
	static void _bind_methods();

public:
	TennisGameManager();
	~TennisGameManager();

	void _ready() override;
	void _process(double delta) override;

	void set_ball(Node *p_ball);
	void set_player(Node *p_player);
	void set_ai(Node *p_ai);
	
	void start_serve();
	void add_point(bool p_player_won);
	
	String get_score_string(int p_score) const;
};

} // namespace godot

VARIANT_ENUM_CAST(godot::TennisGameManager::GameState);

#endif // TENNIS_GAME_MANAGER_H

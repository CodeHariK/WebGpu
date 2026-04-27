#ifndef TENNIS_BALL_H
#define TENNIS_BALL_H

#include <godot_cpp/classes/character_body3d.hpp>

namespace godot {

class TennisBall : public CharacterBody3D {
	GDCLASS(TennisBall, CharacterBody3D)

public:
	enum SpinType {
		FLAT,
		TOPSPIN,
		SLICE,
		LOB
	};

private:
	Vector3 current_velocity;
	Vector3 start_pos;
	Vector3 target_pos;
	
	float flight_time;
	float current_time;
	
	bool is_active;
	int bounce_count;
	SpinType current_spin;

protected:
	static void _bind_methods();

public:
	TennisBall();
	~TennisBall();

	void _physics_process(double delta) override;

	void hit(const Vector3 &p_target_pos, float p_flight_time, SpinType p_spin);
	void serve(const Vector3 &p_target_pos, float p_flight_time);
	
	bool get_is_active() const { return is_active; }
	int get_bounce_count() const { return bounce_count; }
	void reset_ball(const Vector3 &p_pos);
};

} // namespace godot

VARIANT_ENUM_CAST(godot::TennisBall::SpinType);

#endif // TENNIS_BALL_H

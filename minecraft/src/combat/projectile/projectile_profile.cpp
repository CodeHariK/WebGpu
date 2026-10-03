#include "projectile_profile.h"

namespace godot {

/// Binds setter/getter and exposes the field as an editor property.
#define BIND_FIELD(m_variant, m_name, ...)                                                      \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &ProjectileProfile::set_##m_name); \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &ProjectileProfile::get_##m_name);           \
	ADD_PROPERTY(PropertyInfo(Variant::m_variant, #m_name, ##__VA_ARGS__), "set_" #m_name, "get_" #m_name);

void ProjectileProfile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_ballistic_gravity"), &ProjectileProfile::get_ballistic_gravity);

	BIND_ENUM_CONSTANT(GUIDANCE_HOMING);
	BIND_ENUM_CONSTANT(GUIDANCE_BALLISTIC);
	BIND_ENUM_CONSTANT(WOBBLE_SHAKE);
	BIND_ENUM_CONSTANT(WOBBLE_CORKSCREW);
	BIND_ENUM_CONSTANT(STYLE_MISSILE);
	BIND_ENUM_CONSTANT(STYLE_ARROW);

	ADD_GROUP("Flight", "");
	BIND_FIELD(INT, guidance, PROPERTY_HINT_ENUM, "Homing,Ballistic")
	BIND_FIELD(FLOAT, speed, PROPERTY_HINT_RANGE, "1,40,0.1,suffix:m/s")
	BIND_FIELD(FLOAT, turn_rate, PROPERTY_HINT_RANGE, "0,720,1,suffix:deg/s")
	BIND_FIELD(FLOAT, lead, PROPERTY_HINT_RANGE, "0,1,0.05")
	BIND_FIELD(FLOAT, max_lead_time, PROPERTY_HINT_RANGE, "0,5,0.05,suffix:s")
	BIND_FIELD(FLOAT, launch_time, PROPERTY_HINT_RANGE, "0,2,0.01,suffix:s")
	BIND_FIELD(FLOAT, launch_lift, PROPERTY_HINT_RANGE, "0,4,0.05")
	BIND_FIELD(FLOAT, launch_gravity, PROPERTY_HINT_RANGE, "0,30,0.1,suffix:m/s²")
	BIND_FIELD(FLOAT, commit_distance, PROPERTY_HINT_RANGE, "0,15,0.1,suffix:m")
	BIND_FIELD(FLOAT, lose_lock_angle, PROPERTY_HINT_RANGE, "10,180,1,suffix:deg")
	BIND_FIELD(FLOAT, gravity, PROPERTY_HINT_RANGE, "0,30,0.1,suffix:m/s²")
	BIND_FIELD(FLOAT, min_flight_time, PROPERTY_HINT_RANGE, "0.2,5,0.05,suffix:s")
	BIND_FIELD(FLOAT, lifetime, PROPERTY_HINT_RANGE, "0.5,20,0.1,suffix:s")

	ADD_GROUP("Hit & Parry", "");
	BIND_FIELD(FLOAT, hit_radius, PROPERTY_HINT_RANGE, "0.1,3,0.05,suffix:m")
	BIND_FIELD(FLOAT, damage, PROPERTY_HINT_RANGE, "0,10,0.1")
	BIND_FIELD(FLOAT, blast_radius, PROPERTY_HINT_RANGE, "0,10,0.1,suffix:m")
	BIND_FIELD(BOOL, parryable)
	BIND_FIELD(FLOAT, parry_radius, PROPERTY_HINT_RANGE, "0.5,5,0.05,suffix:m")
	BIND_FIELD(FLOAT, parry_speed_scale, PROPERTY_HINT_RANGE, "0.5,4,0.05")

	ADD_GROUP("Look", "");
	BIND_FIELD(INT, style, PROPERTY_HINT_ENUM, "Missile,Arrow")
	BIND_FIELD(INT, wobble_style, PROPERTY_HINT_ENUM, "Shake,Corkscrew")
	BIND_FIELD(FLOAT, wobble_amp, PROPERTY_HINT_RANGE, "0,1,0.01")
	BIND_FIELD(FLOAT, wobble_freq, PROPERTY_HINT_RANGE, "0,10,0.1")
	BIND_FIELD(FLOAT, model_scale, PROPERTY_HINT_RANGE, "0.2,4,0.05")
	BIND_FIELD(COLOR, body_color)
	BIND_FIELD(BOOL, show_telegraph)
}

#undef BIND_FIELD

} // namespace godot

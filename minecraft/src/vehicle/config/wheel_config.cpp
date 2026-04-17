#include "wheel_config.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void WheelConfig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_hardpoint_offset", "offset"), &WheelConfig::set_hardpoint_offset);
	ClassDB::bind_method(D_METHOD("get_hardpoint_offset"), &WheelConfig::get_hardpoint_offset);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "hardpoint_offset"), "set_hardpoint_offset", "get_hardpoint_offset");

	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &WheelConfig::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &WheelConfig::get_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius"), "set_radius", "get_radius");

	ClassDB::bind_method(D_METHOD("set_suspension_rest_length", "length"), &WheelConfig::set_suspension_rest_length);
	ClassDB::bind_method(D_METHOD("get_suspension_rest_length"), &WheelConfig::get_suspension_rest_length);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "suspension_rest_length"), "set_suspension_rest_length", "get_suspension_rest_length");

	ClassDB::bind_method(D_METHOD("set_suspension_stiffness", "stiffness"), &WheelConfig::set_suspension_stiffness);
	ClassDB::bind_method(D_METHOD("get_suspension_stiffness"), &WheelConfig::get_suspension_stiffness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "suspension_stiffness"), "set_suspension_stiffness", "get_suspension_stiffness");

	ClassDB::bind_method(D_METHOD("set_compression_damping", "damping"), &WheelConfig::set_compression_damping);
	ClassDB::bind_method(D_METHOD("get_compression_damping"), &WheelConfig::get_compression_damping);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "compression_damping"), "set_compression_damping", "get_compression_damping");

	ClassDB::bind_method(D_METHOD("set_rebound_damping", "damping"), &WheelConfig::set_rebound_damping);
	ClassDB::bind_method(D_METHOD("get_rebound_damping"), &WheelConfig::get_rebound_damping);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rebound_damping"), "set_rebound_damping", "get_rebound_damping");
}

WheelConfig::WheelConfig() {
}

WheelConfig::~WheelConfig() {
}

void WheelConfig::set_hardpoint_offset(const Vector3 &p_offset) {
	hardpoint_offset = p_offset;
}

Vector3 WheelConfig::get_hardpoint_offset() const {
	return hardpoint_offset;
}

void WheelConfig::set_radius(float p_radius) {
	radius = p_radius;
}

float WheelConfig::get_radius() const {
	return radius;
}

void WheelConfig::set_suspension_rest_length(float p_length) {
	suspension_rest_length = p_length;
}

float WheelConfig::get_suspension_rest_length() const {
	return suspension_rest_length;
}

void WheelConfig::set_suspension_stiffness(float p_stiffness) {
	suspension_stiffness = p_stiffness;
}

float WheelConfig::get_suspension_stiffness() const {
	return suspension_stiffness;
}

void WheelConfig::set_compression_damping(float p_damping) {
	compression_damping = p_damping;
}

float WheelConfig::get_compression_damping() const {
	return compression_damping;
}

void WheelConfig::set_rebound_damping(float p_damping) {
	rebound_damping = p_damping;
}

float WheelConfig::get_rebound_damping() const {
	return rebound_damping;
}

} // namespace godot

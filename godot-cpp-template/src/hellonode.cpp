#include "hellonode.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void HelloNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_speed"), &HelloNode::get_speed);
	ClassDB::bind_method(D_METHOD("set_speed", "speed"), &HelloNode::set_speed);

	ClassDB::add_property("HelloNode", PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");

	ClassDB::bind_method(D_METHOD("hello_world", "words"), &HelloNode::hello_world);

	ADD_SIGNAL(MethodInfo("hello_world_signal", PropertyInfo(Variant::STRING, "data")));
}

HelloNode::HelloNode() {
	if (Engine::get_singleton()->is_editor_hint()) {
		set_process_mode(Node::ProcessMode::PROCESS_MODE_DISABLED);
	}
	UtilityFunctions::print("Hello world");
}

HelloNode::~HelloNode() {
}

void HelloNode::hello_world(String words) {
	UtilityFunctions::print(words + "Eternal");
	emit_signal("hello_world_signal", "hello data");
}

void HelloNode::_process(double delta) {
	UtilityFunctions::print("Hello process");

	velocity = Vector2(0, 0);

	Input &input_singleton = *Input::get_singleton();

	if (input_singleton.is_action_pressed("ui_up")) {
		velocity.y -= 1;
	}
	if (input_singleton.is_action_pressed("ui_left")) {
		velocity.x -= 1;
	}
	if (input_singleton.is_action_pressed("ui_down")) {
		velocity.y += 1;
	}
	if (input_singleton.is_action_pressed("ui_right")) {
		velocity.x += 1;
	}

	set_position(get_position() + velocity * speed * delta);
}

void HelloNode::set_speed(const double speed) {
	this->speed = speed;
}

double HelloNode::get_speed() const {
	return speed;
}
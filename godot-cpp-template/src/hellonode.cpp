#include "hellonode.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "include/celebi_parse.hpp"
#include "include/json.hpp"
#include "include/min_heap.hpp"

using namespace godot;

using json = nlohmann::json;

void HelloNode::_bind_methods() {
	ClassDB::bind_method(
			D_METHOD("get_speed"),
			&HelloNode::get_speed);
	ClassDB::bind_method(
			D_METHOD("set_speed", "speed"),
			&HelloNode::set_speed);

	ClassDB::add_property("HelloNode",
			PropertyInfo(Variant::FLOAT, "speed"),
			"set_speed", "get_speed");

	ClassDB::bind_method(
			D_METHOD("hello_world", "words"),
			&HelloNode::hello_world);

	ADD_SIGNAL(MethodInfo("hello_world_signal", PropertyInfo(Variant::STRING, "data")));
}

HelloNode::HelloNode() {
	if (Engine::get_singleton()->is_editor_hint()) {
		set_process_mode(Node::ProcessMode::PROCESS_MODE_DISABLED);
	}
	UtilityFunctions::print("Hello world");

	minHeapTest();
	jsonTest();
}

HelloNode::~HelloNode() {
}

void HelloNode::hello_world(String words) {
	UtilityFunctions::print(words + "Eternal");
	emit_signal("hello_world_signal", "hello data");
}

void HelloNode::_process(double delta) {
	// UtilityFunctions::print("Hello process");

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

void HelloNode::minHeapTest() {
	MinHeap<std::string> pq;

	pq.insert("apple", 5);
	pq.insert("banana", 2);
	pq.insert("cherry", 8);

	godot::UtilityFunctions::print(String("Peek: ") + pq.peek().value().c_str()); // banana

	if (!pq.updatePriority("apple", 1)) {
		godot::UtilityFunctions::print("Item not found in priority queue");
	}

	for (const auto &node : pq.getElements()) {
		godot::UtilityFunctions::print(String("item=") + node.item.c_str() + " priority=" + String::num_real(node.priority));
	}

	godot::UtilityFunctions::print(String("ExtractMin: ") + pq.extractMin().value().c_str()); // apple
	godot::UtilityFunctions::print(String("ExtractMin: ") + pq.extractMin().value().c_str()); // banana
	godot::UtilityFunctions::print(String("ExtractMin: ") + pq.extractMin().value().c_str()); // cherry
}

void HelloNode::jsonTest() {
	std::string planetJsonStr = R"(
		{
			"planets": ["Mercury", "Venus", "Earth", "Mars",
						"Jupiter", "Uranus", "Neptune"]
		}
	)";

	// parse without exceptions
	json j = json::parse(planetJsonStr, nullptr, false);

	if (j.is_discarded()) {
		godot::UtilityFunctions::print("Failed to parse JSON!");
	} else {
		godot::UtilityFunctions::print(String("Parsed planets: ") + j.dump(2).c_str());
	}

	// ---- Load and parse library.celebi without exceptions ----
	Ref<FileAccess> f = FileAccess::open("res://data/library.json", FileAccess::READ);
	if (f.is_null()) {
		UtilityFunctions::print("Could not open library.json");
		return;
	}

	String content = f->get_as_text();
	json libraryJson = json::parse(std::string(content.utf8().get_data()), nullptr, false);

	if (libraryJson.is_discarded()) {
		UtilityFunctions::print("Parse error: invalid JSON in library.json");
		return;
	}

	Celebi::Data d1 = libraryJson.get<Celebi::Data>();
	UtilityFunctions::print(String("library.json item count: ") + String::num_int64(d1.items.size()));
	for (const auto &item : d1.items) {
		UtilityFunctions::print(String(item.obj.c_str()));
	}
}

#include "bt_player.h"

namespace godot {

void BTPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_bt_tree", "tree"), &BTPlayer::set_bt_tree);
	ClassDB::bind_method(D_METHOD("get_bt_tree"), &BTPlayer::get_bt_tree);
	
	ClassDB::bind_method(D_METHOD("set_blackboard", "blackboard"), &BTPlayer::set_blackboard);
	ClassDB::bind_method(D_METHOD("get_blackboard"), &BTPlayer::get_blackboard);

	ClassDB::bind_method(D_METHOD("set_agent", "agent"), &BTPlayer::set_agent);
	ClassDB::bind_method(D_METHOD("get_agent"), &BTPlayer::get_agent);

	ClassDB::bind_method(D_METHOD("set_active", "active"), &BTPlayer::set_active);
	ClassDB::bind_method(D_METHOD("is_active"), &BTPlayer::is_active);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "bt_tree", PROPERTY_HINT_RESOURCE_TYPE, "BTTask"), "set_bt_tree", "get_bt_tree");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "blackboard", PROPERTY_HINT_RESOURCE_TYPE, "Blackboard"), "set_blackboard", "get_blackboard");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "active"), "set_active", "is_active");
}

BTPlayer::BTPlayer() {
	blackboard.instantiate();
}

BTPlayer::~BTPlayer() {}

void BTPlayer::set_bt_tree(const Ref<BTTask> &p_tree) {
	bt_tree = p_tree;
}

Ref<BTTask> BTPlayer::get_bt_tree() const {
	return bt_tree;
}

void BTPlayer::set_blackboard(const Ref<Blackboard> &p_blackboard) {
	blackboard = p_blackboard;
}

Ref<Blackboard> BTPlayer::get_blackboard() const {
	return blackboard;
}

void BTPlayer::set_agent(Node *p_agent) {
	agent = p_agent;
}

Node *BTPlayer::get_agent() const {
	return agent;
}

void BTPlayer::_physics_process(double delta) {
	if (!active || bt_tree.is_null()) return;

	Node *current_agent = agent ? agent : get_parent();
	if (!current_agent) return;

	bt_tree->execute(current_agent, blackboard);
}

} // namespace godot

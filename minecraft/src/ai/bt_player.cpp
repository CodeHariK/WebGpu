#include "bt_player.h"

namespace godot {

void BTPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_bt_tree", "tree"), &BTPlayer::set_bt_tree);
	ClassDB::bind_method(D_METHOD("get_bt_tree"), &BTPlayer::get_bt_tree);

	ClassDB::bind_method(D_METHOD("set_btstore", "btstore"), &BTPlayer::set_btstore);
	ClassDB::bind_method(D_METHOD("get_btstore"), &BTPlayer::get_btstore);

	ClassDB::bind_method(D_METHOD("set_agent", "agent"), &BTPlayer::set_agent);
	ClassDB::bind_method(D_METHOD("get_agent"), &BTPlayer::get_agent);

	ClassDB::bind_method(D_METHOD("set_active", "active"), &BTPlayer::set_active);
	ClassDB::bind_method(D_METHOD("is_active"), &BTPlayer::is_active);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "bt_tree", PROPERTY_HINT_RESOURCE_TYPE, "BTTask"), "set_bt_tree", "get_bt_tree");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "btstore", PROPERTY_HINT_RESOURCE_TYPE, "btstore"), "set_btstore", "get_btstore");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "active"), "set_active", "is_active");
}

BTPlayer::BTPlayer() {
	btstore.instantiate();
}

BTPlayer::~BTPlayer() {}

void BTPlayer::set_bt_tree(const Ref<BTTask> &p_tree) {
	bt_tree = p_tree;
}

Ref<BTTask> BTPlayer::get_bt_tree() const {
	return bt_tree;
}

void BTPlayer::set_btstore(const Ref<BTStore> &p_btstore) {
	btstore = p_btstore;
}

Ref<BTStore> BTPlayer::get_btstore() const {
	return btstore;
}

void BTPlayer::set_agent(Node *p_agent) {
	agent = p_agent;
}

Node *BTPlayer::get_agent() const {
	return agent;
}

void BTPlayer::_physics_process(double delta) {
	if (!active || bt_tree.is_null())
		return;

	Node *current_agent = agent ? agent : get_parent();
	if (!current_agent)
		return;

	bt_tree->execute(current_agent, btstore);
}

} // namespace godot

#ifndef BT_PLAYER_H
#define BT_PLAYER_H

#include <godot_cpp/classes/node.hpp>
#include "bt_task.h"
#include "blackboard.h"

namespace godot {

class BTPlayer : public Node {
	GDCLASS(BTPlayer, Node)

private:
	Ref<BTTask> bt_tree;
	Ref<Blackboard> blackboard;
	Node *agent = nullptr;
	bool active = true;

protected:
	static void _bind_methods();

public:
	BTPlayer();
	~BTPlayer();

	void set_bt_tree(const Ref<BTTask> &p_tree);
	Ref<BTTask> get_bt_tree() const;

	void set_blackboard(const Ref<Blackboard> &p_blackboard);
	Ref<Blackboard> get_blackboard() const;

	void set_agent(Node *p_agent);
	Node *get_agent() const;

	void set_active(bool p_active) { active = p_active; }
	bool is_active() const { return active; }

	void _physics_process(double delta) override;
};

} // namespace godot

#endif // BT_PLAYER_H

#ifndef HelloNode_CLASS
#define HelloNode_CLASS

#include <godot_cpp/classes/node2d.hpp>

namespace godot {

class HelloNode : public Node2D {
	GDCLASS(HelloNode, Node2D)

private:
	double speed;
	Vector2 velocity;

	void minHeapTest();
	void jsonTest();

protected:
	static void _bind_methods();

public:
	HelloNode();
	~HelloNode();

	void hello_world(String words);

	void set_speed(const double speed);
	double get_speed() const;

	void _process(double delta) override;
};

} //namespace godot

#endif

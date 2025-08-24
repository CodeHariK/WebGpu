#ifndef VoxelNode_CLASS
#define VoxelNode_CLASS

#include <godot_cpp/classes/node3d.hpp>

using namespace godot;

class VoxelNode : public Node3D {
	GDCLASS(VoxelNode, Node3D);

private:
	void blendTest();

protected:
	static void _bind_methods();

public:
	VoxelNode();
	~VoxelNode();

	void _process(double delta) override;
};

#endif

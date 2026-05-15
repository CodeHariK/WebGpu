#ifndef TENNIS_MANAGER_H
#define TENNIS_MANAGER_H

#include <godot_cpp/classes/node3d.hpp>
#include "tennis_types.h"

namespace godot {

class TennisManager : public Node3D {
	GDCLASS(TennisManager, Node3D)

private:
	void _spawn_court();

protected:
	static void _bind_methods();

public:
	TennisManager();
	~TennisManager();

	void _ready() override;
};

} // namespace godot

#endif // TENNIS_MANAGER_H

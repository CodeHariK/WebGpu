#ifndef CUTTING_STATION_H
#define CUTTING_STATION_H

#include "oc_station.h"

namespace godot {

class OCCuttingStation : public OCStation {
	GDCLASS(OCCuttingStation, OCStation)

private:
	float chop_speed = 0.25f; // Each interact press adds 25%

protected:
	static void _bind_methods();

public:
	OCCuttingStation();
	~OCCuttingStation();

	void interact(Node3D *p_actor) override;

	void set_chop_speed(float p_speed);
	float get_chop_speed() const;
};

} // namespace godot

#endif // CUTTING_STATION_H

#ifndef OC_COOKING_STATION_H
#define OC_COOKING_STATION_H

#include "oc_station.h"

namespace godot {

class OCCookingStation : public OCStation {
	GDCLASS(OCCookingStation, OCStation)

private:
	float cook_speed = 0.1f; // 10 seconds to cook
	float burn_speed = 0.05f; // 20 seconds to burn

protected:
	static void _bind_methods();

public:
	OCCookingStation();
	~OCCookingStation();

	void _process(double delta) override;

	void set_cook_speed(float p_speed);
	float get_cook_speed() const;

	void set_burn_speed(float p_speed);
	float get_burn_speed() const;
};

} // namespace godot

#endif // OC_COOKING_STATION_H

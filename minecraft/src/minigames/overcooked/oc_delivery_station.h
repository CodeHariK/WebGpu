#ifndef OC_DELIVERY_STATION_H
#define OC_DELIVERY_STATION_H

#include "oc_station.h"
#include "oc_recipe.h"

namespace godot {

class OCDeliveryStation : public OCStation {
	GDCLASS(OCDeliveryStation, OCStation)

protected:
	static void _bind_methods();

public:
	OCDeliveryStation();
	~OCDeliveryStation();

	bool can_place_item(Interactable *p_item) override;
	void place_item(Interactable *p_item) override;
};

} // namespace godot

#endif // OC_DELIVERY_STATION_H

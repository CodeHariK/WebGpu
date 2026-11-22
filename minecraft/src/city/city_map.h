/**
 * @file city_map.h
 * @brief CityMap: builds a city exported from the 2d_map project's Map 26 (Map26Export.ts, a JSON
 * file in metres) as nodes under itself: ground and sea (city_ground), roads and the motorway
 * bridge (city_roads), buildings and trees (city_buildings), car parks, dock yards, ponds and
 * piers (city_props); all with colliders, to drive on; and traffic on the roads (CityTraffic).
 *
 * With centre_on_road, the city is moved and turned so the middle of its longest big road piece
 * (an arterial, highway, ring or main road) is at this node's origin, running along +x: a scene's
 * player and cars placed round the origin start on that road.
 */
#ifndef CITY_MAP_H
#define CITY_MAP_H

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class CityMap : public Node3D {
	GDCLASS(CityMap,
			Node3D)

private:
	String json_path = "res://assets/city/map26.json";
	bool centre_on_road = true;
	int traffic_cars = 2000; // cars driving round the roads (CityTraffic)
	Node3D *city = nullptr; // what was built (not saved with the scene)
	float map_size = 0.0f; // metres: the exported map's width (0 until built)

	void _build();
	void _clear();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void rebuild();

	// The built city (its origin is the map's middle), or null; and the map's width in metres.
	Node3D *get_city() const;
	float get_map_size() const;

	void set_json_path(const String &p_path);
	String get_json_path() const;

	void set_centre_on_road(bool p_centre);
	bool get_centre_on_road() const;

	void set_traffic_cars(int p_cars);
	int get_traffic_cars() const;
};

} // namespace godot

#endif // CITY_MAP_H

/**
 * @file city_map.cpp
 * @brief CityMap: reads the JSON, builds the city under a child node (rebuilt on demand, never
 * saved with the scene), and places it so the start road is at the origin.
 */
#include "city_map.h"
#include "city_buildings.h"
#include "city_ground.h"
#include "city_props.h"
#include "city_roads.h"
#include "city_spawn.h"
#include "city_traffic.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void CityMap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("rebuild"), &CityMap::rebuild);
	ClassDB::bind_method(D_METHOD("get_map_size"), &CityMap::get_map_size);
	ClassDB::bind_method(D_METHOD("set_json_path", "path"), &CityMap::set_json_path);
	ClassDB::bind_method(D_METHOD("get_json_path"), &CityMap::get_json_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "json_path", PROPERTY_HINT_FILE, "*.json"), "set_json_path", "get_json_path");
	ClassDB::bind_method(D_METHOD("set_centre_on_road", "centre"), &CityMap::set_centre_on_road);
	ClassDB::bind_method(D_METHOD("get_centre_on_road"), &CityMap::get_centre_on_road);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "centre_on_road"), "set_centre_on_road", "get_centre_on_road");
	ClassDB::bind_method(D_METHOD("set_traffic_cars", "cars"), &CityMap::set_traffic_cars);
	ClassDB::bind_method(D_METHOD("get_traffic_cars"), &CityMap::get_traffic_cars);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "traffic_cars", PROPERTY_HINT_RANGE, "0,10000,1"), "set_traffic_cars", "get_traffic_cars");
}

void CityMap::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build();
	}
}

void CityMap::rebuild() {
	_clear();
	_build();
}

void CityMap::_clear() {
	if (city) {
		remove_child(city);
		city->queue_free();
		city = nullptr;
	}
}

void CityMap::_build() {
	if (city || json_path.is_empty()) {
		return;
	}
	if (!FileAccess::file_exists(json_path)) {
		UtilityFunctions::push_warning("CityMap: no file at ", json_path);
		return;
	}
	const Variant parsed = JSON::parse_string(FileAccess::get_file_as_string(json_path));
	if (parsed.get_type() != Variant::DICTIONARY) {
		UtilityFunctions::push_warning("CityMap: ", json_path, " is not a Map 26 export");
		return;
	}
	const Dictionary map = parsed;
	city = memnew(Node3D);
	city->set_name("City");
	add_child(city);
	const Array roads = map.get("roads", Array());
	map_size = float(double(map.get("size", 1200.0)));
	city::build_ground(city, map.get("coastlines", Array()), map_size);
	city::build_roads(city, roads);
	city::build_buildings(city, map.get("buildings", Array()), map.get("trees", Array()));
	city::build_props(city, map);
	const Transform3D start = centre_on_road ? city::start_on_road(roads) : Transform3D();
	if (traffic_cars > 0 && !Engine::get_singleton()->is_editor_hint()) { // only when playing
		const Vector3 player = start.affine_inverse().xform(Vector3()); // the scene's middle, on the map
		CityTraffic *traffic = memnew(CityTraffic);
		traffic->set_name("Traffic");
		city->add_child(traffic);
		traffic->setup(roads, traffic_cars, uint32_t(int64_t(map.get("seed", 1))), Vector2(player.x, player.z));
	}
	city->set_transform(start);
}

Node3D *CityMap::get_city() const {
	return city;
}

float CityMap::get_map_size() const {
	return map_size;
}

void CityMap::set_json_path(const String &p_path) {
	json_path = p_path;
	if (is_inside_tree()) {
		rebuild();
	}
}

String CityMap::get_json_path() const {
	return json_path;
}

void CityMap::set_centre_on_road(bool p_centre) {
	centre_on_road = p_centre;
	if (is_inside_tree()) {
		rebuild();
	}
}

bool CityMap::get_centre_on_road() const {
	return centre_on_road;
}

void CityMap::set_traffic_cars(int p_cars) {
	traffic_cars = MAX(0, p_cars);
	if (is_inside_tree()) {
		rebuild();
	}
}

int CityMap::get_traffic_cars() const {
	return traffic_cars;
}

} // namespace godot

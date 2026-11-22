/**
 * @file city_traffic.h
 * @brief CityTraffic: cars driving round the Map 26 city's roads (the road graph of
 * city_road_graph). Each car keeps to the right in one of its road's lanes, drives at its own
 * cruising speed, slows behind a car ahead in its lane, and at every junction turns onto another
 * road, picked at random with wider roads likelier (back the way it came only at a dead end).
 *
 * All the cars are drawn by one MultiMesh. Only the POOL cars nearest the camera have a body to hit:
 * kinematic boxes (AnimatableBody3D) that push whatever they meet and that nothing pushes.
 */
#ifndef CITY_TRAFFIC_H
#define CITY_TRAFFIC_H

#include "city_road_graph.h"
#include <godot_cpp/classes/animatable_body3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <vector>

namespace godot {

class CityTraffic : public Node3D {
	GDCLASS(CityTraffic,
			Node3D)

private:
	struct Car {
		int edge = 0;
		int direction = 1; // +1: from the edge's first point to its last; −1: back
		float travelled = 0.0f; // metres along the edge, the way it drives
		float speed = 0.0f; // metres a second now
		float cruise = 0.0f; // metres a second it likes
		int lane = 0; // 0: the inner lane
		Vector3 shown; // where it is drawn: eased towards its place on the road, so turns curve
		float yaw = 0.0f; // the way it is drawn facing (radians about y; 0 faces −z)
		Color paint;
	};

	city::RoadGraph graph;
	std::vector<Car> cars;
	std::vector<std::vector<int>> on_edge; // per edge: the cars on it (rebuilt each frame)
	std::vector<AnimatableBody3D *> bodies;
	std::vector<int> body_car; // per body: the car it follows
	Ref<MultiMesh> multimesh;
	PackedFloat32Array buffer;
	uint32_t seed = 1;
	int frame = 0;

	uint32_t _random();
	float _random_unit();
	int _weighted_edge(const std::vector<int> &p_choices, int p_not);
	void _target(
			const Car &p_car,
			Vector3 *r_place,
			float *r_yaw
	) const;
	void _next_edge(Car &p_car);
	void _place_at_random(
			Car &p_car,
			float p_lane_metres
	);
	float _gap_ahead(
			int p_index,
			const Car &p_car
	) const;
	void _make_bodies(int p_count);
	void _give_bodies();
	void _draw();

protected:
	static void _bind_methods();

public:
	// p_count cars on p_roads (the export's roads), placed at random from p_seed, none within
	// CLEAR metres of p_clear (where the player starts, (x, z)).
	void setup(
			const Array &p_roads,
			int p_count,
			uint32_t p_seed,
			const Vector2 &p_clear
	);

	void _physics_process(double p_delta) override;
};

} // namespace godot

#endif // CITY_TRAFFIC_H

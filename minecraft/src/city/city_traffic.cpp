/**
 * @file city_traffic.cpp
 * @brief CityTraffic: placing the cars, then every physics frame each car's speed (towards its
 * cruising speed, braking behind the car ahead), its move along its road, its turn at a junction,
 * its drawn place eased towards its lane; then the MultiMesh's buffer written in one go and the
 * bodies moved onto the cars nearest the camera.
 */
#include "city_traffic.h"
#include "city_car_mesh.h"
#include <algorithm>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

namespace {

const Vector3 CAR_SIZE(3.6f, 2.8f, 7.2f); // metres: width, height, length (to sit by the player's car)
constexpr float MIN_CRUISE = 11.0f; // metres a second (40 km/h)
constexpr float MAX_CRUISE = 19.0f; // metres a second (68 km/h)
constexpr float ACCELERATION = 4.0f; // metres a second, each second
constexpr float BRAKING = 14.0f;
constexpr float SAFE_GAP = 16.0f; // metres between bumpers where it starts to slow
constexpr float STOP_GAP = 4.0f; // metres where it stops
constexpr float EASE = 5.0f; // how fast the drawn place catches up (a second⁻¹): turns become curves
constexpr float ROAD_TOP = 0.07f; // metres: the roads' surface
constexpr float CLEAR = 60.0f; // metres round the player's start kept free of cars
constexpr int POOL = 48; // cars with a body to hit
constexpr int REPICK = 15; // frames between choosing which cars have them
constexpr int FLOATS = 16; // per MultiMesh instance: a 3 × 4 transform and a colour
const char *PAINTS[] = { "#e0533f", "#3f7fe0", "#f2c94c", "#56b870", "#f4f1ea", "#9b59b6", "#2d2d2d", "#ff8c42", "#5bc0c8", "#e86fa8" };
constexpr int PAINT_COUNT = 10;

} // namespace

void CityTraffic::_bind_methods() {}

uint32_t CityTraffic::_random() {
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;
	return seed;
}

float CityTraffic::_random_unit() {
	return float(_random() % 1000000) / 1000000.0f;
}

// One of p_choices, likelier the more lanes it has, but not p_not unless it is the only one.
int CityTraffic::_weighted_edge(
		const std::vector<int> &p_choices,
		int p_not
) {
	int total = 0;
	for (int e : p_choices) {
		total += e == p_not ? 0 : graph.edges[e].lanes;
	}
	if (total == 0) {
		return p_not;
	}
	int pick = int(_random() % uint32_t(total));
	for (int e : p_choices) {
		if (e == p_not) {
			continue;
		}
		pick -= graph.edges[e].lanes;
		if (pick < 0) {
			return e;
		}
	}
	return p_choices.back();
}

// p_car somewhere on the roads at random: each lane metre (of p_lane_metres) as likely.
void CityTraffic::_place_at_random(
		Car &p_car,
		float p_lane_metres
) {
	float pick = _random_unit() * p_lane_metres;
	for (size_t e = 0; e < graph.edges.size(); e++) {
		const float room = graph.edges[e].length * float(graph.edges[e].lanes);
		if (pick <= room || e + 1 == graph.edges.size()) {
			p_car.edge = int(e);
			p_car.travelled = MIN(pick / float(graph.edges[e].lanes), graph.edges[e].length);
			return;
		}
		pick -= room;
	}
}

void CityTraffic::setup(
		const Array &p_roads,
		int p_count,
		uint32_t p_seed,
		const Vector2 &p_clear
) {
	seed = p_seed | 1u;
	graph = city::build_road_graph(p_roads);
	if (graph.edges.empty() || p_count <= 0) {
		return;
	}
	on_edge.resize(graph.edges.size());
	float total = 0.0f; // lane metres, so wider roads get more cars
	for (const city::RoadEdge &e : graph.edges) {
		total += e.length * float(e.lanes);
	}
	cars.resize(p_count);
	for (Car &car : cars) {
		for (int tries = 0; tries < 20; tries++) {
			_place_at_random(car, total);
			car.direction = 1;
			Vector3 place;
			_target(car, &place, &car.yaw);
			if (Vector2(place.x, place.z).distance_to(p_clear) > CLEAR) {
				break;
			}
		}
		car.direction = _random() % 2 ? 1 : -1;
		car.lane = int(_random() % uint32_t(graph.edges[car.edge].lanes));
		car.cruise = Math::lerp(MIN_CRUISE, MAX_CRUISE, _random_unit());
		car.speed = car.cruise;
		car.paint = Color::html(PAINTS[_random() % PAINT_COUNT]);
		_target(car, &car.shown, &car.yaw);
	}
	Rect2 area(graph.edges[0].points[0], Vector2());
	for (const city::RoadEdge &e : graph.edges) {
		for (const Vector2 &p : e.points) {
			area.expand_to(p);
		}
	}
	multimesh.instantiate();
	multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
	multimesh->set_use_colors(true);
	multimesh->set_mesh(city::make_car_mesh(CAR_SIZE));
	multimesh->set_instance_count(p_count);
	MultiMeshInstance3D *drawn = memnew(MultiMeshInstance3D);
	drawn->set_name("Cars");
	drawn->set_multimesh(multimesh);
	drawn->set_custom_aabb(AABB(Vector3(area.position.x - 50, -5, area.position.y - 50), Vector3(area.size.x + 100, 20, area.size.y + 100)));
	add_child(drawn);
	buffer.resize(int64_t(p_count) * FLOATS);
	_make_bodies(MIN(POOL, p_count));
	_draw();
	set_physics_process(true);
}

// Where p_car belongs now: its point on its road, in its lane, and the way it faces.
void CityTraffic::_target(
		const Car &p_car,
		Vector3 *r_place,
		float *r_yaw
) const {
	const city::RoadEdge &edge = graph.edges[p_car.edge];
	const float distance = p_car.direction > 0 ? p_car.travelled : edge.length - p_car.travelled;
	Vector2 point;
	Vector2 run;
	city::point_along(edge, distance, &point, &run);
	const Vector2 ahead = run * float(p_car.direction);
	const Vector2 right(-ahead.y, ahead.x);
	const Vector2 at = point + right * city::lane_offset(edge, p_car.lane);
	*r_place = Vector3(at.x, ROAD_TOP, at.y);
	*r_yaw = Math::atan2(-ahead.x, -ahead.y);
}

// At the end of its road: onto another road at that junction, in a random lane of it.
void CityTraffic::_next_edge(Car &p_car) {
	const city::RoadEdge &edge = graph.edges[p_car.edge];
	const int junction = p_car.direction > 0 ? edge.to : edge.from;
	const int next = _weighted_edge(graph.junctions[junction], p_car.edge);
	p_car.travelled -= edge.length;
	p_car.edge = next;
	const city::RoadEdge &road = graph.edges[next];
	p_car.direction = road.from == junction ? 1 : -1;
	p_car.travelled = CLAMP(p_car.travelled, 0.0f, road.length);
	p_car.lane = int(_random() % uint32_t(road.lanes));
}

// Metres between p_car's front bumper and the back of the nearest car ahead in its lane (or a big
// number).
float CityTraffic::_gap_ahead(
		int p_index,
		const Car &p_car
) const {
	float gap = 1e9f;
	for (int other : on_edge[p_car.edge]) {
		const Car &o = cars[other];
		if (other == p_index || o.direction != p_car.direction || o.lane != p_car.lane) {
			continue;
		}
		const float d = o.travelled - p_car.travelled;
		if ((d > 0.0f || (d == 0.0f && other > p_index)) && d < gap) {
			gap = d;
		}
	}
	return gap - CAR_SIZE.z;
}

void CityTraffic::_make_bodies(int p_count) {
	Ref<BoxShape3D> shape;
	shape.instantiate();
	shape->set_size(CAR_SIZE);
	for (int k = 0; k < p_count; k++) {
		AnimatableBody3D *body = memnew(AnimatableBody3D);
		body->set_sync_to_physics(false); // moved by hand each frame; a jump to another car pushes nothing
		CollisionShape3D *collider = memnew(CollisionShape3D);
		collider->set_shape(shape);
		collider->set_position(Vector3(0, CAR_SIZE.y * 0.5f, 0));
		body->add_child(collider);
		add_child(body);
		bodies.push_back(body);
		body_car.push_back(k);
	}
}

// The bodies onto the cars nearest the camera.
void CityTraffic::_give_bodies() {
	const Camera3D *camera = get_viewport() ? get_viewport()->get_camera_3d() : nullptr;
	if (!camera || bodies.empty()) {
		return;
	}
	const Vector3 eye = to_local(camera->get_global_position());
	std::vector<std::pair<float, int>> near(cars.size());
	for (size_t i = 0; i < cars.size(); i++) {
		near[i] = { cars[i].shown.distance_squared_to(eye), int(i) };
	}
	std::nth_element(near.begin(), near.begin() + int(bodies.size()) - 1, near.end());
	for (size_t b = 0; b < bodies.size(); b++) {
		body_car[b] = near[b].second;
	}
}

// Every car's transform and paint into the MultiMesh, and each body onto its car.
void CityTraffic::_draw() {
	float *out = buffer.ptrw();
	for (size_t i = 0; i < cars.size(); i++) {
		const Car &car = cars[i];
		const Basis basis(Vector3(0, 1, 0), car.yaw);
		float *f = out + i * FLOATS;
		for (int row = 0; row < 3; row++) {
			f[row * 4 + 0] = basis[row][0];
			f[row * 4 + 1] = basis[row][1];
			f[row * 4 + 2] = basis[row][2];
			f[row * 4 + 3] = car.shown[row];
		}
		f[12] = car.paint.r;
		f[13] = car.paint.g;
		f[14] = car.paint.b;
		f[15] = 1.0f;
	}
	multimesh->set_buffer(buffer);
	for (size_t b = 0; b < bodies.size(); b++) {
		const Car &car = cars[body_car[b]];
		bodies[b]->set_transform(Transform3D(Basis(Vector3(0, 1, 0), car.yaw), car.shown));
	}
}

void CityTraffic::_physics_process(double p_delta) {
	if (cars.empty()) {
		return;
	}
	const float dt = float(p_delta);
	for (std::vector<int> &list : on_edge) {
		list.clear();
	}
	for (size_t i = 0; i < cars.size(); i++) {
		on_edge[cars[i].edge].push_back(int(i));
	}
	const float ease = 1.0f - Math::exp(-EASE * dt);
	for (size_t i = 0; i < cars.size(); i++) {
		Car &car = cars[i];
		const float gap = _gap_ahead(int(i), car);
		const float wanted = gap < STOP_GAP ? 0.0f : gap < SAFE_GAP ? car.cruise * (gap - STOP_GAP) / (SAFE_GAP - STOP_GAP) : car.cruise;
		car.speed = car.speed < wanted ? MIN(wanted, car.speed + ACCELERATION * dt) : MAX(wanted, car.speed - BRAKING * dt);
		car.travelled += car.speed * dt;
		while (car.travelled > graph.edges[car.edge].length) {
			_next_edge(car);
		}
		Vector3 place;
		float yaw = 0.0f;
		_target(car, &place, &yaw);
		car.shown = car.shown.lerp(place, ease);
		car.yaw = Math::lerp_angle(car.yaw, yaw, ease);
	}
	if (frame++ % REPICK == 0) {
		_give_bodies();
	}
	_draw();
}

} // namespace godot

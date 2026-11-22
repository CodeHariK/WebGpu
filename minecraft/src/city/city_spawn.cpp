/**
 * @file city_spawn.cpp
 * @brief The start: of the big roads, the longest straight piece (arterials beat highways beat the
 * ring beat main roads: a kind's pieces only count if no better kind has any).
 */
#include "city_spawn.h"
#include "city_data.h"

namespace godot::city {

Transform3D start_on_road(const Array &p_roads) {
	const char *order[] = { "arterial", "highway", "ring", "main" };
	for (const char *kind : order) {
		float best = 0.0f;
		Vector2 a;
		Vector2 b;
		for (int64_t r = 0; r < p_roads.size(); r++) {
			const Dictionary road = p_roads[r];
			if (String(road.get("kind", "")) != kind) {
				continue;
			}
			const PackedVector2Array line = read_line(road["points"]);
			for (int64_t i = 0; i + 1 < line.size(); i++) {
				const float length = line[i].distance_to(line[i + 1]);
				if (length > best) {
					best = length;
					a = line[i];
					b = line[i + 1];
				}
			}
		}
		if (best <= 0.0f) {
			continue;
		}
		const Vector2 middle = (a + b) * 0.5f;
		const float angle = (b - a).angle(); // from x towards z
		const Basis turn(Vector3(0, 1, 0), angle); // turns the road's direction onto +x
		return Transform3D(turn, turn.xform(Vector3(-middle.x, 0, -middle.y)));
	}
	return Transform3D();
}

} // namespace godot::city

/**
 * @file city_road_graph.h
 * @brief The Map 26 city's roads (all but the motorway) as a graph for traffic. Each road line is
 * cut where another road's end meets it part way along (a T-junction), and each piece is an edge
 * between the junctions at its two ends (ends within JOIN metres of each other are one junction).
 * Each edge knows its lanes each way and its lengths, so a point any distance along it can be found.
 */
#ifndef CITY_ROAD_GRAPH_H
#define CITY_ROAD_GRAPH_H

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <vector>

namespace godot::city {

struct RoadEdge {
	PackedVector2Array points; // (x, z), metres
	PackedFloat32Array along; // metres from the first point to each point
	float length = 0.0f;
	int lanes = 1; // lanes each way
	float lane_width = 0.0f; // metres
	float median = 0.0f; // metres from the road's middle to its first lane's edge
	int from = -1; // the junction at its first point
	int to = -1; // the junction at its last point
};

struct RoadGraph {
	std::vector<RoadEdge> edges;
	std::vector<std::vector<int>> junctions; // per junction: the edges that end there
};

// The graph of the export's roads, the motorway left out.
RoadGraph build_road_graph(const Array &p_roads);

// The point p_distance metres along p_edge from its first point, and the way the road runs there
// (towards its last point, unit length).
void point_along(
		const RoadEdge &p_edge,
		float p_distance,
		Vector2 *r_point,
		Vector2 *r_direction
);

// Metres right of the road's middle (seen driving) to lane p_lane's middle; lane 0 is the inner.
float lane_offset(
		const RoadEdge &p_edge,
		int p_lane
);

} // namespace godot::city

#endif // CITY_ROAD_GRAPH_H

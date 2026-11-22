/**
 * @file city_road_graph.cpp
 * @brief Building the road graph in three passes: the lines read; every line end that meets another
 * line part way along cuts that line there (and is moved onto it); then the pieces, their lengths
 * and lanes measured, joined at junctions found by nearness (a hash of JOIN-metre squares).
 */
#include "city_road_graph.h"
#include "city_data.h"
#include <algorithm>
#include <unordered_map>

namespace godot::city {

namespace {

constexpr float JOIN = 3.0f; // metres: road ends closer than this meet
constexpr float LANE = 6.0f; // metres: the narrowest lane
constexpr float MEDIAN = 1.0f; // metres from the road's middle to the first lane
constexpr float KERB = 1.5f; // metres from the last lane to the road's side

struct Line {
	PackedVector2Array points;
	float width = 0.0f;
	Rect2 bounds; // grown by half the width
};

struct Cut {
	int segment = 0; // the cut lies on the segment from this point to the next
	float t = 0.0f; // how far along that segment
	Vector2 point;
};

std::vector<Line> read_lines(const Array &p_roads) {
	std::vector<Line> lines;
	for (int64_t r = 0; r < p_roads.size(); r++) {
		const Dictionary road = p_roads[r];
		if (String(road.get("kind", "")) == "motorway") {
			continue;
		}
		Line line;
		line.points = read_line(road["points"]);
		if (line.points.size() < 2) {
			continue;
		}
		line.width = read_float(road, "width", 20.0f);
		line.bounds = Rect2(line.points[0], Vector2());
		for (const Vector2 &p : line.points) {
			line.bounds.expand_to(p);
		}
		line.bounds = line.bounds.grow(line.width * 0.5f + JOIN);
		lines.push_back(line);
	}
	return lines;
}

// The nearest point to p_point on p_line: its segment, how far along it, and the distance.
float nearest_on(
		const Line &p_line,
		const Vector2 &p_point,
		Cut *r_cut
) {
	float best = 1e9f;
	for (int64_t s = 0; s + 1 < p_line.points.size(); s++) {
		const Vector2 a = p_line.points[s];
		const Vector2 ab = p_line.points[s + 1] - a;
		const float len2 = ab.length_squared();
		const float t = len2 > 0.0f ? CLAMP((p_point - a).dot(ab) / len2, 0.0f, 1.0f) : 0.0f;
		const Vector2 q = a + ab * t;
		const float d = q.distance_to(p_point);
		if (d < best) {
			best = d;
			*r_cut = { int(s), t, q };
		}
	}
	return best;
}

// Metres along p_line to a cut.
float cut_along(
		const Line &p_line,
		const Cut &p_cut
) {
	float walked = 0.0f;
	for (int s = 0; s < p_cut.segment; s++) {
		walked += p_line.points[s].distance_to(p_line.points[s + 1]);
	}
	return walked + p_line.points[p_cut.segment].distance_to(p_cut.point);
}

float line_length(const Line &p_line) {
	float walked = 0.0f;
	for (int64_t s = 0; s + 1 < p_line.points.size(); s++) {
		walked += p_line.points[s].distance_to(p_line.points[s + 1]);
	}
	return walked;
}

// Each line end that meets another line: part way along, it cuts that line there; near that
// line's end, it is moved onto that end (so the two join).
std::vector<std::vector<Cut>> find_cuts(std::vector<Line> *r_lines) {
	std::vector<Line> &lines = *r_lines;
	std::vector<std::vector<Cut>> cuts(lines.size());
	for (size_t i = 0; i < lines.size(); i++) {
		for (int end = 0; end < 2; end++) {
			const int64_t index = end == 0 ? 0 : lines[i].points.size() - 1;
			const Vector2 p = lines[i].points[index];
			int best_line = -1;
			Cut best_cut;
			float best = 1e9f;
			for (size_t j = 0; j < lines.size(); j++) {
				if (j == i || !lines[j].bounds.has_point(p)) {
					continue;
				}
				Cut cut;
				const float d = nearest_on(lines[j], p, &cut);
				if (d < MAX(JOIN, lines[j].width * 0.5f) && d < best) {
					best = d;
					best_line = int(j);
					best_cut = cut;
				}
			}
			if (best_line < 0) {
				continue;
			}
			const Line &other = lines[best_line];
			const float along = cut_along(other, best_cut);
			if (along < JOIN) {
				lines[i].points.set(index, other.points[0]);
			} else if (along > line_length(other) - JOIN) {
				lines[i].points.set(index, other.points[other.points.size() - 1]);
			} else {
				lines[i].points.set(index, best_cut.point);
				cuts[best_line].push_back(best_cut);
			}
		}
	}
	return cuts;
}

// p_line cut into pieces at p_cuts.
std::vector<PackedVector2Array> cut_line(
		const Line &p_line,
		std::vector<Cut> p_cuts
) {
	std::sort(p_cuts.begin(), p_cuts.end(), [](const Cut &a, const Cut &b) {
		return a.segment != b.segment ? a.segment < b.segment : a.t < b.t;
	});
	std::vector<PackedVector2Array> pieces(1);
	size_t c = 0;
	for (int64_t s = 0; s < p_line.points.size(); s++) {
		const PackedVector2Array &last = pieces.back();
		if (last.is_empty() || last[last.size() - 1].distance_to(p_line.points[s]) > 0.01f) { // no zero-length pieces
			pieces.back().push_back(p_line.points[s]);
		}
		while (c < p_cuts.size() && p_cuts[c].segment == s) {
			if (pieces.back()[pieces.back().size() - 1].distance_to(p_cuts[c].point) > 0.5f) {
				pieces.back().push_back(p_cuts[c].point);
			}
			pieces.emplace_back();
			pieces.back().push_back(p_cuts[c].point);
			c++;
		}
	}
	return pieces;
}

// The junction at p_point: one already within JOIN, or a new one.
int junction_at(
		const Vector2 &p_point,
		std::unordered_map<int64_t, std::vector<int>> *r_cells,
		std::vector<Vector2> *r_places,
		RoadGraph *r_graph
) {
	const int cx = int(Math::floor(p_point.x / JOIN));
	const int cz = int(Math::floor(p_point.y / JOIN));
	auto key = [](int x, int z) { return (int64_t(x) << 32) ^ int64_t(uint32_t(z)); };
	for (int dx = -1; dx <= 1; dx++) {
		for (int dz = -1; dz <= 1; dz++) {
			const auto found = r_cells->find(key(cx + dx, cz + dz));
			if (found == r_cells->end()) {
				continue;
			}
			for (int id : found->second) {
				if ((*r_places)[id].distance_to(p_point) < JOIN) {
					return id;
				}
			}
		}
	}
	const int id = int(r_places->size());
	r_places->push_back(p_point);
	(*r_cells)[key(cx, cz)].push_back(id);
	r_graph->junctions.emplace_back();
	return id;
}

// An edge on p_points, measured, with lanes to fit p_width.
RoadEdge make_edge(
		const PackedVector2Array &p_points,
		float p_width
) {
	RoadEdge edge;
	edge.points = p_points;
	edge.along.resize(p_points.size());
	float walked = 0.0f;
	for (int64_t i = 0; i < p_points.size(); i++) {
		if (i > 0) {
			walked += p_points[i - 1].distance_to(p_points[i]);
		}
		edge.along.set(i, walked);
	}
	edge.length = walked;
	const float room = MAX(LANE, p_width * 0.5f - MEDIAN - KERB);
	edge.lanes = MAX(1, int(room / LANE));
	edge.lane_width = room / float(edge.lanes);
	edge.median = MEDIAN;
	return edge;
}

} // namespace

RoadGraph build_road_graph(const Array &p_roads) {
	RoadGraph graph;
	std::vector<Line> lines = read_lines(p_roads);
	const std::vector<std::vector<Cut>> cuts = find_cuts(&lines);
	std::unordered_map<int64_t, std::vector<int>> cells;
	std::vector<Vector2> places;
	for (size_t i = 0; i < lines.size(); i++) {
		for (const PackedVector2Array &piece : cut_line(lines[i], cuts[i])) {
			if (piece.size() < 2) {
				continue;
			}
			RoadEdge edge = make_edge(piece, lines[i].width);
			if (edge.length < 1.0f) {
				continue;
			}
			edge.from = junction_at(piece[0], &cells, &places, &graph);
			edge.to = junction_at(piece[piece.size() - 1], &cells, &places, &graph);
			const int id = int(graph.edges.size());
			graph.junctions[edge.from].push_back(id);
			if (edge.to != edge.from) {
				graph.junctions[edge.to].push_back(id);
			}
			graph.edges.push_back(edge);
		}
	}
	return graph;
}

void point_along(
		const RoadEdge &p_edge,
		float p_distance,
		Vector2 *r_point,
		Vector2 *r_direction
) {
	const float d = CLAMP(p_distance, 0.0f, p_edge.length);
	int64_t lo = 0;
	int64_t hi = p_edge.points.size() - 1;
	while (hi - lo > 1) { // the piece holding d
		const int64_t mid = (lo + hi) / 2;
		if (p_edge.along[mid] <= d) {
			lo = mid;
		} else {
			hi = mid;
		}
	}
	const Vector2 a = p_edge.points[lo];
	const Vector2 b = p_edge.points[hi];
	const float piece = p_edge.along[hi] - p_edge.along[lo];
	const float t = piece > 0.0f ? (d - p_edge.along[lo]) / piece : 0.0f;
	*r_point = a.lerp(b, t);
	*r_direction = (b - a).normalized();
}

float lane_offset(
		const RoadEdge &p_edge,
		int p_lane
) {
	return p_edge.median + (float(p_lane) + 0.5f) * p_edge.lane_width;
}

} // namespace godot::city

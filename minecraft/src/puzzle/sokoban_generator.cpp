#include "sokoban_generator.h"

#include "sokoban_solver.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <queue>
#include <random>

namespace godot {

namespace {

const int MIN_ROOM = 3; // interior cells per side

struct Rect {
	int x0, y0, x1, y1; // interior, inclusive
	int w() const { return x1 - x0 + 1; }
	int h() const { return y1 - y0 + 1; }
};

struct Door {
	int x, y;
	bool vertical_wall; // true: wall runs along y, rooms are left / right
};

struct Builder {
	const SokobanGenerator::Params &p;
	std::mt19937 &rng;
	int W, H;
	std::vector<std::string> g;
	std::vector<Rect> rooms;
	std::vector<Door> doors;
	std::vector<std::vector<int>> adj; // room -> door indices
	std::vector<char> reserved; // cells nothing random may use

	Builder(const SokobanGenerator::Params &p_p, std::mt19937 &p_rng) :
			p(p_p), rng(p_rng), W(p_p.width), H(p_p.height) {}

	int rnd(int p_lo, int p_hi) { return std::uniform_int_distribution<int>(p_lo, p_hi)(rng); }
	bool has_door(int p_x, int p_y) const {
		for (const Door &d : doors) {
			if (d.x == p_x && d.y == p_y) {
				return true;
			}
		}
		return false;
	}
	int room_of(int p_x, int p_y) const {
		for (size_t i = 0; i < rooms.size(); ++i) {
			const Rect &r = rooms[i];
			if (p_x >= r.x0 && p_x <= r.x1 && p_y >= r.y0 && p_y <= r.y1) {
				return (int)i;
			}
		}
		return -1;
	}
	void reserve(int p_x, int p_y) {
		if (p_x >= 0 && p_y >= 0 && p_x < W && p_y < H) {
			reserved[p_y * W + p_x] = 1;
		}
	}

	// --- 1. Rooms (BSP) -------------------------------------------------------
	// Split candidates exclude lines that would wall off an existing doorway on the
	// rect's border (the door's inner neighbour must stay floor).
	bool split(int p_index) {
		Rect r = rooms[p_index];
		const bool prefer_vertical = r.w() >= r.h();
		for (int attempt = 0; attempt < 2; ++attempt) {
			const bool vertical = (attempt == 0) == prefer_vertical;
			const int lo = (vertical ? r.x0 : r.y0) + MIN_ROOM;
			const int hi = (vertical ? r.x1 : r.y1) - MIN_ROOM;
			std::vector<int> cands;
			for (int s = lo; s <= hi; ++s) {
				const bool blocked = vertical ? (has_door(s, r.y0 - 1) || has_door(s, r.y1 + 1))
											  : (has_door(r.x0 - 1, s) || has_door(r.x1 + 1, s));
				if (!blocked) {
					cands.push_back(s);
				}
			}
			if (cands.empty()) {
				continue;
			}
			const int s = cands[rnd(0, (int)cands.size() - 1)];
			Rect a = r;
			Rect b = r;
			Door d;
			d.vertical_wall = vertical;
			if (vertical) {
				a.x1 = s - 1;
				b.x0 = s + 1;
				d.x = s;
				d.y = rnd(r.y0, r.y1);
			} else {
				a.y1 = s - 1;
				b.y0 = s + 1;
				d.x = rnd(r.x0, r.x1);
				d.y = s;
			}
			rooms[p_index] = a;
			rooms.push_back(b);
			doors.push_back(d);
			return true;
		}
		return false;
	}

	bool make_rooms() {
		rooms.push_back({ 1, 1, W - 2, H - 2 });
		while ((int)rooms.size() < p.rooms) {
			// Split the biggest room that still can be split.
			std::vector<int> order(rooms.size());
			for (size_t i = 0; i < order.size(); ++i) {
				order[i] = (int)i;
			}
			std::sort(order.begin(), order.end(),
					[&](int a, int b) { return rooms[a].w() * rooms[a].h() > rooms[b].w() * rooms[b].h(); });
			bool did = false;
			for (int i : order) {
				if (split(i)) {
					did = true;
					break;
				}
			}
			if (!did) {
				return false;
			}
		}
		g.assign(H, std::string(W, '#'));
		for (const Rect &r : rooms) {
			for (int y = r.y0; y <= r.y1; ++y) {
				for (int x = r.x0; x <= r.x1; ++x) {
					g[y][x] = '.';
				}
			}
		}
		adj.assign(rooms.size(), {});
		for (size_t i = 0; i < doors.size(); ++i) {
			const Door &d = doors[i];
			g[d.y][d.x] = '.';
			const int a = d.vertical_wall ? room_of(d.x - 1, d.y) : room_of(d.x, d.y - 1);
			const int b = d.vertical_wall ? room_of(d.x + 1, d.y) : room_of(d.x, d.y + 1);
			if (a < 0 || b < 0) {
				return false;
			}
			adj[a].push_back((int)i);
			adj[b].push_back((int)i);
		}
		return true;
	}

	int other_room(int p_door, int p_room) const {
		const Door &d = doors[p_door];
		const int a = d.vertical_wall ? room_of(d.x - 1, d.y) : room_of(d.x, d.y - 1);
		const int b = d.vertical_wall ? room_of(d.x + 1, d.y) : room_of(d.x, d.y + 1);
		return a == p_room ? b : a;
	}

	// Room path as the list of doors crossed (rooms form a tree, so it is unique).
	std::vector<int> door_path(int p_from, int p_to, std::vector<int> *r_arrive_from = nullptr) const {
		std::vector<int> via(rooms.size(), -1);
		std::vector<int> prev(rooms.size(), -1);
		std::vector<char> seen(rooms.size(), 0);
		std::queue<int> q;
		q.push(p_from);
		seen[p_from] = 1;
		while (!q.empty()) {
			const int r = q.front();
			q.pop();
			for (int di : adj[r]) {
				const int n = other_room(di, r);
				if (!seen[n]) {
					seen[n] = 1;
					prev[n] = r;
					via[n] = di;
					q.push(n);
				}
			}
		}
		std::vector<int> path;
		for (int r = p_to; r != p_from && r >= 0; r = prev[r]) {
			path.push_back(via[r]);
			if (r_arrive_from) {
				r_arrive_from->push_back(prev[r]); // the room the player crosses this door from
			}
		}
		return path;
	}

	// An outer-wall cell next to room p_room (not a corner, not already open).
	bool outer_cell(int p_room, int &r_x, int &r_y, int &r_in_x, int &r_in_y) {
		const Rect &r = rooms[p_room];
		std::vector<std::array<int, 4>> c;
		if (r.x0 == 1) {
			for (int y = r.y0; y <= r.y1; ++y) {
				c.push_back({ 0, y, 1, y });
			}
		}
		if (r.x1 == W - 2) {
			for (int y = r.y0; y <= r.y1; ++y) {
				c.push_back({ W - 1, y, W - 2, y });
			}
		}
		if (r.y0 == 1) {
			for (int x = r.x0; x <= r.x1; ++x) {
				c.push_back({ x, 0, x, 1 });
			}
		}
		if (r.y1 == H - 2) {
			for (int x = r.x0; x <= r.x1; ++x) {
				c.push_back({ x, H - 1, x, H - 2 });
			}
		}
		c.erase(std::remove_if(c.begin(), c.end(),
						[&](const std::array<int, 4> &e) { return g[e[1]][e[0]] != '#' || reserved[e[3] * W + e[2]]; }),
				c.end());
		if (c.empty()) {
			return false;
		}
		const std::array<int, 4> &e = c[rnd(0, (int)c.size() - 1)];
		r_x = e[0];
		r_y = e[1];
		r_in_x = e[2];
		r_in_y = e[3];
		return true;
	}

	bool free_cell(int p_room, int &r_x, int &r_y) {
		const Rect &r = rooms[p_room];
		for (int tries = 0; tries < 60; ++tries) {
			const int x = rnd(r.x0, r.x1);
			const int y = rnd(r.y0, r.y1);
			if (g[y][x] == '.' && !reserved[y * W + x]) {
				r_x = x;
				r_y = y;
				return true;
			}
		}
		return false;
	}

	// Water gap in doorway p_d, crossed from room p_from: the doorway becomes water and a
	// crate sits two cells back on the approach line (player behind it pushes it twice).
	bool water_gap(const Door &p_d, int p_from) {
		int dx = 0;
		int dy = 0;
		if (p_d.vertical_wall) {
			dx = room_of(p_d.x - 1, p_d.y) == p_from ? 1 : -1;
		} else {
			dy = room_of(p_d.x, p_d.y - 1) == p_from ? 1 : -1;
		}
		const int c2x = p_d.x - 2 * dx, c2y = p_d.y - 2 * dy; // crate
		const int c3x = p_d.x - 3 * dx, c3y = p_d.y - 3 * dy; // player stands here
		if (room_of(c2x, c2y) != p_from || room_of(c3x, c3y) != p_from) {
			return false;
		}
		if (g[c2y][c2x] != '.' || g[c3y][c3x] != '.' || reserved[c2y * W + c2x]) {
			return false;
		}
		g[p_d.y][p_d.x] = '~';
		g[c2y][c2x] = 'B';
		reserve(c2x, c2y);
		reserve(c3x, c3y);
		return true;
	}

	// Small 2x1 / 2x2 pools inside rooms (water: fillable with a box; lava: burns boxes).
	void add_pools() {
		for (int i = 0; i < p.pools; ++i) {
			const Rect &r = rooms[rnd(0, (int)rooms.size() - 1)];
			if (r.w() < 4 || r.h() < 4) {
				continue;
			}
			const int pw = rnd(1, 2);
			const int ph = pw == 1 ? 2 : rnd(1, 2);
			const int x0 = rnd(r.x0 + 1, r.x1 - pw);
			const int y0 = rnd(r.y0 + 1, r.y1 - ph);
			bool ok = true;
			for (int y = y0; y < y0 + ph && ok; ++y) {
				for (int x = x0; x < x0 + pw && ok; ++x) {
					ok = g[y][x] == '.' && !reserved[y * W + x];
				}
			}
			if (!ok) {
				continue;
			}
			const char liquid = std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < p.lava_chance ? '^' : '~';
			for (int y = y0; y < y0 + ph; ++y) {
				for (int x = x0; x < x0 + pw; ++x) {
					g[y][x] = liquid;
					reserve(x, y);
				}
			}
		}
	}

	// Ice patches: boxes pushed onto them slide until blocked (placed last, on empty floor).
	void add_ice() {
		for (int i = 0; i < p.ice_patches; ++i) {
			const Rect &r = rooms[rnd(0, (int)rooms.size() - 1)];
			const int pw = std::min(r.w(), rnd(2, 3));
			const int ph = std::min(r.h(), rnd(2, 3));
			const int x0 = rnd(r.x0, r.x1 - pw + 1);
			const int y0 = rnd(r.y0, r.y1 - ph + 1);
			for (int y = y0; y < y0 + ph; ++y) {
				for (int x = x0; x < x0 + pw; ++x) {
					if (g[y][x] == '.') {
						g[y][x] = 'I';
					}
				}
			}
		}
	}

	// Island look: the outer wall ring becomes water (or lava); entrance bridge and exit stay.
	void make_ring() {
		if (p.ring == 0) {
			return;
		}
		const char liquid = p.ring == 2 ? '^' : '~';
		for (int y = 0; y < H; ++y) {
			for (int x = 0; x < W; ++x) {
				if ((x == 0 || y == 0 || x == W - 1 || y == H - 1) && g[y][x] == '#') {
					g[y][x] = liquid;
				}
			}
		}
	}

	// --- 2 + 3. Goals and blockers --------------------------------------------
	bool populate() {
		reserved.assign((size_t)W * H, 0);
		// Doorway neighbours stay clear so a doorway crate can always be pushed through.
		for (const Door &d : doors) {
			reserve(d.x, d.y);
			reserve(d.x - 1, d.y);
			reserve(d.x + 1, d.y);
			reserve(d.x, d.y - 1);
			reserve(d.x, d.y + 1);
		}
		const int n = (int)rooms.size();
		const int start = rnd(0, n - 1);
		int ex, ey, ix, iy;
		if (!outer_cell(start, ex, ey, ix, iy)) {
			return false;
		}
		g[ey][ex] = '='; // entrance bridge (a gap in the wall / water ring)
		reserve(ix, iy);
		int px, py;
		if (!free_cell(start, px, py)) {
			return false;
		}
		g[py][px] = 'P';
		reserve(px, py);

		// Key: the room farthest (most doors) from the start.
		int key_room = start;
		size_t best = 0;
		for (int r = 0; r < n; ++r) {
			const size_t len = door_path(start, r).size();
			if (len > best || (len == best && rnd(0, 1))) {
				best = len;
				key_room = r;
			}
		}
		int kx, ky;
		if (!free_cell(key_room, kx, ky)) {
			return false;
		}
		g[ky][kx] = 'K';
		reserve(kx, ky);

		// Exit door: outer wall of another room (not the start, ideally not the key room).
		std::vector<int> exit_rooms;
		for (int r = 0; r < n; ++r) {
			if (r != start && (r != key_room || n <= 2)) {
				exit_rooms.push_back(r);
			}
		}
		if (exit_rooms.empty()) {
			return false;
		}
		std::shuffle(exit_rooms.begin(), exit_rooms.end(), rng);
		int exit_room = -1;
		int dx, dy, dix, diy;
		for (int r : exit_rooms) {
			if (outer_cell(r, dx, dy, dix, diy)) {
				exit_room = r;
				break;
			}
		}
		if (exit_room < 0) {
			return false;
		}
		g[dy][dx] = 'E';
		reserve(dix, diy);

		// Every doorway on start -> key -> exit is blocked: a crate in the doorway, or (on an
		// island level) a water gap with a crate lined up to be pushed in as a bridge.
		std::vector<int> from_rooms;
		std::vector<int> route = door_path(start, key_room, &from_rooms);
		const std::vector<int> back = door_path(key_room, exit_room, &from_rooms);
		route.insert(route.end(), back.begin(), back.end());
		for (size_t i = 0; i < route.size(); ++i) {
			const Door &d = doors[route[i]];
			if (g[d.y][d.x] != '.') {
				continue; // already blocked (shared by both legs)
			}
			const bool gap = p.ring != 0 && std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < p.gap_chance;
			if (!(gap && water_gap(d, from_rooms[i]))) {
				g[d.y][d.x] = 'B';
			}
		}
		add_pools();

		// Aliens in a room other than the start.
		int alien_room = rnd(0, n - 1);
		for (int t = 0; t < 8 && alien_room == start; ++t) {
			alien_room = rnd(0, n - 1);
		}
		std::vector<std::pair<int, int>> aliens;
		for (int i = 0; i < p.aliens; ++i) {
			int ax, ay;
			if (free_cell(alien_room, ax, ay)) {
				g[ay][ax] = 'A';
				aliens.push_back({ ax, ay });
				reserve(ax, ay);
			}
		}

		// Box bombs a few cells away from the aliens: in the alien room or a room next to
		// it (so the puzzle is "bring it over"), never touching a wall (a box against a
		// wall can only slide along it, in a corner it is stuck for good).
		std::vector<int> bomb_rooms{ alien_room };
		for (int di : adj[alien_room]) {
			bomb_rooms.push_back(other_room(di, alien_room));
		}
		for (int i = 0; i < p.bombs; ++i) {
			for (int tries = 0; tries < 40; ++tries) {
				int bx, by;
				if (!free_cell(bomb_rooms[rnd(0, (int)bomb_rooms.size() - 1)], bx, by)) {
					continue;
				}
				if (g[by][bx - 1] == '#' || g[by][bx + 1] == '#' || g[by - 1][bx] == '#' || g[by + 1][bx] == '#') {
					continue;
				}
				int nearest = 1 << 20;
				for (auto &a : aliens) {
					nearest = std::min(nearest, std::abs(a.first - bx) + std::abs(a.second - by));
				}
				if (nearest >= 3) {
					g[by][bx] = 'X';
					reserve(bx, by);
					break;
				}
			}
		}
		// Extra crates anywhere free.
		for (int i = 0; i < p.extra_crates; ++i) {
			int cx, cy;
			if (free_cell(rnd(0, n - 1), cx, cy)) {
				g[cy][cx] = 'B';
				reserve(cx, cy);
			}
		}
		add_ice();
		make_ring();
		return true;
	}
};

} // namespace

SokobanGenerator::Output SokobanGenerator::generate(const Params &p_params) {
	Params p = p_params;
	p.width = std::max(p.width, 2 * MIN_ROOM + 3);
	p.height = std::max(p.height, MIN_ROOM + 2);
	p.rooms = std::max(1, p.rooms);
	std::mt19937 rng(p.seed);
	Output out;
	for (int attempt = 1; attempt <= p.max_attempts; ++attempt) {
		Builder b(p, rng);
		if (!b.make_rooms() || !b.populate()) {
			++out.rejected_layout;
			continue;
		}
		// --- 4. Verify ---
		SokobanSolver solver(b.g);
		SokobanSolver::Result route = solver.solve_key_and_exit();
		if (!route.solved) {
			++out.rejected_unsolvable;
			continue;
		}
		if (route.pushes < p.min_pushes) {
			++out.rejected_easy;
			continue;
		}
		SokobanSolver::Result bomb = solver.solve_bomb_to_alien();
		if (p.bombs > 0 && p.aliens > 0 && !bomb.solved) {
			++out.rejected_bomb;
			continue;
		}
		out.ok = true;
		out.rows = b.g;
		out.pushes = route.pushes;
		out.bomb_pushes = bomb.solved ? bomb.pushes : 0;
		out.attempts = attempt;
		out.states = route.states + bomb.states;
		return out;
	}
	out.attempts = p.max_attempts;
	return out;
}

} // namespace godot

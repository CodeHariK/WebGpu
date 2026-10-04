#include "sokoban_solver.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <unordered_set>

namespace godot {

static const int DX[4] = { 1, -1, 0, 0 };
static const int DY[4] = { 0, 0, 1, -1 };

static bool _contains(const std::vector<int> &p_sorted, int p_v) {
	return std::binary_search(p_sorted.begin(), p_sorted.end(), p_v);
}

static bool _has_box(const std::vector<int> &p_boxes, int p_cell) {
	auto it = std::lower_bound(p_boxes.begin(), p_boxes.end(), p_cell * 2);
	return it != p_boxes.end() && (*it >> 1) == p_cell;
}

SokobanSolver::SokobanSolver(const std::vector<std::string> &p_rows) {
	h = (int)p_rows.size();
	for (const std::string &r : p_rows) {
		w = std::max(w, (int)r.size());
	}
	base.assign((size_t)w * h, ' ');
	std::vector<int> doors;
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < (int)p_rows[y].size(); ++x) {
			const char c = p_rows[y][x];
			const int i = y * w + x;
			switch (c) {
				case '#':
					base[i] = '#';
					break;
				case 'E':
					base[i] = '#';
					doors.push_back(i);
					break;
				case 'A':
					base[i] = 'A';
					alien_cells.push_back(i);
					break;
				case 'K':
					base[i] = 'K';
					key = i;
					break;
				case '~':
				case '^':
					base[i] = c;
					break;
				case 'I':
					base[i] = 'i';
					break;
				case 'B':
				case 'X':
					base[i] = '.';
					start.boxes.push_back(i * 2 + (c == 'X' ? 1 : 0));
					break;
				case 'P':
					base[i] = '.';
					start.player = i;
					break;
				case ' ':
					break;
				default:
					base[i] = '.'; // floor, bridge
			}
		}
	}
	std::sort(start.boxes.begin(), start.boxes.end());
	for (int d : doors) {
		for (int k = 0; k < 4; ++k) {
			const int x = d % w + DX[k];
			const int y = d / w + DY[k];
			if (x >= 0 && y >= 0 && x < w && y < h && (base[y * w + x] == '.' || base[y * w + x] == 'i')) {
				exit_cells.push_back(y * w + x);
			}
		}
	}
}

bool SokobanSolver::_walk(int p_cell, const State &p_s) const {
	const char c = base[p_cell];
	return c == '.' || c == 'K' || c == 'i' || (c == '~' && _contains(p_s.filled, p_cell));
}

bool SokobanSolver::_rest(int p_cell, const State &p_s) const {
	const char c = base[p_cell];
	return c == '.' || c == 'i' || (c == '~' && _contains(p_s.filled, p_cell));
}

// Flood fill of the cells the player can walk to without pushing.
void SokobanSolver::_reach(const State &p_s, std::vector<char> &r_seen) const {
	r_seen.assign((size_t)w * h, 0);
	static thread_local std::vector<int> stack;
	stack.clear();
	stack.push_back(p_s.player);
	r_seen[p_s.player] = 1;
	while (!stack.empty()) {
		const int c = stack.back();
		stack.pop_back();
		for (int k = 0; k < 4; ++k) {
			const int x = c % w + DX[k];
			const int y = c / w + DY[k];
			if (x < 0 || y < 0 || x >= w || y >= h) {
				continue;
			}
			const int n = y * w + x;
			if (!r_seen[n] && _walk(n, p_s) && !_has_box(p_s.boxes, n)) {
				r_seen[n] = 1;
				stack.push_back(n);
			}
		}
	}
}

// One push of box p_box in direction p_dir. The box moves one cell, keeps sliding while it
// is on ice, then a liquid under it resolves: water swallows it (cell becomes floor), lava
// destroys it. Returns false if the first cell is blocked.
bool SokobanSolver::_push(const State &p_s, size_t p_box, int p_dir, State &r_next) const {
	const int from = p_s.boxes[p_box] >> 1;
	int cell = from;
	bool moved = false;
	while (true) {
		const int x = cell % w + DX[p_dir];
		const int y = cell / w + DY[p_dir];
		if (x < 0 || y < 0 || x >= w || y >= h) {
			break;
		}
		const int n = y * w + x;
		if (_has_box(p_s.boxes, n)) {
			break;
		}
		const char c = base[n];
		const bool liquid = (c == '~' && !_contains(p_s.filled, n)) || c == '^';
		if (!liquid && !_rest(n, p_s)) {
			break;
		}
		cell = n;
		moved = true;
		if (liquid || base[n] != 'i') {
			break; // sinks / burns here, or stops on normal ground
		}
	}
	if (!moved) {
		return false;
	}
	r_next = p_s;
	r_next.player = from; // the player steps into the box's old cell
	r_next.pushes = p_s.pushes + 1;
	const int kind = p_s.boxes[p_box] & 1;
	r_next.boxes.erase(r_next.boxes.begin() + p_box);
	const char c = base[cell];
	if (c == '~' && !_contains(p_s.filled, cell)) {
		r_next.filled.insert(std::upper_bound(r_next.filled.begin(), r_next.filled.end(), cell), cell);
	} else if (c != '^') {
		r_next.boxes.insert(std::upper_bound(r_next.boxes.begin(), r_next.boxes.end(), cell * 2 + kind), cell * 2 + kind);
	}
	return true;
}

bool SokobanSolver::_bomb_goal(const State &p_s) const {
	for (int b : p_s.boxes) {
		if (!(b & 1)) {
			continue;
		}
		const int c = b >> 1;
		for (int a : alien_cells) {
			if (std::abs(a % w - c % w) + std::abs(a / w - c / w) == 1) {
				return true;
			}
		}
	}
	return false;
}

static uint64_t _hash(const std::vector<int> &p_boxes, const std::vector<int> &p_filled, int p_player) {
	uint64_t h = 1469598103934665603ull; // FNV-1a (verification only: a rare collision is harmless)
	for (int v : p_boxes) {
		h = (h ^ (uint64_t)(uint32_t)v) * 1099511628211ull;
	}
	h = (h ^ 0xffffffffull) * 1099511628211ull; // separator
	for (int v : p_filled) {
		h = (h ^ (uint64_t)(uint32_t)v) * 1099511628211ull;
	}
	return (h ^ (uint64_t)(uint32_t)p_player) * 1099511628211ull;
}

SokobanSolver::Result SokobanSolver::_search(
		const State &p_start,
		Goal p_goal,
		const std::vector<int> &p_targets,
		int p_max_states,
		State *r_end
) const {
	Result res;
	std::deque<State> queue;
	std::unordered_set<uint64_t> seen; // by canonical player region
	std::unordered_set<uint64_t> queued; // cheap pre-dedupe by raw player cell
	std::vector<char> reach;
	queue.push_back(p_start);
	// Duplicates are dropped when popped; the queue cap bounds memory on huge levels.
	while (!queue.empty() && res.states < p_max_states && (int)queue.size() < p_max_states * 8) {
		State s = std::move(queue.front());
		queue.pop_front();
		_reach(s, reach);
		int norm = s.player; // canonical player = smallest reachable cell
		for (int i = 0; i < (int)reach.size(); ++i) {
			if (reach[i]) {
				norm = i;
				break;
			}
		}
		if (!seen.insert(_hash(s.boxes, s.filled, norm)).second) {
			continue;
		}
		++res.states;

		int end_cell = -1;
		if (p_goal == GOAL_BOMB) {
			end_cell = _bomb_goal(s) ? s.player : -1;
		} else {
			for (int t : p_targets) {
				if (reach[t]) {
					end_cell = t;
					break;
				}
			}
		}
		if (end_cell >= 0) {
			res.solved = true;
			res.pushes = s.pushes;
			if (r_end) {
				*r_end = s;
				r_end->player = end_cell;
			}
			return res;
		}

		for (size_t bi = 0; bi < s.boxes.size(); ++bi) {
			const int c = s.boxes[bi] >> 1;
			for (int k = 0; k < 4; ++k) {
				const int px = c % w - DX[k];
				const int py = c / w - DY[k];
				if (px < 0 || py < 0 || px >= w || py >= h || !reach[py * w + px]) {
					continue;
				}
				State next;
				if (_push(s, bi, k, next) && queued.insert(_hash(next.boxes, next.filled, next.player)).second) {
					queue.push_back(std::move(next));
				}
			}
		}
	}
	return res;
}

SokobanSolver::Result SokobanSolver::solve_key_and_exit(int p_max_states) const {
	if (start.player < 0 || key < 0 || exit_cells.empty()) {
		return Result();
	}
	State at_key;
	Result a = _search(start, GOAL_CELL, { key }, p_max_states, &at_key);
	if (!a.solved) {
		return a;
	}
	at_key.pushes = 0;
	Result b = _search(at_key, GOAL_CELL, exit_cells, p_max_states, nullptr);
	Result r;
	r.solved = b.solved;
	r.pushes = b.solved ? a.pushes + b.pushes : -1;
	r.states = a.states + b.states;
	return r;
}

SokobanSolver::Result SokobanSolver::solve_bomb_to_alien(int p_max_states) const {
	if (start.player < 0 || alien_cells.empty()) {
		return Result();
	}
	return _search(start, GOAL_BOMB, {}, p_max_states, nullptr);
}

} // namespace godot

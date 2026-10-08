## Splits a TacticalGrid into regions: cells that can all reach each other (strongly connected
## over the links — a DROP is one-way, so a crate top you can only jump down from is its own
## region). Two cells in the same region: there's a way there and a way back.
## Kosaraju's algorithm, both passes iterative: finish order on the links, then flood the
## reversed links in reverse finish order.
class_name GridRegions
extends RefCounted


## Region id per cell (ids are just labels: 0, 1, 2 …).
static func compute(grid: TacticalGrid) -> PackedInt32Array:
	var count := grid.cell_count()
	var order := _finish_order(grid, count)
	var reverse := _reverse_links(grid, count)
	var region := PackedInt32Array()
	region.resize(count)
	region.fill(-1)
	var next_id := 0
	for i in range(count - 1, -1, -1):
		var root := order[i]
		if region[root] >= 0:
			continue
		var stack := PackedInt32Array([root])
		region[root] = next_id
		while not stack.is_empty():
			var cell := stack[stack.size() - 1]
			stack.resize(stack.size() - 1)
			for from in reverse[cell]:
				if region[from] < 0:
					region[from] = next_id
					stack.append(from)
		next_id += 1
	return region


# Cells in the order a depth-first walk over the links finishes them.
static func _finish_order(grid: TacticalGrid, count: int) -> PackedInt32Array:
	var order := PackedInt32Array()
	var visited := PackedByteArray()
	visited.resize(count)
	for start in count:
		if visited[start] == 1:
			continue
		visited[start] = 1
		var stack: Array[Vector2i] = [Vector2i(start, 0)] # (cell, next link to try)
		while not stack.is_empty():
			var top := stack[stack.size() - 1]
			var cell_links: Array = grid.links[top.x]
			if top.y < cell_links.size():
				stack[stack.size() - 1] = Vector2i(top.x, top.y + 1)
				var next: int = cell_links[top.y][0]
				if visited[next] == 0:
					visited[next] = 1
					stack.append(Vector2i(next, 0))
			else:
				stack.pop_back()
				order.append(top.x)
	return order


# Per cell, the cells with a link into it. (Plain Arrays: a packed array in an Array is a value,
# so appending to reverse[i] would append to a copy.)
static func _reverse_links(grid: TacticalGrid, count: int) -> Array:
	var reverse := []
	reverse.resize(count)
	for cell in count:
		reverse[cell] = []
	for cell in count:
		for link: Array in grid.links[cell]:
			reverse[link[0]].append(cell)
	return reverse

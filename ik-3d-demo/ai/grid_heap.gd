## A binary min-heap of (id, priority): pop() always returns the id with the smallest priority.
## A* keeps its open set in one. Parallel packed arrays, so pushing and popping allocate nothing.
class_name GridHeap
extends RefCounted

var _ids := PackedInt32Array()
var _priorities := PackedFloat32Array()


func is_empty() -> bool:
	return _ids.is_empty()


func push(id: int, priority: float) -> void:
	_ids.append(id)
	_priorities.append(priority)
	_sift_up(_ids.size() - 1)


## Remove and return the id with the smallest priority. Don't call when empty.
func pop() -> int:
	var top := _ids[0]
	var last := _ids.size() - 1
	_swap(0, last)
	_ids.resize(last)
	_priorities.resize(last)
	_sift_down(0)
	return top


# Move entry i up while it's smaller than its parent.
func _sift_up(i: int) -> void:
	while i > 0:
		var parent := (i - 1) / 2
		if _priorities[parent] <= _priorities[i]:
			return
		_swap(i, parent)
		i = parent


# Move entry i down while a child is smaller.
func _sift_down(i: int) -> void:
	var count := _ids.size()
	while true:
		var smallest := i
		var left := 2 * i + 1
		var right := left + 1
		if left < count and _priorities[left] < _priorities[smallest]:
			smallest = left
		if right < count and _priorities[right] < _priorities[smallest]:
			smallest = right
		if smallest == i:
			return
		_swap(i, smallest)
		i = smallest


func _swap(a: int, b: int) -> void:
	var id := _ids[a]
	_ids[a] = _ids[b]
	_ids[b] = id
	var priority := _priorities[a]
	_priorities[a] = _priorities[b]
	_priorities[b] = priority

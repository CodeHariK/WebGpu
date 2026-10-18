## Drag a curve's control points (tuning panel open, a preview on, curves shown with C): left-drag any
## point of a blade's path (L1… R7) and the move's curve bends there; the preview replays to the same
## moment as you drag, so you see the change at once. Save (on the panel) keeps the edits.
##
## An edit is an offset per path point, per move and phase ("slam/yank/2"), stored in the attack's own
## frame — x out to the blade's side, y up, z forward along your facing — so it applies wherever you
## aim, and mirrored: the left and right blades bend the same way on their own sides (except on a
## curve both blades share, like a pull's yank: there both get the same bend). Every path a
## blade starts (and every live path a move rebuilds) gets its offsets added (BladesDemo.shape_path).
## A point where two pieces join moves with its twin and both its handles, so the join stays smooth.
## The first point (where the blade is now) can't be moved.
class_name CurveEditor
extends RefCounted

const PICK_PIXELS := 14.0 ## a point this close to the mouse on screen can be grabbed

var hover := Vector2i(-1, -1) ## (blade, point) under the mouse
var offsets := {} ## "move/phase/point" → Vector3 offset in the attack's frame

var _demo: BladesDemo
var _drag := Vector2i(-1, -1) ## (blade, point) being dragged
var _plane := Plane() ## the drag moves the point in this plane (facing the camera)


func _init(demo: BladesDemo) -> void:
	_demo = demo


## Whether the editor is on: panel open, curves shown, a preview running or paused.
func active() -> bool:
	return _demo.debug.enabled and _demo.preview.active and _demo.is_editing()


## Mouse input; true when it grabbed / dragged / dropped a point (so it isn't also a throw).
func handle(event: InputEvent) -> bool:
	if not active():
		hover = Vector2i(-1, -1)
		_drag = Vector2i(-1, -1)
		return false
	if event is InputEventMouseMotion:
		if _drag.x >= 0:
			_move(event.position)
			return true
		hover = _pick(event.position)
		return false
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		if event.pressed:
			var picked := _pick(event.position)
			if picked.x < 0:
				return false
			_drag = picked
			var camera := _demo.camera
			_plane = Plane(camera.global_basis.z, _demo.blades[picked.x].path[picked.y])
			return true
		if _drag.x >= 0:
			_drag = Vector2i(-1, -1)
			return true
	return false


## The offsets a path gets: every point of `path` with an edit for this blade's move and phase.
func shape(
		blade: Blade,
		path: PackedVector3Array
) -> PackedVector3Array:
	if offsets.is_empty() or blade.phase == Blade.IDLE or blade.phase == Blade.HOLD:
		return path
	var prefix := _prefix(blade)
	var frame := frame_of(blade)
	var out := path.duplicate()
	for i in path.size():
		var key := prefix + str(i)
		if offsets.has(key):
			out[i] += frame * (offsets[key] as Vector3)
	return out


## The attack's frame for a blade: x out to its own side, y up, z forward along your facing.
## A curve both blades share (a pull's or slam's yank, a whirl: the enemy rides it) is not mirrored —
## both get the same offsets, so they stay on one curve.
func frame_of(blade: Blade) -> Basis:
	var shared := blade.phase == Blade.YANK or blade.phase == Blade.WHIRL
	return Basis(_demo.right() * (1.0 if shared else blade.side), Vector3.UP, _demo.facing)


## Forget the edits of one move (all its phases).
func clear(move: String) -> void:
	for key: String in offsets.keys():
		if key.begins_with(move + "/"):
			offsets.erase(key)


## Mark the points: the hovered / dragged one big, the edited ones orange.
func draw_debug(debug: BladeDebug) -> void:
	if not active():
		return
	for blade in _demo.blades:
		if blade.phase == Blade.IDLE or blade.phase == Blade.HOLD:
			continue
		var prefix := _prefix(blade)
		for i in blade.path.size():
			if offsets.has(prefix + str(i)):
				debug.cross(blade.path[i], Color(1.0, 0.5, 0.1), 0.14)
	var at := _drag if _drag.x >= 0 else hover
	if at.x >= 0 and at.y < _demo.blades[at.x].path.size():
		debug.cross(_demo.blades[at.x].path[at.y], Color(1.0, 0.2, 0.9), 0.3)


func _prefix(blade: Blade) -> String:
	return "%s/%s/" % [blade.move, Blade.PHASE_NAMES[blade.phase]]


# The control point nearest the mouse on screen (not a path's first point); (-1, -1): none.
func _pick(mouse: Vector2) -> Vector2i:
	var camera := _demo.camera
	var best := Vector2i(-1, -1)
	var best_pixels := PICK_PIXELS
	for b in _demo.blades.size():
		var blade := _demo.blades[b]
		if blade.phase == Blade.IDLE or blade.phase == Blade.HOLD:
			continue
		for i in range(1, blade.path.size()):
			if camera.is_position_behind(blade.path[i]):
				continue
			var pixels := camera.unproject_position(blade.path[i]).distance_to(mouse)
			if pixels < best_pixels:
				best = Vector2i(b, i)
				best_pixels = pixels
	return best


# Move the dragged point to under the mouse (in its camera-facing plane): add the move to its offset
# (and to its twin / handles at a join), then replay the preview to the same moment.
func _move(mouse: Vector2) -> void:
	var camera := _demo.camera
	var hit = _plane.intersects_ray(camera.project_ray_origin(mouse), camera.project_ray_normal(mouse))
	if hit == null:
		return
	var blade := _demo.blades[_drag.x]
	if _drag.y >= blade.path.size():
		return
	var change: Vector3 = frame_of(blade).inverse() * ((hit as Vector3) - blade.path[_drag.y])
	var prefix := _prefix(blade)
	for i in _linked(blade.path.size(), _drag.y):
		var key := prefix + str(i)
		offsets[key] = (offsets.get(key, Vector3.ZERO) as Vector3) + change
	_demo.preview.refresh()


# The points that move together with point i: at a join of two pieces (…3 / 4…) the shared point's
# twin and both handles beside it (so the curve stays smooth through it); else just i.
static func _linked(
		size: int,
		i: int
) -> Array[int]:
	var join := -1
	if i % 4 == 3 and i + 1 < size:
		join = i
	elif i % 4 == 0 and i > 0:
		join = i - 1
	if join < 0:
		return [i]
	return [join - 1, join, join + 1, join + 2]

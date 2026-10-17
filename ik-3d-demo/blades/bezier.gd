## Cubic Bezier helpers. A cubic Bezier is 4 points: it starts at p0 heading toward p1, ends at p3
## arriving from p2's direction, and never passes through p1 / p2 (they are handles that pull the
## curve). t = 0..1 runs along it, but not at even speed: points bunch where the curve bends.
class_name Bezier
extends RefCounted

## A quarter circle as one cubic Bezier: its two handles lie along the circle's tangents at its
## ends, this × the radius long (4/3 · tan(π/8) ≈ 0.5523: off the true circle by under 0.03 %).
const QUARTER_CIRCLE := 0.5523


static func point(
		p0: Vector3,
		p1: Vector3,
		p2: Vector3,
		p3: Vector3,
		t: float
) -> Vector3:
	var u := 1.0 - t
	return p0 * (u * u * u) + p1 * (3.0 * u * u * t) + p2 * (3.0 * u * t * t) + p3 * (t * t * t)


static func tangent(
		p0: Vector3,
		p1: Vector3,
		p2: Vector3,
		p3: Vector3,
		t: float
) -> Vector3:
	var u := 1.0 - t
	return (p1 - p0) * (3.0 * u * u) + (p2 - p1) * (6.0 * u * t) + (p3 - p2) * (3.0 * t * t)


## `count` points spaced evenly ALONG the curve (by length, not by t), so chain links never bunch:
## walk the curve in small steps, measure, then place each point at its share of the length.
static func even_points(
		p0: Vector3,
		p1: Vector3,
		p2: Vector3,
		p3: Vector3,
		count: int,
		samples := 48
) -> PackedVector3Array:
	var raw := PackedVector3Array()
	var lengths := PackedFloat32Array()
	raw.resize(samples + 1)
	lengths.resize(samples + 1)
	var total := 0.0
	for i in samples + 1:
		raw[i] = point(p0, p1, p2, p3, float(i) / samples)
		if i > 0:
			total += raw[i].distance_to(raw[i - 1])
		lengths[i] = total
	var out := PackedVector3Array()
	out.resize(count)
	var j := 1
	for k in count:
		var wanted := total * k / maxf(count - 1, 1)
		while j < samples and lengths[j] < wanted:
			j += 1
		var span := lengths[j] - lengths[j - 1]
		var f := (wanted - lengths[j - 1]) / span if span > 1e-6 else 0.0
		out[k] = raw[j - 1].lerp(raw[j], clampf(f, 0.0, 1.0))
	return out


## The part of the curve from t = 0 to t = `at`, as its own 4 control points (de Casteljau split:
## lerp the control points at `at` three times; the in-between points are the new handles).
## The chain uses it to lie exactly on the path the blade has travelled so far.
static func split_start(
		c: PackedVector3Array,
		at: float
) -> PackedVector3Array:
	var a := c[0].lerp(c[1], at)
	var b := c[1].lerp(c[2], at)
	var d := c[2].lerp(c[3], at)
	var ab := a.lerp(b, at)
	var bd := b.lerp(d, at)
	return PackedVector3Array([c[0], a, ab, ab.lerp(bd, at)])


## The part from t = `at` to t = 1 (the path still ahead).
static func split_end(
		c: PackedVector3Array,
		at: float
) -> PackedVector3Array:
	var a := c[0].lerp(c[1], at)
	var b := c[1].lerp(c[2], at)
	var d := c[2].lerp(c[3], at)
	var ab := a.lerp(b, at)
	var bd := b.lerp(d, at)
	return PackedVector3Array([ab.lerp(bd, at), bd, d, c[3]])


# --- paths: several cubic Beziers end to end, 4 control points each, played as one (e = 0..1) ---

static func segment_count(path: PackedVector3Array) -> int:
	return path.size() / 4


## Which segment `e` falls in, and how far along it (0..1).
static func locate(
		path: PackedVector3Array,
		e: float
) -> Vector2:
	var segments := segment_count(path)
	var at := clampf(e, 0.0, 1.0) * segments
	var s := mini(int(at), segments - 1)
	return Vector2(s, at - s)


static func path_point(
		path: PackedVector3Array,
		e: float
) -> Vector3:
	var where := locate(path, e)
	var k := int(where.x) * 4
	return point(path[k], path[k + 1], path[k + 2], path[k + 3], where.y)


static func path_tangent(
		path: PackedVector3Array,
		e: float
) -> Vector3:
	var where := locate(path, e)
	var k := int(where.x) * 4
	return tangent(path[k], path[k + 1], path[k + 2], path[k + 3], where.y)


## `count` points spaced evenly by length along the path between e0 and e1 (across segments).
static func path_even_points(
		path: PackedVector3Array,
		e0: float,
		e1: float,
		count: int,
		samples := 64
) -> PackedVector3Array:
	var raw := PackedVector3Array()
	var lengths := PackedFloat32Array()
	raw.resize(samples + 1)
	lengths.resize(samples + 1)
	var total := 0.0
	for i in samples + 1:
		raw[i] = path_point(path, lerpf(e0, e1, float(i) / samples))
		if i > 0:
			total += raw[i].distance_to(raw[i - 1])
		lengths[i] = total
	var out := PackedVector3Array()
	out.resize(count)
	var j := 1
	for k in count:
		var wanted := total * k / maxf(count - 1, 1)
		while j < samples and lengths[j] < wanted:
			j += 1
		var span := lengths[j] - lengths[j - 1]
		var f := (wanted - lengths[j - 1]) / span if span > 1e-6 else 0.0
		out[k] = raw[j - 1].lerp(raw[j], clampf(f, 0.0, 1.0))
	return out


## A smooth path through every one of `points` (Catmull-Rom): one cubic Bezier per gap. At each
## point the curve's direction is the line from the point before to the point after, so the
## pieces join without kinks; the handles are that direction / 3 either side of the point.
## Any polyline — a circle of points, a spiral, a hand-placed route — becomes a path this way.
static func from_points(points: PackedVector3Array) -> PackedVector3Array:
	var path := PackedVector3Array()
	var n := points.size()
	if n < 2:
		return path
	var tangents := PackedVector3Array()
	tangents.resize(n)
	for i in n:
		var before := points[maxi(i - 1, 0)]
		var after := points[mini(i + 1, n - 1)]
		tangents[i] = (after - before) * (0.5 if i > 0 and i < n - 1 else 1.0)
	for i in n - 1:
		path.append(points[i])
		path.append(points[i] + tangents[i] / 3.0)
		path.append(points[i + 1] - tangents[i + 1] / 3.0)
		path.append(points[i + 1])
	return path

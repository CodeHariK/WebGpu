## The player's visible body: any body the demo can switch between (B). It poses itself from the
## shared BodyPose each frame and says where the chains leave its hands.
##
## Debug view (C), drawn by every body:
##   feet     grey arrow = where the mouse says to face, dim blue = the heading spring's goal,
##            blue arrow = where the body faces right now (it lags the goal: heavy blades)
##   chest    red arrow = lean (×2 so it shows), magenta arrow = the chest's facing after twist
##   hands    grey cross = the blade path's fixed hand anchor, yellow cross = the grip the chain
##            really leaves from (the line between them is how far the arm is off the path's anchor)
##   arms     green = the IK's solved arm (shoulder → elbow → wrist), dim white = the clip's arm,
##            red cross = the reach target, cyan cross + line = the pole the elbow bends toward
##   head     yellow arrow = where the head is told to look (between the blades, or the mouse)
##   label    heading / lean / twist and each arm's weight (0 clip … 1 IK)
class_name PlayerBody
extends Node3D

const DEBUG_SOLVED := Color(0.3, 1.0, 0.4)
const DEBUG_ANIMATED := Color(1.0, 1.0, 1.0, 0.35)
const DEBUG_TARGET := Color(1.0, 0.3, 0.3)
const DEBUG_POLE := Color(0.3, 0.85, 1.0)

var demo: BladesDemo
var pose: BodyPose


func setup(owner: BladesDemo) -> void:
	demo = owner
	pose = BodyPose.new(owner)


func reset() -> void:
	pass


func update(_delta: float) -> void:
	pass


## Where the chain leaves this side's hand (side −1 left, +1 right).
func grip(_side: float) -> Vector3:
	return Vector3.ZERO


## Where the lean and twist arrows start (about the chest).
func chest() -> Vector3:
	return demo.player + Vector3.UP * 1.3


## About the head (where the look arrow starts).
func head() -> Vector3:
	return chest() + Vector3.UP * 0.5


## The debug view of the body (see the header for the colours).
func draw_debug(debug: BladeDebug) -> void:
	var feet := demo.player + Vector3.UP * 0.05
	debug.arrow(feet, feet + demo.facing * 1.5, Color(0.6, 0.6, 0.6))
	debug.line(feet, feet + pose.goal_forward() * 1.3, Color(0.3, 0.5, 1.0, 0.5))
	debug.arrow(feet, feet + pose.forward() * 1.1, Color(0.3, 0.55, 1.0))
	var c := chest()
	debug.cross(c, Color.WHITE, 0.05)
	debug.arrow(c, c + pose.lean * 2.0, Color(1.0, 0.3, 0.3))
	debug.arrow(c, c + pose.forward().rotated(Vector3.UP, pose.twist) * 0.6, Color(1.0, 0.3, 0.9))
	debug.arrow(head(), head() + pose.look * 1.2, Color(1.0, 0.9, 0.2))
	for i in 2:
		var side := -1.0 if i == 0 else 1.0
		var anchor := demo.hand(side)
		debug.cross(anchor, Color(0.6, 0.6, 0.6), 0.05)
		debug.line(anchor, grip(side), Color(0.6, 0.6, 0.6, 0.6))
		debug.cross(grip(side), Color(1.0, 0.95, 0.2), 0.06)
		_draw_arm_debug(debug, i)
	debug.label(c + Vector3.UP * 0.9, "heading %d°  lean %.2f  twist %.2f\narm L %.2f   R %.2f" % [
		roundi(rad_to_deg(wrapf(pose.heading, -PI, PI))), pose.lean.length(), pose.twist,
		pose.arm_weight[0], pose.arm_weight[1]], Color(0.75, 0.85, 1.0))


## Each body draws arm `i` (0 left, 1 right) with draw_arm (and its clip arm, if it has one).
func _draw_arm_debug(
		_debug: BladeDebug,
		_i: int
) -> void:
	pass


## One arm: the solved shoulder → elbow → wrist, the reach target and the pole.
func draw_arm(
		debug: BladeDebug,
		i: int,
		points: PackedVector3Array,
		target: Vector3,
		pole: Vector3
) -> void:
	if points.size() < 3:
		return
	var side_name := "L" if i == 0 else "R"
	debug.line(points[0], points[1], DEBUG_SOLVED)
	debug.line(points[1], points[2], DEBUG_SOLVED)
	for p in points:
		debug.cross(p, DEBUG_SOLVED, 0.03)
	debug.cross(target, DEBUG_TARGET, 0.06)
	debug.line(points[2], target, Color(DEBUG_TARGET, 0.5)) # the miss (out of reach, or the spring lagging)
	debug.cross(pole, DEBUG_POLE, 0.06)
	debug.line(points[1], pole, Color(DEBUG_POLE, 0.6))
	debug.label(pole + Vector3.UP * 0.1, "pole " + side_name, DEBUG_POLE)
	debug.label(target + Vector3.UP * 0.1, "target " + side_name, DEBUG_TARGET)


## An arm as a dim polyline (the clip's arm under the IK).
func draw_animated_arm(
		debug: BladeDebug,
		points: PackedVector3Array
) -> void:
	for k in range(1, points.size()):
		debug.line(points[k - 1], points[k], DEBUG_ANIMATED)

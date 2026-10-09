## On-screen performance numbers while the demo runs (bottom-right), plus an on-demand GPU measurement.
##
## Live (updated 4× a second, cheap — read from Godot's Performance monitors):
##   FPS · CPU frame time · draw calls · triangles · video memory · GPU time (Godot's GPU timer —
##   it works on Vulkan/Android, reads 0 on Metal/macOS, so it's hidden when it reports nothing)
## On demand (measure_gpu): draws N frames back to back as fast as possible and times them, so the
##   result is "how long the GPU needs for this view" even where the GPU timer is missing. It freezes
##   the picture for a moment, which is why it isn't run every frame.
class_name PerfHud
extends CanvasLayer

var label := Label.new()
var last_measure := ""
var _accum := 0.0
var _settle := 0.0  # seconds to ignore FPS/CPU after a measurement (the burst itself stalls a frame)


func _ready() -> void:
	layer = 10
	var panel := PanelContainer.new()  # bottom-right corner, growing up and to the left
	panel.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_RIGHT, Control.PRESET_MODE_MINSIZE, 12)
	panel.grow_horizontal = Control.GROW_DIRECTION_BEGIN
	panel.grow_vertical = Control.GROW_DIRECTION_BEGIN
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	panel.add_child(label)
	add_child(panel)
	RenderingServer.viewport_set_measure_render_time(get_viewport().get_viewport_rid(), true)


func _process(delta: float) -> void:
	_accum += delta
	_settle -= delta
	if _accum < 0.25:
		return
	_accum = 0.0
	var lines := PackedStringArray()
	if _settle > 0.0:
		lines.append("… settling after measurement")
	else:
		lines.append("%d FPS" % Engine.get_frames_per_second())
		lines.append("CPU (scripts) %.2f ms" % (Performance.get_monitor(Performance.TIME_PROCESS) * 1000.0))
	var gpu := RenderingServer.viewport_get_measured_render_time_gpu(get_viewport().get_viewport_rid())
	if gpu > 0.0:
		lines.append("GPU %.2f ms" % gpu)
	lines.append("%d draw calls · %dk triangles" % [
		Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME),
		Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME) / 1000])
	lines.append("video memory %.0f MB" % (Performance.get_monitor(Performance.RENDER_VIDEO_MEM_USED) / 1048576.0))
	if last_measure != "":
		lines.append(last_measure)
	lines.append("P: measure GPU · B: bench all steps → perf.md · H: hide")
	label.text = "\n".join(lines)


## Time `frames` frames drawn back to back. The GPU keeps only a few frames in flight, so each
## force_draw soon has to wait for the GPU; total / frames ≈ GPU time per frame for this view.
func measure_gpu(frames: int = 120) -> float:
	for f in 3:
		RenderingServer.force_draw(false)  # warm-up
	RenderingServer.force_sync()
	var start := Time.get_ticks_usec()
	for f in frames:
		RenderingServer.force_draw(false)
	RenderingServer.force_sync()
	return (Time.get_ticks_usec() - start) / 1000.0 / frames


## Measure the current view and keep the result on screen.
func measure_now() -> void:
	var ms := measure_gpu()
	last_measure = "this view: %.2f ms GPU per frame (%.0f FPS if GPU-bound)" % [ms, 1000.0 / ms]
	_accum = 1.0
	_settle = 1.5

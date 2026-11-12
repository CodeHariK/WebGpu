class_name DebugPanel
extends CanvasLayer
## What the frame costs, and each speed-up switched on or off to see what it saves:
##   occlusion culling   skip what is hidden behind the hills (the terrain's occluders)
##   visibility ranges   stop drawing each kind past its distance; far ground coarse
##   mesh LOD            draw far batches with the simpler meshes Godot made on import
##   shadows             the sun's shadows (only the big kinds cast them, only to 80 m)
##   show occluders      draw the occluders instead of the scene (Viewport debug draw)
##   near / far grass    the grass's two rings (GrassField)
##   props               the trees, bushes, mushrooms, ... (PropScatter)
## Under them, the grass's sliders (GrassSliders): density, wind, colour.

const SWITCHES := ["occlusion culling", "visibility ranges", "mesh LOD", "shadows", "show occluders", "near grass", "far grass", "props"]

@export var terrain: Terrain
@export var props: PropScatter
@export var sun: DirectionalLight3D
@export var grass: GrassField

var state := {"occlusion culling": true, "visibility ranges": true, "mesh LOD": true, "shadows": true, "show occluders": false,
		"near grass": true, "far grass": true, "props": true}
var _stats := Label.new()
var _buttons := {}
var _wait := 0.0


func _ready() -> void:
	var panel := PanelContainer.new()
	panel.position = Vector2(12, 12)
	var column := VBoxContainer.new()
	panel.add_child(column)
	_stats.add_theme_font_size_override(&"font_size", 12)
	column.add_child(_stats)
	for name: String in SWITCHES:
		var button := Button.new()
		button.add_theme_font_size_override(&"font_size", 12)
		button.focus_mode = Control.FOCUS_NONE
		button.pressed.connect(func() -> void: apply(name, not state[name]))
		column.add_child(button)
		_buttons[name] = button
	var help := Label.new()
	help.text = "right mouse: look   WASD: move   Q / E: down / up   Shift: fast"
	help.add_theme_font_size_override(&"font_size", 11)
	column.add_child(help)
	var sliders := GrassSliders.new()
	sliders.grass = grass
	column.add_child(sliders)
	add_child(panel)
	for name: String in SWITCHES:
		apply(name, state[name])


## Switch one speed-up on or off.
func apply(p_name: String, p_on: bool) -> void:
	state[p_name] = p_on
	var viewport := get_viewport()
	match p_name:
		"occlusion culling":
			viewport.use_occlusion_culling = p_on
		"visibility ranges":
			terrain.set_ranges(p_on)
			props.set_ranges(p_on)
		"mesh LOD":
			viewport.mesh_lod_threshold = 1.0 if p_on else 0.0
		"shadows":
			sun.shadow_enabled = p_on
		"show occluders":
			viewport.debug_draw = Viewport.DEBUG_DRAW_OCCLUDERS if p_on else Viewport.DEBUG_DRAW_DISABLED
		"near grass":
			grass.near.visible = p_on
		"far grass":
			grass.far.visible = p_on
		"props":
			props.visible = p_on
	_buttons[p_name].text = "%s: %s" % [p_name, "on" if p_on else "off"]


func _process(p_delta: float) -> void:
	_wait -= p_delta
	if _wait > 0.0:
		return
	_wait = 0.25
	_stats.text = stats_text()


## The frame's numbers, as the panel shows them.
func stats_text() -> String:
	var fps := Engine.get_frames_per_second()
	return "%d FPS (%.1f ms)\n%d draw calls   %d objects   %.2fM triangles\n%d props in %d batches, %d terrain chunks\ngrass: %dk blades in %d near tiles, %dk tufts in %d far tiles" % [
			fps, 1000.0 / maxf(fps, 1.0),
			Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME),
			Performance.get_monitor(Performance.RENDER_TOTAL_OBJECTS_IN_FRAME),
			Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME) / 1000000.0,
			props.placed, props.batches.size(), terrain.chunks.size(),
			grass.instance_count(grass.near) / 1000, grass.near.tiles.size(),
			grass.instance_count(grass.far) / 1000, grass.far.tiles.size()]

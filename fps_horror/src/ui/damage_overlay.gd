## Red veins creeping over the screen as you get hurt. Their VeinsOverlay texture is dark veins on
## white, so a tiny shader turns "how dark" into "how red and opaque", and adds a red vignette at
## the edges. Strength comes from player_health.gd's `overlay` (0 = nothing, 3+ = full).
## Hidden at 0, so it costs nothing at full health.
##
## Mobile: one full-screen quad with a 2-line shader, only while hurt. (Theirs was a 3D screen
## shader on a quad in front of the camera; same idea.)
extends CanvasLayer

const VEINS := preload("res://assets/effects/textures/VeinsOverlay.png")
const SHADER_CODE := """
shader_type canvas_item;
uniform float strength = 0.0;
void fragment() {
	float veins = 1.0 - texture(TEXTURE, UV).r;
	float edge = smoothstep(0.3, 0.75, length(UV - 0.5));
	COLOR = vec4(0.5, 0.02, 0.02, clamp((veins * 1.5 + edge * 0.7) * strength, 0.0, 0.95));
}
"""

var health: Node  ## a player_health.gd

var _rect := TextureRect.new()
var _material := ShaderMaterial.new()


func _ready() -> void:
	layer = 5
	var shader := Shader.new()
	shader.code = SHADER_CODE
	_material.shader = shader
	_rect.material = _material
	_rect.texture = VEINS
	_rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	_rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_rect.stretch_mode = TextureRect.STRETCH_SCALE
	_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_rect.visible = false
	add_child(_rect)


func _process(_delta: float) -> void:
	if health == null:
		return
	var strength := clampf(health.overlay / 3.0, 0.0, 1.0)
	_rect.visible = strength > 0.01
	_material.set_shader_parameter("strength", strength)

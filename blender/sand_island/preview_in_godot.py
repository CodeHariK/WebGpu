"""Render sand_island.glb with sand.gdshader + water.gdshader in Godot and save a screenshot.

Makes a throwaway Godot project in /tmp, imports the glb, puts the shaders on the terrain and the
water, adds a sun, sky colour and an Animal-Crossing-style high camera, renders 30 frames and saves
godot_preview.png next to this script.  Usage (macOS): python3 preview_in_godot.py [seconds_of_time]
"""
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path

GODOT = "/Applications/Godot.app/Contents/MacOS/Godot"
HERE = Path(__file__).resolve().parent
PROJECT = Path("/tmp/sand_island_preview")

MAIN = '''
extends Node3D
var frames := 0
func _ready() -> void:
	var island: Node3D = load("res://sand_island.glb").instantiate()
	add_child(island)
	var sand := ShaderMaterial.new(); sand.shader = load("res://sand.gdshader")
	var sea := ShaderMaterial.new(); sea.shader = load("res://water.gdshader")
	for m in island.find_children("*", "MeshInstance3D", true, false):
		if m.name.begins_with("Terrain"): m.material_override = sand
		elif m.name.begins_with("Water"): m.material_override = sea
	var sun := DirectionalLight3D.new()
	sun.shadow_enabled = true
	sun.rotation_degrees = Vector3(-55, 30, 0)
	sun.light_color = Color(1.0, 0.97, 0.9)
	add_child(sun)
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.62, 0.82, 0.98)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.8, 0.85, 1.0)
	env.ambient_light_energy = 0.55
	var we := WorldEnvironment.new(); we.environment = env; add_child(we)
	var cam := Camera3D.new(); add_child(cam)
	cam.fov = 40
	cam.position = Vector3(-4.0, 11.0, 19.0); cam.look_at(Vector3(0.5, 0.2, 3.0))
func _process(_d: float) -> void:
	frames += 1
	if frames == 30:
		get_viewport().get_texture().get_image().save_png("res://shot.png")
		get_tree().quit()
'''


def main():
    shutil.rmtree(PROJECT, ignore_errors=True)
    PROJECT.mkdir(parents=True)
    for name in ("sand_island.glb", "sand.gdshader", "water.gdshader"):
        shutil.copy(HERE / name, PROJECT / name)
    (PROJECT / "project.godot").write_text(textwrap.dedent('''
        config_version=5
        [application]
        config/name="sand island preview"
        run/main_scene="res://main.tscn"
        [display]
        window/size/viewport_width=1280
        window/size/viewport_height=720
        [rendering]
        renderer/rendering_method="mobile"
        anti_aliasing/quality/msaa_3d=2
    '''))
    (PROJECT / "main.gd").write_text(MAIN)
    (PROJECT / "main.tscn").write_text('[gd_scene load_steps=2 format=3]\n[ext_resource type="Script" path="res://main.gd" id="1"]\n'
                                       '[node name="Main" type="Node3D"]\nscript = ExtResource("1")\n')
    subprocess.run([GODOT, "--headless", "--path", str(PROJECT), "--import"], check=True, capture_output=True, timeout=180)
    run = subprocess.run([GODOT, "--path", str(PROJECT)], capture_output=True, text=True, timeout=180)
    if run.stderr.strip():
        print(run.stderr)
    shutil.copy(PROJECT / "shot.png", HERE / "godot_preview.png")
    return run.stderr


if __name__ == "__main__":
    sys.exit(1 if main().strip() else 0)

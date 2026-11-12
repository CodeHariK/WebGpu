extends Node3D
## The fall forest test: a chunked terrain, grass round the camera (GrassField), the fall asset
## pack's props (PropScatter), and the ways to keep it fast switched from the debug panel
## (DebugPanel).
## `-- --capture` / `-- --diagnose` (or an export with the "capture" / "diagnose" feature) time
## it instead and quit (Capture).

@onready var camera: FlyCamera = $Camera
@onready var panel: DebugPanel = $DebugPanel
@onready var props: PropScatter = $Props


func _ready() -> void:
	var capture := Capture.new()
	capture.camera = camera
	capture.panel = panel
	capture.props = props
	add_child(capture)
	capture.place("grass")
	var args := OS.get_cmdline_user_args()
	if "--diagnose" in args or OS.has_feature("diagnose"):
		capture.run(true)
	elif "--capture" in args or OS.has_feature("capture"):
		capture.run(false)

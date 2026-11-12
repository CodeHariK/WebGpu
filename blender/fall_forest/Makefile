# Fall Forest: the fall asset pack (source/) on a chunked Godot terrain, ~20 000 props, and the
# ways to keep it fast (see README.md).
#
#   make            list the targets
#
# Override the app paths if yours differ: make run GODOT=/path/to/Godot

BLENDER ?= /Applications/Blender.app/Contents/MacOS/Blender
GODOT   ?= /Applications/Godot.app/Contents/MacOS/Godot
DEMO    := godot_demo

.PHONY: help blend assets import run edit capture

help:
	@echo "make blend     AllFallAssets.fbx + FallTex.png -> source/FallAssetsNature.blend (laid out, textured, metres)"
	@echo "make assets    source/FallAssetsNature.blend -> $(DEMO)/assets/fall_assets.glb (the meshes Godot uses)"
	@echo "make import    let Godot import the glb (makes the mesh LODs)"
	@echo "make run       play the forest (debug panel: switch each speed-up on / off)"
	@echo "make edit      open the demo in the Godot editor"
	@echo "make capture   time two views with each speed-up off in turn -> table + $(DEMO)/shots/"

blend:
	$(BLENDER) -b --factory-startup --python source/build_fall_assets.py -- $(CURDIR)/source

assets:
	$(BLENDER) -b source/FallAssetsNature.blend --python source/export_fall_assets.py -- $(CURDIR)/$(DEMO)/assets/fall_assets.glb

import:
	$(GODOT) --headless --editor --quit --path $(DEMO)

run: import
	$(GODOT) --path $(DEMO)

edit:
	$(GODOT) --editor --path $(DEMO)

capture: import
	$(GODOT) --path $(DEMO) -- --capture

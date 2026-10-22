# Toxic Factory — build the Blender kit and run the Godot demo.
#
#   make            list the targets
#   make run        build the kit if it changed, then play the demo
#   make shots      screenshot every camera view into shots/
#
# Override the app paths if yours differ: make run GODOT=/path/to/Godot

BLENDER ?= /Applications/Blender.app/Contents/MacOS/Blender
GODOT   ?= /Applications/Godot.app/Contents/MacOS/Godot

DEMO     := godot_demo
KIT_GLB  := $(DEMO)/toxic_kit.glb
KIT_SRC  := build_toxic_kit.py tf_kit.py tf_buildings.py tf_props.py
IMPORTED := $(DEMO)/.godot/imported/.stamp

.PHONY: help kit import run edit shots clean

help:
	@echo "make kit      Blender → $(KIT_GLB) + toxic_kit_sheet.png"
	@echo "make import   let Godot import the kit (done for you by run/edit/shots)"
	@echo "make run      play the demo (1-5 camera views, Space spin)"
	@echo "make edit     open the demo in the Godot editor"
	@echo "make shots    screenshot every view into shots/"
	@echo "make clean    remove shots and caches"

kit: $(KIT_GLB)

# Rebuilt only when a kit script changed.
$(KIT_GLB): $(KIT_SRC)
	$(BLENDER) -b --python build_toxic_kit.py 2>&1 | grep -iE "error|traceback|KIT PIECES|Saved:" || true
	@rm -rf __pycache__
	@test -f $@ || (echo "kit build failed" && false)

import: $(IMPORTED)

# Godot must import the glb before the demo can load it; redo it whenever the kit changes.
$(IMPORTED): $(KIT_GLB)
	$(GODOT) --headless --path $(DEMO) --import >/dev/null 2>&1 || true
	@mkdir -p $(dir $@) && touch $@

run: import
	$(GODOT) --path $(DEMO)

edit: import
	$(GODOT) --editor --path $(DEMO)

shots: import
	$(GODOT) --path $(DEMO) -- --shot
	@mkdir -p shots
	@mv $(DEMO)/shot_*.png shots/
	@rm -f $(DEMO)/shot_*.png.import
	@ls shots

clean:
	rm -rf shots __pycache__
	rm -f $(DEMO)/shot_*.png $(DEMO)/shot_*.png.import

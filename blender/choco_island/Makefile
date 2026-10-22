# Choco Island — build the Blender kit/scene and run the Godot demo.
#
#   make            list the targets
#   make run        build the kit if it changed, then play the Godot demo
#   make shots SEEDS="1 2 3"   screenshot those seeds (shots/ folder)
#
# Override the app paths if yours differ: make run GODOT=/path/to/Godot

BLENDER ?= /Applications/Blender.app/Contents/MacOS/Blender
GODOT   ?= /Applications/Godot.app/Contents/MacOS/Godot
SEEDS   ?= 1 2 3 4
SEED    ?= 1

DEMO    := godot_demo
KIT_GLB := $(DEMO)/choco_kit.glb
KIT_SRC := build_choco_kit.py choco_kit.py choco_props.py choco_terrain.py
SCENE_SRC := build_choco_island.py choco_kit.py choco_props.py choco_terrain.py choco_syrup.py
IMPORTED := $(DEMO)/.godot/imported/.stamp

.PHONY: help kit scene import run edit shots river all clean

help:
	@echo "make kit      Blender → $(KIT_GLB) + choco_kit_sheet.png (pieces Godot repeats)"
	@echo "make scene    Blender → choco_island.blend/.glb + preview pngs (the hand-made island)"
	@echo "make import   let Godot import the kit (done for you by run/edit/shots)"
	@echo "make run      play the demo (N next seed · R random · 1-4 views)"
	@echo "make edit     open the demo in the Godot editor"
	@echo "make shots    screenshot SEEDS=\"$(SEEDS)\" into shots/"
	@echo "make river    one shot per chocolate river shader step (SEED=$(SEED)) into shots/"
	@echo "make all      kit + scene + shots"
	@echo "make clean    remove shots, Blender backups and caches"

kit: $(KIT_GLB)

# Rebuilt only when a kit script changed.
$(KIT_GLB): $(KIT_SRC)
	$(BLENDER) -b --python build_choco_kit.py 2>&1 | grep -iE "error|traceback|KIT PIECES|Saved:" || true
	@rm -rf __pycache__
	@test -f $@ || (echo "kit build failed" && false)

scene: choco_island.glb

choco_island.glb: $(SCENE_SRC)
	$(BLENDER) -b --python build_choco_island.py 2>&1 | grep -iE "error|traceback|Saved:" || true
	@rm -rf __pycache__ choco_island.blend1
	@test -f $@ || (echo "scene build failed" && false)

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
	$(GODOT) --path $(DEMO) -- --shot $(SEEDS)
	@mkdir -p shots
	@mv $(DEMO)/shot_*.png shots/
	@rm -f $(DEMO)/shot_*.png.import
	@ls shots

# river_00.png is the finished shader, river_01–09 the steps in godot_demo/river_steps/.
river: import
	$(GODOT) --path $(DEMO) -- --river $(SEED)
	@mkdir -p shots
	@mv $(DEMO)/river_*.png shots/
	@rm -f $(DEMO)/river_*.png.import
	@ls shots

all: kit scene shots

clean:
	rm -rf shots __pycache__ choco_island.blend1
	rm -f $(DEMO)/shot_*.png $(DEMO)/shot_*.png.import $(DEMO)/river_*.png $(DEMO)/river_*.png.import

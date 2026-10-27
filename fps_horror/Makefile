# FPS Horror (mobile)
#
#   make            list targets
#   make run        play the current step demo
#   make shots      screenshot the current step into shots/
#   make assets     re-copy assets from the FPSHorrorGDScript clone (ORIGINAL=...)

GODOT    ?= /Applications/Godot.app/Contents/MacOS/Godot
ORIGINAL ?= /Users/Shared/Code/FPSHorrorGDScript/CurrentProject
DEMO     ?= res://demos/step06_enemy.tscn

.PHONY: help import run edit shots assets clean

help:
	@echo "make run      play $(DEMO)"
	@echo "make edit     open the Godot editor"
	@echo "make shots    screenshots of $(DEMO) into shots/"
	@echo "make assets   re-copy assets from $(ORIGINAL)"
	@echo "make clean    remove shots"

import:
	$(GODOT) --headless --path . --import >/dev/null 2>&1 || true

run: import
	$(GODOT) --path . $(DEMO)

edit:
	$(GODOT) --editor --path .

shots: import
	$(GODOT) --path . $(DEMO) -- --shot
	@ls shots

assets:
	python3 tools/copy_from_original.py $(ORIGINAL)

clean:
	rm -rf shots

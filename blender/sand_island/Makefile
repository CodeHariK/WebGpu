# Sand Island: the Water Shader Steps project (water_tutorial/: the island's water, sand, snow and
# rock shaders, step by step), the Liquid Shaders project (liquid/: the minecraft game's water, lava
# and poison on its MC water mesh), the small godot_demo/ project and the Blender preview script.
# `make` lists the targets.

GODOT    := /Applications/Godot.app/Contents/MacOS/Godot
TUTORIAL := water_tutorial
RUN      := $(GODOT) --path $(TUTORIAL)
LIQUID   := liquid
# the minecraft game's Godot project (liquid-sample exports from it)
GAME     := $(CURDIR)/../../minecraft/project

help:
	@echo "Water Shader Steps ($(TUTORIAL)/)"
	@echo "  make e           open it in the Godot editor"
	@echo "  make reimport    import new files headless (makes the .uid files Godot wants)"
	@echo "  make water       run the water steps (main.tscn, water_steps/)"
	@echo "  make sand        run the sand steps"
	@echo "  make snow        run the snow steps     make snow-demo   the snow island"
	@echo "  make rock        run the rock steps"
	@echo "  make lab         the shader_lib function lab (each function stage by stage)"
	@echo "  make bench       perf bench of the water / sand / snow steps (perf.md)"
	@echo "  make bench-rock  perf bench of the rock steps"
	@echo "  make shots       regenerate every screenshot in $(TUTORIAL)/shots/"
	@echo "Liquid Shaders ($(LIQUID)/)"
	@echo "  make liquids     the 6 liquid shaders (water / lava / poison, mobile + desktop) on the"
	@echo "                   game's MC water sample (keys 1 - 6, left / right; N day / night)"
	@echo "  make liquid-e    open it in the Godot editor     make liquid-reimport   import headless"
	@echo "  make liquid-shots   $(LIQUID)/shots/liquid_<shader>.png, by day and by night"
	@echo "  make liquid-sample  export the sample again from the game (its 'last' inspect map)"
	@echo "Other"
	@echo "  make demo        run godot_demo/ (the first island + shaders)"
	@echo "  make preview     render sand_island.glb in Godot -> godot_preview.png"

# --- Water Shader Steps ---
e:
	$(GODOT) -e --path $(TUTORIAL)

reimport:
	$(GODOT) --headless --editor --quit --path $(TUTORIAL)

water:
	$(RUN) res://main.tscn

sand:
	$(RUN) res://sand.tscn

snow:
	$(RUN) res://snow_steps.tscn

snow-demo:
	$(RUN) res://snow/snow.tscn

rock:
	$(RUN) res://rock/rock.tscn

lab:
	$(RUN) res://shader_lib/lab/function_lab.tscn

bench:
	$(RUN) res://main.tscn -- --bench

bench-rock:
	$(RUN) res://rock/rock.tscn -- --bench

# Every capture the scripts know (each run saves its PNGs and quits).
shots:
	$(RUN) res://main.tscn -- --capture
	$(RUN) res://main.tscn -- --capture-sand
	$(RUN) res://main.tscn -- --capture-snow-steps
	$(RUN) res://main.tscn -- --capture-layers
	$(RUN) res://main.tscn -- --capture-loft
	$(RUN) res://main.tscn -- --capture-bank
	$(RUN) res://main.tscn -- --capture-procedural
	$(RUN) res://main.tscn -- --hud-shot
	$(RUN) res://snow/snow.tscn -- --capture-snow
	$(RUN) res://rock/rock.tscn -- --capture-steps $(CURDIR)/$(TUTORIAL)/shots
	$(RUN) res://shader_lib/lab/function_lab.tscn -- --capture-lab

# --- Liquid Shaders ---
liquids:
	$(GODOT) --path $(LIQUID)

liquid-e:
	$(GODOT) -e --path $(LIQUID)

liquid-reimport:
	$(GODOT) --headless --editor --quit --path $(LIQUID)

liquid-shots:
	$(GODOT) --path $(LIQUID) -- --capture-liquids

# The game's MC water + terrain -> liquid/sample/water.res, terrain.res. The exporter needs the
# game's GDExtension, so it runs in the game project (copied in for the run, then removed).
liquid-sample:
	cp $(LIQUID)/tools/export_liquid_sample.gd $(GAME)/_liquid_sample_export.gd
	$(GODOT) --headless --path $(GAME) -s res://_liquid_sample_export.gd -- $(CURDIR)/$(LIQUID)/sample; \
		status=$$?; rm -f $(GAME)/_liquid_sample_export.gd $(GAME)/_liquid_sample_export.gd.uid; exit $$status

# --- Other ---
demo:
	$(GODOT) --path godot_demo

preview:
	python3 preview_in_godot.py

.PHONY: help e reimport water sand snow snow-demo rock lab bench bench-rock shots liquids liquid-e \
	liquid-reimport liquid-shots liquid-sample demo preview

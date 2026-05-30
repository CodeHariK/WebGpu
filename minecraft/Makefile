# Variables
TARGET       := template_debug
PLATFORM     := macos
OUTPUT_DIR   := project/bin/$(PLATFORM)
LIB_NAME     := libMinecraftExt.$(PLATFORM).$(TARGET).dylib

SCONS        := /opt/homebrew/bin/scons
GODOT        := /Applications/Godot.app/Contents/MacOS/Godot
NM           := nm

r:
	$(GODOT) -e --path project

reimport:
	$(GODOT) --headless --editor --quit --path project

mcr: reimport run_mc

# Explicit Debug Build
debug:
	time $(SCONS) target=template_debug platform=$(PLATFORM)

# Explicit Release Build
release:
	time $(SCONS) target=template_release platform=$(PLATFORM)

clang:
	$(SCONS) target=$(TARGET) platform=$(PLATFORM) compiledb=yes compile_commands.json

bindings:
	$(SCONS) target=$(TARGET) platform=$(PLATFORM) generate_bindings=yes

debug_entitlements:
	printf '<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">\n<plist version="1.0">\n<dict>\n\t<key>com.apple.security.get-task-allow</key>\n\t<true/>\n</dict>\n</plist>' > debug.entitlements
	codesign -f -s - --entitlements debug.entitlements /Applications/Godot.app/Contents/MacOS/Godot


# Inspect exported symbols
i:
# 	$(NM) $(OUTPUT_DIR)/$(LIB_NAME) > .nm_debug_symbols.txt | code .nm_debug_symbols.txt 
	ls -lh $(OUTPUT_DIR)/$(LIB_NAME)

# Clean build artifacts
clean:
	$(SCONS) -c
	rm -f compile_commands.json

.PHONY: all build inspect clean reimport brr


run_mc:
	$(GODOT) --path project/ --scene scene/MarchingCubes/MARCHING_CUBES.tscn
run_chunked:
	$(GODOT) --path project/ --scene scene/terraspline/chunked_terrain_demo.tscn
run_pg:
	$(GODOT) --path project/ --scene scene/terraspline/procgen.tscn
run_octo:
	$(GODOT) --path project/ --scene scene/octocar/octocar.tscn
run_road:
	$(GODOT) --path project/ --scene scene/road/road.tscn
run_gizmo:
	$(GODOT) --path project/ --scene scene/road/gizmo.tscn
run_oc:
	$(GODOT) --path project/ --scene scene/mini_games/Overcooked.tscn
run_tennis:
	$(GODOT) --path project/ --scene scene/mini_games/Tennis.tscn
run_sn:
	$(GODOT) --path project/ --scene scene/minecraft/scene_minecraft.tscn

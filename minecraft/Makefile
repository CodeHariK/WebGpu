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

brr: b reimport run_mc


# Explicit Debug Build
debug:
	time $(SCONS) target=template_debug platform=$(PLATFORM)

# Explicit Release Build
release:
	time $(SCONS) target=template_release platform=$(PLATFORM)

world_blender:
	cp ../world_blender/example/Voxel.glb project/assets/
	cp ../world_blender/example/library.json project/assets/

clang:
	$(SCONS) target=$(TARGET) platform=$(PLATFORM) compiledb=yes compile_commands.json

bindings:
	$(SCONS) target=$(TARGET) platform=$(PLATFORM) generate_bindings=yes

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

run_octo:
	$(GODOT) --path project/ --scene scene/octocar/octocar.tscn

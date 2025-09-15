# Variables
TARGET       := template_debug
PLATFORM     := macos
OUTPUT_DIR   := project/bin/$(PLATFORM)
LIB_NAME     := libMinecraftExt.$(PLATFORM).$(TARGET).dylib

SCONS        := scons
NM           := nm

e:
	godot -e --path project
r:
	godot --path project

# Build the extension
b:
	time $(SCONS) target=$(TARGET) platform=$(PLATFORM)

br:
	time $(SCONS) target=$(TARGET) platform=$(PLATFORM)
	godot --path project

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

.PHONY: all build inspect clean

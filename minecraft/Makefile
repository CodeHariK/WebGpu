# Variables
TARGET       := template_debug
PLATFORM     := macos
OUTPUT_DIR   := project/bin/$(PLATFORM)
LIB_NAME     := libMinecraftExt.$(PLATFORM).$(TARGET).dylib

SCONS        := scons
NM           := nm

# Default target
all: build inspect

editor:
	godot -e --path project

# Build the extension
build:
	$(SCONS) target=$(TARGET) platform=$(PLATFORM)

clang:
	$(SCONS) target=$(TARGET) platform=$(PLATFORM) compiledb=yes compile_commands.json

bindings:
	$(SCONS) target=$(TARGET) platform=$(PLATFORM) generate_bindings=yes

# Inspect exported symbols
inspect:
# 	$(NM) $(OUTPUT_DIR)/$(LIB_NAME) > .nm_debug_symbols.txt | code .nm_debug_symbols.txt 
	ls -lh $(OUTPUT_DIR)/$(LIB_NAME)

# Clean build artifacts
clean:
	$(SCONS) -c
	rm -f compile_commands.json

.PHONY: all build inspect clean

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
TARGET = quadtree_demo
SOURCES = main.cpp
HEADERS = quadtree.h quadtree.hpp

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

# Run the program
run: $(TARGET)
	./$(TARGET)

# Clean build files
clean:
	rm -f $(TARGET)

# Debug build
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# Release build with optimizations
release: CXXFLAGS += -O3 -DNDEBUG
release: $(TARGET)

# Check for memory leaks with valgrind (if available)
valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)

# Install dependencies (for Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y build-essential

# Install dependencies (for macOS with Homebrew)
install-deps-mac:
	brew install gcc

# Show help
help:
	@echo "Available targets:"
	@echo "  all          - Build the quadtree demo (default)"
	@echo "  run          - Build and run the program"
	@echo "  clean        - Remove build files"
	@echo "  debug        - Build with debug symbols"
	@echo "  release      - Build with maximum optimizations"
	@echo "  valgrind     - Run with memory leak detection"
	@echo "  install-deps - Install build dependencies (Ubuntu/Debian)"
	@echo "  install-deps-mac - Install build dependencies (macOS)"
	@echo "  help         - Show this help message"

.PHONY: all run clean debug release valgrind install-deps install-deps-mac help

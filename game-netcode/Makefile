# ============================================================================
#  Game Netcode from Scratch — build file
#
#  Layout:
#    src/net/    reusable networking library (sockets, connection, channels,
#                reliability, bitstream, network simulator)
#    src/game/   game-specific logic (types, simulation, prediction, delta,
#                lag compensation, interest management)  -- all header-only
#    src/apps/   the demo programs: server.cpp and client.cpp
#    tests/      one test program per concept (see docs/)
#
#  Common commands:
#    make            build everything (apps + tests)
#    make test       build and run every test
#    make run        run server + client locally (clean network)
#    make run-sim    run server + client under a simulated bad network
#    make clean      remove build/ and bin/
# ============================================================================

CXX ?= clang++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -O3 -Isrc

SRC_DIR   = src
TEST_DIR  = tests
BUILD_DIR = build
BIN_DIR   = bin

# --- Reusable networking library (the .cpp files with real definitions) ------
LIB_SRCS = $(SRC_DIR)/net/Address.cpp \
           $(SRC_DIR)/net/Socket.cpp \
           $(SRC_DIR)/net/NetworkSimulator.cpp \
           $(SRC_DIR)/net/ReliabilitySystem.cpp \
           $(SRC_DIR)/net/Connection.cpp
LIB_OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(LIB_SRCS))

# --- Demo apps ---------------------------------------------------------------
APPS = $(BIN_DIR)/server $(BIN_DIR)/client

# --- Tests: header-only (need no library) vs. library-linked -----------------
HEADER_TESTS = $(BIN_DIR)/prediction_test $(BIN_DIR)/bitstream_test \
               $(BIN_DIR)/delta_test $(BIN_DIR)/lagcomp_test $(BIN_DIR)/aoi_test
LIB_TESTS    = $(BIN_DIR)/sim_test $(BIN_DIR)/protocol_test \
               $(BIN_DIR)/channel_test $(BIN_DIR)/netsim_test
TESTS = $(HEADER_TESTS) $(LIB_TESTS)

.PHONY: all clean test rebuild run run-sim run-server run-client format

all: $(APPS) $(TESTS)

# --- Compile rules (source tree and test tree) -------------------------------
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# --- Demo apps (link the network library) ------------------------------------
$(BIN_DIR)/server: $(BUILD_DIR)/apps/server.o $(LIB_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN_DIR)/client: $(BUILD_DIR)/apps/client.o $(LIB_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# --- Header-only tests (game logic is all in headers, nothing to link) -------
$(HEADER_TESTS): $(BIN_DIR)/%: $(BUILD_DIR)/tests/%.o
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# --- Library-linked tests (touch sockets / connection / reliability) ---------
$(LIB_TESTS): $(BIN_DIR)/%: $(BUILD_DIR)/tests/%.o $(LIB_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# --- Run all tests -----------------------------------------------------------
test: $(TESTS)
	@for t in $(TESTS); do echo "== $$t =="; $$t || exit 1; done

# --- Force a clean rebuild of just the client and server binaries ------------
rebuild:
	rm -f $(BUILD_DIR)/apps/server.o $(BUILD_DIR)/apps/client.o
	$(MAKE) $(BIN_DIR)/server $(BIN_DIR)/client

# --- Run helpers -------------------------------------------------------------
PORT ?= 40000

run-server: $(BIN_DIR)/server
	./$(BIN_DIR)/server $(PORT)

run-client: $(BIN_DIR)/client
	./$(BIN_DIR)/client

# Server in the background, client in the foreground; server stops on exit.
run: $(BIN_DIR)/server $(BIN_DIR)/client
	@echo "Starting server on port $(PORT)..."; \
	./$(BIN_DIR)/server $(PORT) & SERVER_PID=$$!; \
	trap 'kill $$SERVER_PID 2>/dev/null' EXIT INT TERM; \
	sleep 1; \
	echo "Starting client..."; \
	./$(BIN_DIR)/client $(PORT)

# Same, but both sides run under a simulated adverse network.
NETSIM_LATENCY_MS ?= 80
NETSIM_JITTER_MS  ?= 20
NETSIM_LOSS       ?= 0.05
NETSIM_DUP        ?= 0.02
run-sim: $(BIN_DIR)/server $(BIN_DIR)/client
	@echo "Simulated network: $(NETSIM_LATENCY_MS)ms +/-$(NETSIM_JITTER_MS)ms, loss $(NETSIM_LOSS), dup $(NETSIM_DUP)"; \
	NETSIM_LATENCY_MS=$(NETSIM_LATENCY_MS) NETSIM_JITTER_MS=$(NETSIM_JITTER_MS) \
	NETSIM_LOSS=$(NETSIM_LOSS) NETSIM_DUP=$(NETSIM_DUP) ./$(BIN_DIR)/server $(PORT) & SERVER_PID=$$!; \
	trap 'kill $$SERVER_PID 2>/dev/null' EXIT INT TERM; \
	sleep 1; \
	NETSIM_LATENCY_MS=$(NETSIM_LATENCY_MS) NETSIM_JITTER_MS=$(NETSIM_JITTER_MS) \
	NETSIM_LOSS=$(NETSIM_LOSS) NETSIM_DUP=$(NETSIM_DUP) ./$(BIN_DIR)/client $(PORT)

# --- Formatting (applies .clang-format to all sources) -----------------------
# Prefers clang-format on PATH, else the one bundled with Xcode.
CLANG_FORMAT ?= $(shell command -v clang-format 2>/dev/null || xcrun -f clang-format 2>/dev/null || echo clang-format)
format:
	@$(CLANG_FORMAT) -i $$(find $(SRC_DIR) $(TEST_DIR) \( -name '*.hpp' -o -name '*.cpp' \))
	@echo "Formatted $(SRC_DIR)/ and $(TEST_DIR)/ with $(CLANG_FORMAT)"

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

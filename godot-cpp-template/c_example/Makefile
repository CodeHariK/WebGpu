CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
TARGET = main
SRC = main.cpp

# If json.hpp is in the same folder as main.cpp, no include path is needed.
# If it's in a folder (e.g. libs/), add: CXXFLAGS += -Ilibs

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

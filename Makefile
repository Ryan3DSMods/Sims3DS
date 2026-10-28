# sims — host build (portable core + verification tool)
# The same src/core and src/formats sources cross-compile for the 3DS later
# via distribution/3ds; nothing here pulls in host-only APIs except src/core/png.h
# and the tool, which the 3DS build omits.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Isrc
LDLIBS   ?= -lz

CORE_SRC := src/formats/far1.cpp src/formats/iff.cpp src/sim/vm.cpp src/sim/autonomy.cpp
TOOL_SRC := src/tools/simtool.cpp

BUILD := build
OBJ := $(patsubst %.cpp,$(BUILD)/%.o,$(CORE_SRC) $(TOOL_SRC))

all: $(BUILD)/simtool

$(BUILD)/simtool: $(OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean

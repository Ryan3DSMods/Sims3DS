# Host-only build of the sims.files translation + selftest.
#   make -C src/files files-selftest
#   ./src/files/files-selftest /path/to/House02.iff
#   ./src/files/files-selftest /path/to/Objects.far AlarmClock.iff
# Cross-compile check (object files only):  make -C src/files 3ds-check
SRC_ROOT := $(abspath ..)
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -O2 -g -I$(SRC_ROOT)
ARM_CXX  ?= arm-none-eabi-g++
ARM_FLAGS := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mfpu=vfp -mtp=soft -std=gnu++17 -fexceptions -D__3DS__ -I$(SRC_ROOT)

SOURCES := $(wildcard *.cpp FAR1/*.cpp FAR3/*.cpp formats/iff/*.cpp formats/iff/chunks/*.cpp formats/otf/*.cpp)
LIB_SOURCES := $(filter-out selftest.cpp,$(SOURCES))
OBJDIR := build/host
ARMDIR := build/3ds
OBJS := $(patsubst %.cpp,$(OBJDIR)/%.o,$(SOURCES))
ARM_OBJS := $(patsubst %.cpp,$(ARMDIR)/%.o,$(LIB_SOURCES))

.PHONY: files-selftest all clean 3ds-check
all: files-selftest

files-selftest: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ARMDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(ARM_CXX) $(ARM_FLAGS) -c $< -o $@

3ds-check: $(ARM_OBJS)
	@echo "3DS cross-compile OK ($(words $(ARM_OBJS)) objects)"

clean:
	rm -rf build files-selftest

# SimAntics core (src/simantics) + content manager (src/content).
# Run from src/simantics:   make simantics-check        (host + 3DS, against _files_contract.h)
#                           make simantics-check-real   (host + 3DS, against the real src/files headers)
#                           make host | make 3ds        (one target only)
# Primitives (src/simantics/primitives/*.cpp) are included automatically once they exist.

SRC_ROOT   := ..
CORE_SRCS  := $(shell find . -name '*.cpp' -not -path './primitives/*' | sort) ../content/ContentManager.cpp
PRIM_SRCS  := $(shell find ./primitives -name '*.cpp' 2>/dev/null | sort)
SRCS       := $(CORE_SRCS) $(PRIM_SRCS)

HOST_CXX   ?= g++
HOST_FLAGS := -std=c++17 -Wall -I$(SRC_ROOT)

DEVKITARM  ?= /opt/devkitpro/devkitARM
ARM_CXX    ?= $(shell command -v arm-none-eabi-g++ 2>/dev/null || echo $(DEVKITARM)/bin/arm-none-eabi-g++)
ARM_FLAGS  := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mfpu=vfp -mtp=soft -std=gnu++17 -fexceptions -D__3DS__ -Wall -I$(SRC_ROOT)

BUILD      := build

.PHONY: simantics-check simantics-check-real host 3ds host-real 3ds-real clean

simantics-check: host 3ds
	@echo "simantics-check: host + 3DS OK ($(words $(SRCS)) files)"

simantics-check-real: host-real 3ds-real
	@echo "simantics-check-real: host + 3DS OK against src/files ($(words $(SRCS)) files)"

define compile_all
	@mkdir -p $(BUILD)/$(1)
	@fail=0; for f in $(SRCS); do \
	  o=$(BUILD)/$(1)/$$(echo $$f | sed 's|^\./||; s|^\.\./||; s|/|_|g; s|\.cpp$$|.o|'); \
	  $(2) $(3) -c $$f -o $$o || fail=1; \
	done; test $$fail = 0
endef

host:
	$(call compile_all,host,$(HOST_CXX),$(HOST_FLAGS))

3ds:
	$(call compile_all,3ds,$(ARM_CXX),$(ARM_FLAGS))

host-real:
	$(call compile_all,host-real,$(HOST_CXX),$(HOST_FLAGS) -DST_FILES_REAL)

3ds-real:
	$(call compile_all,3ds-real,$(ARM_CXX),$(ARM_FLAGS) -DST_FILES_REAL)

clean:
	rm -rf $(BUILD)

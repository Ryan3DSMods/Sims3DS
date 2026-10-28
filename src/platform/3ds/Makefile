#---------------------------------------------------------------------------------
# sims3ds — 3DS build (devkitPro devkitARM + libctru).
#   sudo dkp-pacman -S 3ds-dev          # devkitARM + libctru + 3dstools
#   source /etc/profile.d/devkit-env.sh
#   cd src/platform/3ds && make         # -> sims.3dsx (+ .smdh)
#   make cia                            # optional: sims.cia (needs makerom + bannertool in PATH),
#                                       # runs in the New 3DS 178 MB extended memory mode (sims.rsf)
# Copy sims.3dsx to sdmc:/3ds/sims/ next to the game data (see src/app/README.md).
#
# Builds the whole engine: src/files src/simantics src/content src/world src/vitaboy src/common, the shared
# app (src/app) and this platform layer. Every .cpp below those folders is compiled (no duplicate basenames;
# the template's VPATH flattens folders), except the host-only self test src/files/selftest.cpp.
#---------------------------------------------------------------------------------
.SUFFIXES:
ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM not set. Install devkitPro and `source /etc/profile.d/devkit-env.sh`")
endif
TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET      := sims
BUILD       := build
# repository src/ (this Makefile lives in src/platform/3ds)
REPO_SRC    := $(TOPDIR)/../..
ENGINE_DIRS := files simantics content world vitaboy common app hit audio
SOURCES     := $(sort $(dir $(foreach d,$(ENGINE_DIRS),$(shell find $(REPO_SRC)/$(d) -name '*.cpp' 2>/dev/null)))) $(TOPDIR)
INCLUDES    := $(REPO_SRC)
EXCLUDE_CPP := selftest.cpp

# Unofficial fan project: no EA/Maxis names or marks in the title (see README.md, "Legal").
APP_TITLE       := sims3ds
APP_DESCRIPTION := Sims 1 3DS (unofficial) - needs your own game data
APP_AUTHOR      := sims3ds contributors
ICON            ?= $(TOPDIR)/icon.png
BANNER_PNG      ?= $(TOPDIR)/banner.png

ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
# ST_FILES_REAL: the real IFF/FAR readers; ST_HAVE_DIRENT: devkitARM newlib has <dirent.h> (custom walls/floors,
# Downloads, skins scan)
CFLAGS  := -g -Wall -O2 -mword-relocations -ffunction-sections -fno-strict-aliasing $(ARCH) $(BUILD_CFLAGS) \
           -D__3DS__ -DST_FILES_REAL -DST_HAVE_DIRENT $(foreach dir,$(INCLUDES),-I$(dir)) \
           -I$(CTRULIB)/include -I$(DEVKITPRO)/libctru/include -I$(CURDIR)/$(BUILD)
# exceptions + RTTI: the translated engine uses try/catch and dynamic_cast
CXXFLAGS := $(CFLAGS) -fexceptions -frtti -std=gnu++17 -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare -Wno-reorder
ASFLAGS := -g $(ARCH)
LDFLAGS  = -specs=3dsx.specs -g $(ARCH) -Wl,--gc-sections -Wl,-Map,$(notdir $*.map)
LIBS     := -lctru -lm
CTRULIB  ?= $(DEVKITPRO)/libctru
LIBDIRS  := $(CTRULIB)

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)
export VPATH    := $(SOURCES)
export DEPSDIR  := $(CURDIR)/$(BUILD)
CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(filter-out $(EXCLUDE_CPP),$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp))))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
export LD := $(CXX)
export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
ifneq (,$(wildcard $(ICON)))
export APP_ICON := $(ICON)
else
export APP_ICON := $(CTRULIB)/default_icon.png
endif
export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
.PHONY: $(BUILD) clean all cia
all: $(BUILD)
$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).smdh $(TARGET).cia
# CIA (extended memory). Needs makerom + bannertool (not part of devkitPro).
# HOME Menu banner: by default the 3D "extended banner" (hardware-tested on a New 3DS): banner3d/make_banner3d.sh:
# banner3d/banner_scene.gltf -> pycgfx -> CGFX -> `bannertool makebanner -ci`, plus an SMDH with the required
# `extendedbanner` flag (first run needs git, Python >= 3.12 and network). If that step fails, or with
# `make cia BANNER3D=0`, the flat banner is used:
#   bannertool makebanner -i banner.png -a <audio> -o build/banner.bnr   (+ the normal sims.smdh)
# The logo scene/art is CC BY-SA 4.0 (assets/logo/ATTRIBUTION.md); regenerate it with tools/banner3d/make_scene.py.
# Neutral art instead: make cia BANNER3D=0 ICON=$PWD/neutral/icon.png BANNER_PNG=$PWD/neutral/banner.png
# Banner audio: banner3d/banner_sound.wav if you exported one from YOUR OWN game data
# (tools/banner3d/export_game_sound.sh, <= 3 s 44.1 kHz 16-bit stereo), else banner3d/banner_silence.wav.
BANNER3D ?= 1
cia: $(BUILD)
	@if [ "$(BANNER3D)" != "0" ] && ICON_PNG=$(ICON) bash $(TOPDIR)/banner3d/make_banner3d.sh $(BUILD); then \
		echo "cia: using the 3D banner"; SMDH=$(BUILD)/sims_cia.smdh; \
	else \
		[ "$(BANNER3D)" != "0" ] && echo "cia: 3D banner step failed - falling back to the flat banner.png"; \
		AUDIO=$(TOPDIR)/banner3d/banner_sound.wav; [ -f "$$AUDIO" ] || AUDIO=$(TOPDIR)/banner3d/banner_silence.wav; \
		bannertool makebanner -i $(BANNER_PNG) -a $$AUDIO -o $(BUILD)/banner.bnr || exit 1; \
		SMDH=$(TARGET).smdh; \
	fi; \
	makerom -f cia -o $(TARGET).cia -elf $(TARGET).elf -rsf $(TOPDIR)/sims.rsf -icon $$SMDH -banner $(BUILD)/banner.bnr -exefslogo -target t
else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).3dsx : $(OUTPUT).elf $(OUTPUT).smdh
$(OUTPUT).elf  : $(OFILES)
-include $(DEPENDS)
endif

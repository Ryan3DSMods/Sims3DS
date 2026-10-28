#---------------------------------------------------------------------------------
# sims3ds — 3DS build (devkitPro devkitARM + libctru).
#   sudo dkp-pacman -S 3ds-dev          # devkitARM + libctru + 3dstools
#   source /etc/profile.d/devkit-env.sh
#   cd src/platform/3ds && make         # -> sims.3dsx (+ .smdh)
#   make cia                            # optional: sims.cia (needs makerom + bannertool in PATH),
#                                       # New 3DS memory mode from CIA_MEM (124 or 96 MB, see below)
#   make release VERSION=1.0            # sims.3dsx + sims.cia -> ../../../dist/Sims3DS-1.0.zip (docs/RELEASING.md)
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
# Banner audio override (a .wav path). `make release` forces the silent one so no game audio ends up in a published CIA.
#
# CIA_MEM: the New 3DS application memory mode written into the CIA (sims.rsf is patched into build/sims_cia.rsf):
#   124 -> SystemMode 64MB, SystemModeExt 124MB   (default; hardware-tested)
#   96  -> SystemMode 96MB, SystemModeExt Legacy  (more memory left to the HOME Menu)
# Pressing HOME in-game with the 3D banner crashed the HOME Menu in BOTH modes (docs/TESTING.md), so 124 stays default.
# (178 MB was tried earlier and crashed the HOME Menu.) The .3dsx is not affected: it gets what the launcher gives it.
BANNER3D    ?= 1
BANNER_AUDIO ?=
CIA_MEM     ?= 124
ifeq ($(CIA_MEM),124)
CIA_SYSMODE    := 64MB
CIA_SYSMODEEXT := 124MB
else ifeq ($(CIA_MEM),96)
CIA_SYSMODE    := 96MB
CIA_SYSMODEEXT := Legacy
else
$(error CIA_MEM must be 124 or 96 (got '$(CIA_MEM)'))
endif
# release: version for the zip name (default: the git tag/commit, e.g. v1.0 -> 1.0)
VERSION ?= $(shell git -C $(TOPDIR) describe --tags --always --dirty 2>/dev/null | sed 's/^v//')
.PHONY: release
cia: $(BUILD)
	@sed -e 's/^\(  SystemMode  *: \).*/\1$(CIA_SYSMODE)/' -e 's/^\(  SystemModeExt  *: \).*/\1$(CIA_SYSMODEEXT)/' \
		$(TOPDIR)/sims.rsf > $(BUILD)/sims_cia.rsf
	@grep -q '^  SystemMode  *: $(CIA_SYSMODE)$$' $(BUILD)/sims_cia.rsf && grep -q '^  SystemModeExt  *: $(CIA_SYSMODEEXT)$$' $(BUILD)/sims_cia.rsf \
		|| { echo "cia: could not set the memory mode in build/sims_cia.rsf"; exit 1; }
	@echo "cia: memory mode CIA_MEM=$(CIA_MEM) (SystemMode $(CIA_SYSMODE), SystemModeExt $(CIA_SYSMODEEXT))"
	@if [ "$(BANNER3D)" != "0" ] && BANNER_AUDIO="$(BANNER_AUDIO)" ICON_PNG=$(ICON) bash $(TOPDIR)/banner3d/make_banner3d.sh $(BUILD); then \
		echo "cia: using the 3D banner"; SMDH=$(BUILD)/sims_cia.smdh; \
	else \
		[ "$(BANNER3D)" != "0" ] && echo "cia: 3D banner step failed - falling back to the flat banner.png"; \
		AUDIO="$(BANNER_AUDIO)"; [ -n "$$AUDIO" ] || AUDIO=$(TOPDIR)/banner3d/banner_sound.wav; \
		[ -f "$$AUDIO" ] || AUDIO=$(TOPDIR)/banner3d/banner_silence.wav; \
		bannertool makebanner -i $(BANNER_PNG) -a $$AUDIO -o $(BUILD)/banner.bnr || exit 1; \
		SMDH=$(TARGET).smdh; \
	fi; \
	makerom -f cia -o $(TARGET).cia -elf $(TARGET).elf -rsf $(BUILD)/sims_cia.rsf -icon $$SMDH -banner $(BUILD)/banner.bnr -exefslogo -target t
# Prebuilt release zip for players (docs/RELEASING.md): builds the .3dsx and the .cia (3D banner, flat fallback,
# always the silent banner sound) and packs dist/Sims3DS-<VERSION>.zip with tools/release/make_release.sh.
release:
	@[ -n "$(VERSION)" ] || { echo "release: set VERSION=1.0 (or tag the commit)"; exit 1; }
	@$(MAKE) --no-print-directory cia BANNER_AUDIO=$(TOPDIR)/banner3d/banner_silence.wav CIA_MEM=$(CIA_MEM) BANNER3D=$(BANNER3D)
	@bash $(TOPDIR)/../../../tools/release/make_release.sh --package-only --version "$(VERSION)" \
		--3dsx $(TOPDIR)/$(TARGET).3dsx --cia $(TOPDIR)/$(TARGET).cia --cia-mem $(CIA_MEM)
else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).3dsx : $(OUTPUT).elf $(OUTPUT).smdh
$(OUTPUT).elf  : $(OFILES)
-include $(DEPENDS)
endif

#---------------------------------------------------------------------------------
# Basic project information
#---------------------------------------------------------------------------------
TARGET      := bvg-navigator
BUILD       := build
SOURCES     := source
INCLUDES    := include
GRAPHICS    := gfx

APP_TITLE   := BVG Navigator
APP_AUTHOR  := Homebrew
APP_DESCRIPTION := Berlin public transport navigation

#---------------------------------------------------------------------------------
# devkitPro configuration
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
endif

export DEVKITPRO

include $(DEVKITPRO)/devkitARM/3ds_rules

LIBDIRS := $(CTRULIB)

#---------------------------------------------------------------------------------
# Source files
#---------------------------------------------------------------------------------
CFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
OFILES := $(CFILES:%.c=$(BUILD)/%.o)

#---------------------------------------------------------------------------------
# Compiler flags
#---------------------------------------------------------------------------------
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
           $(foreach dir,$(LIBDIRS),-I$(dir)/include)

CFLAGS := -g -Wall -O2 -mword-relocations \
          -ffunction-sections -fdata-sections \
          $(INCLUDE) -D__3DS__ $(ARCH)

LDFLAGS := -specs=3dsx.specs $(ARCH) -Wl,-Map,$(notdir $@).map

LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

LIBS := -lctru -lm

#---------------------------------------------------------------------------------
# Build
#---------------------------------------------------------------------------------
.PHONY: all clean

all: $(TARGET).3dsx

$(TARGET).elf: $(OFILES)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LIBPATHS) $(LIBS) -o $@

$(TARGET).smdh: $(APP_ICON)
	smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $< $@

$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh
	$(3DSXTOOL) $< $@ --smdh=$(TARGET).smdh

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf.map


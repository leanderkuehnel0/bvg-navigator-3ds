#---------------------------------------------------------------------------------
# Basic project information
#---------------------------------------------------------------------------------
TARGET      := bvg-navigator
BUILD       := build
SOURCES     := source
INCLUDES    := include
GRAPHICS    := gfx

APP_TITLE   := BVG Navigator
APP_AUTHOR := Homebrew
APP_DESCRIPTION := Berlin public transport navigation

#---------------------------------------------------------------------------------
# devkitPro configuration
#---------------------------------------------------------------------------------
include $(DEVKITPRO)/devkitARM/3ds_rules

#---------------------------------------------------------------------------------
# Source files
#---------------------------------------------------------------------------------
CFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
OFILES := $(CFILES:%.c=$(BUILD)/%.o)

#---------------------------------------------------------------------------------
# Compiler flags
#---------------------------------------------------------------------------------
CFLAGS  := -g -Wall -O2 -mword-relocations \
           -ffunction-sections -fdata-sections \
           $(INCLUDE)

CFLAGS += $(ARCH)

LDFLAGS := $(ARCH) -Wl,-Map,$(notdir $@).map

LIBS := -lctru

#---------------------------------------------------------------------------------
# Build
#---------------------------------------------------------------------------------
.PHONY: all clean

all: $(TARGET).3dsx

$(TARGET).elf: $(OFILES)
	@mkdir -p $(dir $@)
	$(LINK.o) $(LDFLAGS) -o $@ $^ $(LIBS)

$(TARGET).3dsx: $(TARGET).elf
	$(3DSXTOOL) $< $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).3dsx $(TARGET).elf.map


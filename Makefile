# Makefile for Tangents - Steiner-Parker Filter for distingNT
#
# Usage:
#   make hardware    - Build for distingNT hardware (.o file)
#   make test        - Build for nt_emu testing (.dylib/.so/.dll)
#   make both        - Build both targets
#   make clean       - Remove all build artifacts

# ============================================================================
# PROJECT CONFIGURATION
# ============================================================================

PLUGIN_NAME = tangents
SOURCES = tangents.cpp

# Detect platform
UNAME_S := $(shell uname -s)

# Target selection (hardware or test)
TARGET ?= hardware

# ============================================================================
# HARDWARE BUILD (ARM Cortex-M7 for distingNT)
# ============================================================================
ifeq ($(TARGET),hardware)
    CXX = arm-none-eabi-g++
    CFLAGS = -std=c++11 \
             -mcpu=cortex-m7 \
             -mfpu=fpv5-d16 \
             -mfloat-abi=hard \
             -mthumb \
             -Os \
             -Wall \
             -fPIC \
             -fno-rtti \
             -fno-exceptions
    INCLUDES = -I. -I./distingNT_API/include
    LDFLAGS = -Wl,--relocatable -nostdlib
    OUTPUT_DIR = plugins
    BUILD_DIR = build
    OUTPUT = $(OUTPUT_DIR)/$(PLUGIN_NAME).o
    OBJECTS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SOURCES))
    CHECK_CMD = arm-none-eabi-nm $(OUTPUT) | grep ' U '
    SIZE_CMD = arm-none-eabi-size $(OUTPUT)

# ============================================================================
# DESKTOP BUILD (Native for nt_emu VCV Rack testing)
# ============================================================================
else ifeq ($(TARGET),test)
    # macOS
    ifeq ($(UNAME_S),Darwin)
        CXX = clang++
        CFLAGS = -std=c++11 -fPIC -Os -Wall -fno-rtti -fno-exceptions
        LDFLAGS = -dynamiclib -undefined dynamic_lookup
        EXT = dylib
    endif

    # Linux
    ifeq ($(UNAME_S),Linux)
        CXX = g++
        CFLAGS = -std=c++11 -fPIC -Os -Wall -fno-rtti -fno-exceptions
        LDFLAGS = -shared
        EXT = so
    endif

    # Windows (MinGW or MSVC)
    ifeq ($(OS),Windows_NT)
        CXX = cl
        CFLAGS = /std:c++11 /O2 /W3 /GR- /EHsc-
        LDFLAGS = /LD
        EXT = dll
    endif

    INCLUDES = -I. -I./distingNT_API/include
    OUTPUT_DIR = plugins
    OUTPUT = $(OUTPUT_DIR)/$(PLUGIN_NAME).$(EXT)
    CHECK_CMD = nm $(OUTPUT) | grep ' U ' || echo "No undefined symbols"
    SIZE_CMD = ls -lh $(OUTPUT)
endif

# ============================================================================
# BUILD RULES
# ============================================================================

all: $(OUTPUT)

# Hardware build with object files
ifeq ($(TARGET),hardware)
$(OUTPUT): $(OBJECTS)
	@mkdir -p $(OUTPUT_DIR)
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $^
	@echo "Built hardware plugin: $@"

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Test build (direct linking)
else ifeq ($(TARGET),test)
$(OUTPUT): $(SOURCES)
	@mkdir -p $(OUTPUT_DIR)
	$(CXX) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $(SOURCES)
	@echo "Built test plugin: $@"
endif

# ============================================================================
# CONVENIENCE TARGETS
# ============================================================================

hardware:
	@$(MAKE) TARGET=hardware

test:
	@$(MAKE) TARGET=test

both: hardware test

check: $(OUTPUT)
	@echo "Checking symbols in $(OUTPUT)..."
	@$(CHECK_CMD) || true

size: $(OUTPUT)
	@echo "Size of $(OUTPUT):"
	@$(SIZE_CMD)

clean:
	rm -rf $(BUILD_DIR) $(OUTPUT_DIR)
	@echo "Cleaned build and output directories"

# Deploy to disting NT (macOS - adjust path for your SD card mount)
deploy: hardware
	@if [ -d "/Volumes/DISTINGNT" ]; then \
		cp $(OUTPUT_DIR)/$(PLUGIN_NAME).o /Volumes/DISTINGNT/; \
		echo "Deployed to /Volumes/DISTINGNT/"; \
	else \
		echo "DISTINGNT volume not mounted"; \
	fi

help:
	@echo "Tangents - Steiner-Parker Filter Build System"
	@echo "=============================================="
	@echo ""
	@echo "Targets:"
	@echo "  hardware    - Build for distingNT hardware (.o)"
	@echo "  test        - Build for nt_emu testing (.dylib/.so/.dll)"
	@echo "  both        - Build both targets"
	@echo "  check       - Check undefined symbols"
	@echo "  size        - Show plugin size"
	@echo "  clean       - Remove build artifacts"
	@echo "  deploy      - Copy hardware build to DISTINGNT volume"
	@echo "  help        - Show this help"
	@echo ""
	@echo "Testing Workflow:"
	@echo "  1. make test              # Build for nt_emu"
	@echo "  2. Load in VCV Rack       # Test interactively"
	@echo "  3. make hardware          # Build for hardware when ready"
	@echo "  4. make deploy            # Copy to distingNT SD card"

.PHONY: all hardware test both check size clean deploy help

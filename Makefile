CC ?= cc
PKG_CONFIG ?= pkg-config

BUILD_DIR := build
TARGET := $(BUILD_DIR)/tty-ui
SOURCES := src/main.c src/font_atlas.c src/kms_drm.c
OBJECTS := $(SOURCES:src/%.c=$(BUILD_DIR)/%.o)
FONT_FILE := fonts/wqy-microhei.ttc
FONT_ATLAS_C := src/font_atlas.c
FONT_ATLAS_H := src/font_atlas.h
FONT_ATLAS_GENERATOR := $(BUILD_DIR)/generate_font_atlas

CPPFLAGS += -D_DEFAULT_SOURCE
BASE_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic
CFLAGS ?= -O2 -g
LDFLAGS ?=
STATIC ?= 1
TARGET_CFLAGS :=
TARGET_LDFLAGS :=
GEN_FREETYPE_CFLAGS = $(shell $(PKG_CONFIG) --cflags freetype2)
GEN_FREETYPE_LIBS = $(shell $(PKG_CONFIG) --libs freetype2)

ifeq ($(STATIC),1)
TARGET_CFLAGS += -fno-pie
TARGET_LDFLAGS += -static -no-pie
endif

.PHONY: all clean run font-atlas

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) $(TARGET_CFLAGS) -MMD -MP -c $< -o $@

font-atlas: $(FONT_ATLAS_GENERATOR)
	$(FONT_ATLAS_GENERATOR) $(FONT_FILE) $(FONT_ATLAS_C) $(FONT_ATLAS_H)

$(FONT_ATLAS_GENERATOR): tools/generate_font_atlas.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) $(GEN_FREETYPE_CFLAGS) $< -o $@ $(GEN_FREETYPE_LIBS)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(TARGET_LDFLAGS) $(OBJECTS) -o $@

run: $(TARGET)
	$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

-include $(OBJECTS:.o=.d)

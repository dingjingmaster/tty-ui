CC ?= cc
PKG_CONFIG ?= pkg-config

BUILD_DIR := build
TARGET := $(BUILD_DIR)/tty-ui
SOURCES := src/main.c
OBJECTS := $(SOURCES:src/%.c=$(BUILD_DIR)/%.o)
PKGS := libdrm gbm egl glesv2 freetype2 fontconfig

CPPFLAGS += -D_DEFAULT_SOURCE
BASE_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic
CFLAGS ?= -O2 -g
PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKGS))
PKG_LIBS := $(shell $(PKG_CONFIG) --libs $(PKGS))

.PHONY: all clean run

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) $(PKG_CFLAGS) -MMD -MP -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(PKG_LIBS) -lm

run: $(TARGET)
	$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

-include $(OBJECTS:.o=.d)

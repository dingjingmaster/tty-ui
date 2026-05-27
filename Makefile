CC ?= cc
LD ?= ld
OBJCOPY ?= objcopy
PKG_CONFIG ?= pkg-config

BUILD_DIR := build
TARGET := $(BUILD_DIR)/tty-ui
SOURCES := src/main.c
OBJECTS := $(SOURCES:src/%.c=$(BUILD_DIR)/%.o)
FONT_FILE := fonts/wqy-microhei.ttc
FONT_OBJECT := $(BUILD_DIR)/wqy-microhei-font.o
PKGS := libdrm gbm egl glesv2
FREETYPE_CFLAGS := $(shell $(PKG_CONFIG) --cflags freetype2)
FREETYPE_LIBDIR := $(shell $(PKG_CONFIG) --variable=libdir freetype2)
FREETYPE_STATIC := $(FREETYPE_LIBDIR)/libfreetype.a
FREETYPE_EXTRA_LIBS := $(filter-out -lfreetype,$(shell $(PKG_CONFIG) --libs --static freetype2))
FREETYPE_LIBS := $(if $(wildcard $(FREETYPE_STATIC)),$(FREETYPE_STATIC) $(FREETYPE_EXTRA_LIBS),$(shell $(PKG_CONFIG) --libs freetype2))

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
	$(CC) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) $(PKG_CFLAGS) $(FREETYPE_CFLAGS) -MMD -MP -c $< -o $@

$(FONT_OBJECT): $(FONT_FILE) | $(BUILD_DIR)
	$(LD) -r -b binary $< -o $@
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents $@
	$(OBJCOPY) --add-section .note.GNU-stack=/dev/null --set-section-flags .note.GNU-stack=contents,readonly $@

$(TARGET): $(OBJECTS) $(FONT_OBJECT)
	$(CC) $(CFLAGS) $(OBJECTS) $(FONT_OBJECT) -o $@ $(PKG_LIBS) $(FREETYPE_LIBS) -lm

run: $(TARGET)
	$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

-include $(OBJECTS:.o=.d)

#ifndef FONT_ATLAS_H
#define FONT_ATLAS_H

#include <stddef.h>
#include <stdint.h>

struct font_glyph {
	uint32_t codepoint;
	uint16_t size;
	int16_t width;
	int16_t height;
	int16_t left;
	int16_t top;
	int16_t advance;
	uint32_t offset;
};

extern const struct font_glyph font_glyphs[];
extern const size_t font_glyph_count;
extern const unsigned char font_bitmap_data[];
extern const size_t font_bitmap_data_size;

#endif

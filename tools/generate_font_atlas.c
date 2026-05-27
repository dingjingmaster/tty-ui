#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

struct codepoint_set {
	uint32_t *items;
	size_t count;
	size_t capacity;
};

struct glyph_record {
	uint32_t codepoint;
	uint16_t size;
	int16_t width;
	int16_t height;
	int16_t left;
	int16_t top;
	int16_t advance;
	uint32_t offset;
	unsigned char *bitmap;
	size_t bitmap_size;
};

static const int font_sizes[] = { 22, 24, 28, 34 };
static const char *const ui_texts[] = {
	"安得合众",
	"DiskCrypt登录",
	"用 Tab 切换区域，Enter键确认",
	"用户名称:",
	"用户密码:",
	"继续启动",
	"退出",
	"全盘加密  安全无忧",
};

static void *xrealloc(void *ptr, size_t size)
{
	void *next = realloc(ptr, size);

	if (!next) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	return next;
}

static void *xmalloc(size_t size)
{
	void *ptr = malloc(size);

	if (!ptr) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	return ptr;
}

static uint32_t utf8_next(const char **cursor)
{
	const unsigned char *s = (const unsigned char *)*cursor;
	uint32_t codepoint;
	int length;

	if (!s[0])
		return 0;
	if (s[0] < 0x80) {
		*cursor = (const char *)(s + 1);
		return s[0];
	}
	if ((s[0] & 0xe0) == 0xc0) {
		codepoint = s[0] & 0x1f;
		length = 2;
	} else if ((s[0] & 0xf0) == 0xe0) {
		codepoint = s[0] & 0x0f;
		length = 3;
	} else if ((s[0] & 0xf8) == 0xf0) {
		codepoint = s[0] & 0x07;
		length = 4;
	} else {
		*cursor = (const char *)(s + 1);
		return '?';
	}

	for (int i = 1; i < length; i++) {
		if ((s[i] & 0xc0) != 0x80) {
			*cursor = (const char *)(s + 1);
			return '?';
		}
		codepoint = (codepoint << 6) | (s[i] & 0x3f);
	}

	*cursor = (const char *)(s + length);
	return codepoint;
}

static void add_codepoint(struct codepoint_set *set, uint32_t codepoint)
{
	for (size_t i = 0; i < set->count; i++) {
		if (set->items[i] == codepoint)
			return;
	}

	if (set->count == set->capacity) {
		size_t next_capacity = set->capacity ? set->capacity * 2 : 128;
		set->items = xrealloc(set->items,
				      next_capacity * sizeof(set->items[0]));
		set->capacity = next_capacity;
	}
	set->items[set->count++] = codepoint;
}

static void add_text(struct codepoint_set *set, const char *text)
{
	const char *cursor = text;

	while (*cursor)
		add_codepoint(set, utf8_next(&cursor));
}

static int compare_codepoints(const void *left, const void *right)
{
	const uint32_t a = *(const uint32_t *)left;
	const uint32_t b = *(const uint32_t *)right;

	if (a < b)
		return -1;
	if (a > b)
		return 1;
	return 0;
}

static void build_codepoint_set(struct codepoint_set *set)
{
	memset(set, 0, sizeof(*set));
	for (uint32_t codepoint = 0x20; codepoint <= 0x7e; codepoint++)
		add_codepoint(set, codepoint);
	for (size_t i = 0; i < sizeof(ui_texts) / sizeof(ui_texts[0]); i++)
		add_text(set, ui_texts[i]);
	qsort(set->items, set->count, sizeof(set->items[0]), compare_codepoints);
}

static int render_glyph(FT_Face face, struct glyph_record *record)
{
	FT_GlyphSlot glyph;
	FT_Bitmap *bitmap;
	FT_Error error;
	size_t bitmap_size;

	error = FT_Set_Pixel_Sizes(face, 0, record->size);
	if (error)
		return -1;

	error = FT_Load_Char(face, record->codepoint, FT_LOAD_RENDER);
	if (error && record->codepoint != '?')
		error = FT_Load_Char(face, '?', FT_LOAD_RENDER);
	if (error)
		return -1;

	glyph = face->glyph;
	bitmap = &glyph->bitmap;
	if (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY && bitmap->width &&
	    bitmap->rows) {
		fprintf(stderr, "unsupported glyph pixel mode: %u\n",
			bitmap->pixel_mode);
		return -1;
	}

	record->width = (int16_t)bitmap->width;
	record->height = (int16_t)bitmap->rows;
	record->left = (int16_t)glyph->bitmap_left;
	record->top = (int16_t)glyph->bitmap_top;
	record->advance = (int16_t)(glyph->advance.x >> 6);
	bitmap_size = (size_t)record->width * (size_t)record->height;
	record->bitmap_size = bitmap_size;
	if (!bitmap_size)
		return 0;

	record->bitmap = xmalloc(bitmap_size);
	for (int y = 0; y < record->height; y++) {
		const unsigned char *src_row;

		if (bitmap->pitch >= 0)
			src_row = bitmap->buffer + (size_t)y *
						   (size_t)bitmap->pitch;
		else
			src_row = bitmap->buffer +
				  (size_t)(record->height - 1 - y) *
					  (size_t)(-bitmap->pitch);
		memcpy(record->bitmap + (size_t)y * (size_t)record->width,
		       src_row, (size_t)record->width);
	}

	return 0;
}

static int write_header(const char *path)
{
	FILE *out = fopen(path, "w");

	if (!out) {
		fprintf(stderr, "could not open %s: %s\n", path, strerror(errno));
		return -1;
	}

	fprintf(out,
		"#ifndef FONT_ATLAS_H\n"
		"#define FONT_ATLAS_H\n"
		"\n"
		"#include <stddef.h>\n"
		"#include <stdint.h>\n"
		"\n"
		"struct font_glyph {\n"
		"\tuint32_t codepoint;\n"
		"\tuint16_t size;\n"
		"\tint16_t width;\n"
		"\tint16_t height;\n"
		"\tint16_t left;\n"
		"\tint16_t top;\n"
		"\tint16_t advance;\n"
		"\tuint32_t offset;\n"
		"};\n"
		"\n"
		"extern const struct font_glyph font_glyphs[];\n"
		"extern const size_t font_glyph_count;\n"
		"extern const unsigned char font_bitmap_data[];\n"
		"extern const size_t font_bitmap_data_size;\n"
		"\n"
		"#endif\n");

	if (fclose(out)) {
		fprintf(stderr, "could not close %s: %s\n", path, strerror(errno));
		return -1;
	}
	return 0;
}

static int write_source(const char *path, const struct glyph_record *records,
			size_t record_count)
{
	FILE *out = fopen(path, "w");
	size_t total_bitmap_size = 0;

	if (!out) {
		fprintf(stderr, "could not open %s: %s\n", path, strerror(errno));
		return -1;
	}

	for (size_t i = 0; i < record_count; i++)
		total_bitmap_size += records[i].bitmap_size;

	fprintf(out,
		"#include \"font_atlas.h\"\n"
		"\n"
		"const struct font_glyph font_glyphs[] = {\n");
	for (size_t i = 0; i < record_count; i++) {
		const struct glyph_record *glyph = &records[i];

		fprintf(out,
			"\t{ 0x%04x, %u, %d, %d, %d, %d, %d, %u },\n",
			glyph->codepoint, glyph->size, glyph->width,
			glyph->height, glyph->left, glyph->top,
			glyph->advance, glyph->offset);
	}
	fprintf(out,
		"};\n"
		"\n"
		"const size_t font_glyph_count = sizeof(font_glyphs) / "
		"sizeof(font_glyphs[0]);\n"
		"\n"
		"const unsigned char font_bitmap_data[] = {\n");

	for (size_t i = 0; i < record_count; i++) {
		const struct glyph_record *glyph = &records[i];

		for (size_t j = 0; j < glyph->bitmap_size; j++) {
			if ((j % 12) == 0)
				fprintf(out, "\t");
			fprintf(out, "0x%02x,", glyph->bitmap[j]);
			if ((j % 12) == 11 || j + 1 == glyph->bitmap_size)
				fprintf(out, "\n");
			else
				fprintf(out, " ");
		}
	}

	fprintf(out,
		"};\n"
		"\n"
		"const size_t font_bitmap_data_size = sizeof(font_bitmap_data);\n");

	if (total_bitmap_size == 0)
		fprintf(out, "\n");

	if (fclose(out)) {
		fprintf(stderr, "could not close %s: %s\n", path, strerror(errno));
		return -1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	FT_Library library = NULL;
	FT_Face face = NULL;
	struct codepoint_set codepoints;
	struct glyph_record *records = NULL;
	size_t record_count;
	size_t bitmap_offset = 0;
	int ret = 1;

	if (argc != 4) {
		fprintf(stderr,
			"Usage: %s <font-file> <out-c> <out-h>\n",
			argv[0]);
		return 1;
	}

	build_codepoint_set(&codepoints);
	record_count = codepoints.count *
		       (sizeof(font_sizes) / sizeof(font_sizes[0]));
	records = calloc(record_count, sizeof(records[0]));
	if (!records) {
		fprintf(stderr, "out of memory\n");
		goto out;
	}

	if (FT_Init_FreeType(&library)) {
		fprintf(stderr, "failed to initialize freetype\n");
		goto out;
	}
	if (FT_New_Face(library, argv[1], 0, &face)) {
		fprintf(stderr, "failed to load font %s\n", argv[1]);
		goto out;
	}
	FT_Select_Charmap(face, FT_ENCODING_UNICODE);

	for (size_t size_index = 0;
	     size_index < sizeof(font_sizes) / sizeof(font_sizes[0]);
	     size_index++) {
		for (size_t i = 0; i < codepoints.count; i++) {
			size_t index = size_index * codepoints.count + i;

			records[index].codepoint = codepoints.items[i];
			records[index].size = (uint16_t)font_sizes[size_index];
			if (render_glyph(face, &records[index])) {
				fprintf(stderr,
					"failed to render U+%04x at %d px\n",
					codepoints.items[i], font_sizes[size_index]);
				goto out;
			}
			records[index].offset = (uint32_t)bitmap_offset;
			bitmap_offset += records[index].bitmap_size;
		}
	}

	if (write_header(argv[3]) || write_source(argv[2], records, record_count))
		goto out;

	ret = 0;

out:
	if (face)
		FT_Done_Face(face);
	if (library)
		FT_Done_FreeType(library);
	if (records) {
		for (size_t i = 0; i < record_count; i++)
			free(records[i].bitmap);
		free(records);
	}
	free(codepoints.items);
	return ret;
}

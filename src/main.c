#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "font_atlas.h"
#include "kms_drm.h"

#define DEFAULT_DRM_DEVICE "/dev/dri/card0"
#define INPUT_LIMIT 128
#define BUFFER_COUNT 2

struct color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

struct drm_buffer {
	uint32_t handle;
	uint32_t fb_id;
	uint32_t pitch;
	uint64_t size;
	uint8_t *map;
};

struct display {
	int fd;
	uint32_t connector_id;
	uint32_t crtc_id;
	struct kms_mode_info mode;
	struct kms_crtc *old_crtc;
	struct drm_buffer buffers[BUFFER_COUNT];
	int width;
	int height;
	int front_buffer;
	bool mode_set;
};

struct canvas {
	uint8_t *pixels;
	int width;
	int height;
	uint32_t pitch;
};

enum focus_target {
	FOCUS_USER = 0,
	FOCUS_PASSWORD,
	FOCUS_CONTINUE,
	FOCUS_EXIT,
	FOCUS_COUNT,
};

struct ui_state {
	char username[INPUT_LIMIT];
	char password[INPUT_LIMIT];
	size_t username_len;
	size_t password_len;
	enum focus_target focus;
	bool done;
	int exit_code;
};

struct terminal_state {
	struct termios old_termios;
	bool active;
};

struct text_bounds {
	int min_x;
	int min_y;
	int width;
	int height;
};

static volatile sig_atomic_t g_stop_requested;

static void on_signal(int signo)
{
	(void)signo;
	g_stop_requested = 1;
}

static float clampf(float value, float low, float high)
{
	if (value < low)
		return low;
	if (value > high)
		return high;
	return value;
}

static struct color rgb(uint8_t r, uint8_t g, uint8_t b)
{
	return (struct color){ r, g, b, 255 };
}

static uint32_t pack_xrgb(struct color color)
{
	return ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) | color.b;
}

static void blend_pixel(struct canvas *canvas, int x, int y, struct color color,
			uint8_t alpha)
{
	uint32_t *row;
	uint32_t dst;
	uint32_t inv;
	uint8_t r;
	uint8_t g;
	uint8_t b;

	if (x < 0 || y < 0 || x >= canvas->width || y >= canvas->height || alpha == 0)
		return;

	row = (uint32_t *)(canvas->pixels + (size_t)y * canvas->pitch);
	if (alpha == 255 && color.a == 255) {
		row[x] = pack_xrgb(color);
		return;
	}

	alpha = (uint8_t)(((unsigned int)alpha * color.a) / 255U);
	inv = 255U - alpha;
	dst = row[x];
	r = (uint8_t)(((unsigned int)color.r * alpha + ((dst >> 16) & 0xffU) * inv) / 255U);
	g = (uint8_t)(((unsigned int)color.g * alpha + ((dst >> 8) & 0xffU) * inv) / 255U);
	b = (uint8_t)(((unsigned int)color.b * alpha + (dst & 0xffU) * inv) / 255U);
	row[x] = pack_xrgb((struct color){ r, g, b, 255 });
}

static void draw_rect(struct canvas *canvas, float fx, float fy, float fw, float fh,
		      struct color color)
{
	int x0 = (int)fx;
	int y0 = (int)fy;
	int x1 = (int)(fx + fw);
	int y1 = (int)(fy + fh);

	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (x1 > canvas->width)
		x1 = canvas->width;
	if (y1 > canvas->height)
		y1 = canvas->height;
	if (x0 >= x1 || y0 >= y1)
		return;

	if (color.a == 255) {
		uint32_t value = pack_xrgb(color);
		for (int y = y0; y < y1; y++) {
			uint32_t *row = (uint32_t *)(canvas->pixels + (size_t)y * canvas->pitch);
			for (int x = x0; x < x1; x++)
				row[x] = value;
		}
		return;
	}

	for (int y = y0; y < y1; y++) {
		for (int x = x0; x < x1; x++)
			blend_pixel(canvas, x, y, color, 255);
	}
}

static void draw_rect_outline(struct canvas *canvas, float x, float y, float width,
			      float height, float thickness, struct color color)
{
	draw_rect(canvas, x, y, width, thickness, color);
	draw_rect(canvas, x, y + height - thickness, width, thickness, color);
	draw_rect(canvas, x, y, thickness, height, color);
	draw_rect(canvas, x + width - thickness, y, thickness, height, color);
}

static const struct font_glyph *find_glyph(uint32_t codepoint, int pixel_size)
{
	const struct font_glyph *fallback = NULL;

	for (size_t i = 0; i < font_glyph_count; i++) {
		const struct font_glyph *glyph = &font_glyphs[i];

		if ((int)glyph->size != pixel_size)
			continue;
		if (glyph->codepoint == codepoint)
			return glyph;
		if (glyph->codepoint == '?')
			fallback = glyph;
	}
	return fallback;
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

static size_t utf8_count(const char *text)
{
	size_t count = 0;
	const char *cursor = text;

	while (*cursor) {
		utf8_next(&cursor);
		count++;
	}
	return count;
}

static void utf8_trim_last(char *text, size_t *length)
{
	size_t i;

	if (*length == 0)
		return;

	i = *length - 1;
	while (i > 0 && (((unsigned char)text[i] & 0xc0) == 0x80))
		i--;

	text[i] = '\0';
	*length = i;
}

static int measure_text(const char *text, int pixel_size,
			struct text_bounds *bounds)
{
	const char *cursor = text;
	int pen_x = 0;
	int min_x = 0;
	int max_x = 0;
	int min_y = 0;
	int max_y = 0;

	while (*cursor) {
		uint32_t codepoint = utf8_next(&cursor);
		const struct font_glyph *glyph = find_glyph(codepoint, pixel_size);
		int gx;
		int gy;

		if (!glyph)
			continue;

		gx = pen_x + glyph->left;
		gy = -glyph->top;

		if (gx < min_x)
			min_x = gx;
		if (gx + glyph->width > max_x)
			max_x = gx + glyph->width;
		if (gy < min_y)
			min_y = gy;
		if (gy + glyph->height > max_y)
			max_y = gy + glyph->height;

		pen_x += glyph->advance;
		if (pen_x > max_x)
			max_x = pen_x;
	}

	bounds->min_x = min_x;
	bounds->min_y = min_y;
	bounds->width = max_x - min_x;
	bounds->height = max_y - min_y;
	if (bounds->width < 0)
		bounds->width = 0;
	if (bounds->height <= 0)
		bounds->height = pixel_size;
	return 0;
}

static int draw_text(struct canvas *canvas, float fx, float fy, const char *text,
		     int pixel_size,
		     struct color color)
{
	const char *cursor = text;
	struct text_bounds bounds;
	int pen_x = 0;
	int origin_x;
	int baseline;

	if (measure_text(text, pixel_size, &bounds))
		return -1;

	origin_x = (int)fx - bounds.min_x;
	baseline = (int)fy - bounds.min_y;

	while (*cursor) {
		uint32_t codepoint = utf8_next(&cursor);
		const struct font_glyph *glyph = find_glyph(codepoint, pixel_size);
		const unsigned char *bitmap;
		int dst_x;
		int dst_y;

		if (!glyph)
			continue;
		if ((size_t)glyph->offset +
			    (size_t)glyph->width * (size_t)glyph->height >
		    font_bitmap_data_size)
			return -1;

		bitmap = font_bitmap_data + glyph->offset;
		dst_x = origin_x + pen_x + glyph->left;
		dst_y = baseline - glyph->top;

		for (int y = 0; y < glyph->height; y++) {
			const unsigned char *src_row =
				bitmap + (size_t)y * (size_t)glyph->width;

			for (int x = 0; x < glyph->width; x++)
				blend_pixel(canvas, dst_x + x, dst_y + y,
					    color, src_row[x]);
		}

		pen_x += glyph->advance;
	}

	return 0;
}

static int draw_text_centered(struct canvas *canvas, const char *text,
			      int pixel_size, float center_x,
			      float y, struct color color)
{
	struct text_bounds bounds;

	if (measure_text(text, pixel_size, &bounds))
		return -1;

	return draw_text(canvas, center_x - (float)bounds.width / 2.0f, y, text,
			 pixel_size, color);
}

static uint32_t find_crtc_for_encoder(const struct kms_resources *resources,
				      const struct kms_encoder *encoder)
{
	for (int i = 0; i < resources->count_crtcs; i++) {
		if (encoder->possible_crtcs & (1u << i))
			return resources->crtcs[i];
	}
	return 0;
}

static uint32_t find_crtc_for_connector(int fd,
					const struct kms_resources *resources,
					const struct kms_connector *connector)
{
	for (int i = 0; i < connector->count_encoders; i++) {
		struct kms_encoder *encoder =
			kms_get_encoder(fd, connector->encoders[i]);
		uint32_t crtc_id = 0;

		if (!encoder)
			continue;
		crtc_id = find_crtc_for_encoder(resources, encoder);
		kms_free_encoder(encoder);
		if (crtc_id)
			return crtc_id;
	}
	return 0;
}

static int init_drm(struct display *display, const char *device)
{
	struct kms_resources *resources = NULL;
	struct kms_connector *connector = NULL;
	struct kms_encoder *encoder = NULL;
	int chosen_mode = -1;
	int largest_mode = -1;
	int largest_area = 0;
	int ret = -1;

	display->fd = open(device, O_RDWR | O_CLOEXEC);
	if (display->fd < 0) {
		fprintf(stderr, "could not open drm device %s: %s\n", device,
			strerror(errno));
		return -1;
	}

	resources = kms_get_resources(display->fd);
	if (!resources) {
		fprintf(stderr, "kms_get_resources failed: %s\n", strerror(errno));
		goto out;
	}

	for (int i = 0; i < resources->count_connectors; i++) {
		connector = kms_get_connector(display->fd,
					      resources->connectors[i]);
		if (!connector)
			continue;
		if (connector->connection == KMS_MODE_CONNECTED &&
		    connector->count_modes > 0)
			break;
		kms_free_connector(connector);
		connector = NULL;
	}

	if (!connector) {
		fprintf(stderr, "no connected drm connector found\n");
		goto out;
	}

	for (int i = 0; i < connector->count_modes; i++) {
		int area = connector->modes[i].hdisplay * connector->modes[i].vdisplay;

		if ((connector->modes[i].type & KMS_MODE_TYPE_PREFERRED) &&
		    chosen_mode < 0)
			chosen_mode = i;
		if (area > largest_area) {
			largest_area = area;
			largest_mode = i;
		}
	}
	if (chosen_mode < 0)
		chosen_mode = largest_mode;
	if (chosen_mode < 0)
		goto out;

	display->mode = connector->modes[chosen_mode];
	display->connector_id = connector->connector_id;
	display->width = display->mode.hdisplay;
	display->height = display->mode.vdisplay;

	if (connector->encoder_id)
		encoder = kms_get_encoder(display->fd, connector->encoder_id);
	if (encoder && encoder->crtc_id)
		display->crtc_id = encoder->crtc_id;
	else
		display->crtc_id = find_crtc_for_connector(display->fd, resources,
							   connector);

	if (!display->crtc_id) {
		fprintf(stderr, "no usable crtc found\n");
		goto out;
	}

	display->old_crtc = kms_get_crtc(display->fd, display->crtc_id);
	ret = 0;

out:
	if (encoder)
		kms_free_encoder(encoder);
	if (connector)
		kms_free_connector(connector);
	if (resources)
		kms_free_resources(resources);
	if (ret && display->fd >= 0) {
		close(display->fd);
		display->fd = -1;
	}
	return ret;
}

static void destroy_dumb_buffer(struct display *display, struct drm_buffer *buffer)
{
	if (buffer->map && buffer->map != MAP_FAILED)
		munmap(buffer->map, buffer->size);
	if (buffer->fb_id)
		kms_mode_rm_fb(display->fd, buffer->fb_id);
	if (buffer->handle)
		kms_destroy_dumb_buffer(display->fd, buffer->handle);
	memset(buffer, 0, sizeof(*buffer));
}

static int create_dumb_buffer(struct display *display, struct drm_buffer *buffer)
{
	uint64_t map_offset;
	int ret;

	memset(buffer, 0, sizeof(*buffer));
	ret = kms_create_dumb_buffer(display->fd, (uint32_t)display->width,
				     (uint32_t)display->height, 32,
				     &buffer->handle, &buffer->pitch,
				     &buffer->size);
	if (ret) {
		fprintf(stderr, "DRM_IOCTL_MODE_CREATE_DUMB failed: %s\n",
			strerror(errno));
		return -1;
	}

	ret = kms_mode_add_fb(display->fd, (uint32_t)display->width,
			      (uint32_t)display->height, 24, 32,
			      buffer->pitch, buffer->handle, &buffer->fb_id);
	if (ret) {
		fprintf(stderr, "kms_mode_add_fb failed: %s\n", strerror(errno));
		destroy_dumb_buffer(display, buffer);
		return -1;
	}

	ret = kms_map_dumb_buffer(display->fd, buffer->handle, &map_offset);
	if (ret) {
		fprintf(stderr, "DRM_IOCTL_MODE_MAP_DUMB failed: %s\n",
			strerror(errno));
		destroy_dumb_buffer(display, buffer);
		return -1;
	}

	buffer->map = mmap(NULL, buffer->size, PROT_READ | PROT_WRITE, MAP_SHARED,
			   display->fd, (off_t)map_offset);
	if (buffer->map == MAP_FAILED) {
		fprintf(stderr, "mmap dumb buffer failed: %s\n", strerror(errno));
		destroy_dumb_buffer(display, buffer);
		return -1;
	}

	memset(buffer->map, 0, buffer->size);
	return 0;
}

static void page_flip_handler(int fd, unsigned int frame, unsigned int sec,
			      unsigned int usec, void *data)
{
	bool *waiting = data;

	(void)fd;
	(void)frame;
	(void)sec;
	(void)usec;
	if (waiting)
		*waiting = false;
}

static int wait_for_page_flip(int fd, bool *waiting)
{
	struct kms_event_context event_context = {
		.version = 2,
		.page_flip_handler = page_flip_handler,
	};

	while (*waiting && !g_stop_requested) {
		fd_set fds;
		int ret;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		ret = select(fd + 1, &fds, NULL, NULL, NULL);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "select failed while waiting page flip: %s\n",
				strerror(errno));
			return -1;
		}
		if (FD_ISSET(fd, &fds))
			kms_handle_event(fd, &event_context);
	}

	return g_stop_requested ? -1 : 0;
}

static int display_present(struct display *display, int buffer_index)
{
	struct drm_buffer *buffer = &display->buffers[buffer_index];
	int ret;

	if (!display->mode_set) {
		ret = kms_mode_set_crtc(display->fd, display->crtc_id,
					buffer->fb_id, 0, 0,
					&display->connector_id, 1,
					&display->mode);
		if (ret) {
			fprintf(stderr, "kms_mode_set_crtc failed: %s\n",
				strerror(errno));
			return -1;
		}
		display->mode_set = true;
		display->front_buffer = buffer_index;
		return 0;
	}

	bool waiting = true;
	ret = kms_mode_page_flip(display->fd, display->crtc_id, buffer->fb_id,
				 KMS_MODE_PAGE_FLIP_EVENT, &waiting);
	if (ret) {
		fprintf(stderr, "kms_mode_page_flip failed: %s\n",
			strerror(errno));
		return -1;
	}
	if (wait_for_page_flip(display->fd, &waiting))
		return -1;

	display->front_buffer = buffer_index;
	return 0;
}

static int display_init(struct display *display, const char *device)
{
	memset(display, 0, sizeof(*display));
	display->fd = -1;
	display->front_buffer = -1;

	if (init_drm(display, device))
		return -1;
	for (int i = 0; i < BUFFER_COUNT; i++) {
		if (create_dumb_buffer(display, &display->buffers[i]))
			goto fail;
	}

	return 0;

fail:
	for (int i = 0; i < BUFFER_COUNT; i++)
		destroy_dumb_buffer(display, &display->buffers[i]);
	if (display->old_crtc)
		kms_free_crtc(display->old_crtc);
	if (display->fd >= 0)
		close(display->fd);
	return -1;
}

static void display_destroy(struct display *display)
{
	if (display->mode_set && display->fd >= 0 && display->old_crtc) {
		kms_mode_set_crtc(display->fd, display->old_crtc->crtc_id,
				  display->old_crtc->buffer_id,
				  display->old_crtc->x, display->old_crtc->y,
				  &display->connector_id, 1,
				  &display->old_crtc->mode);
	}

	for (int i = 0; i < BUFFER_COUNT; i++)
		destroy_dumb_buffer(display, &display->buffers[i]);
	if (display->old_crtc)
		kms_free_crtc(display->old_crtc);
	if (display->fd >= 0)
		close(display->fd);
}

static struct canvas canvas_for_buffer(struct display *display, int index)
{
	return (struct canvas){
		.pixels = display->buffers[index].map,
		.width = display->width,
		.height = display->height,
		.pitch = display->buffers[index].pitch,
	};
}

static void make_password_mask(const struct ui_state *state, char *out,
			       size_t out_size)
{
	size_t count = utf8_count(state->password);
	size_t max = out_size > 0 ? out_size - 1 : 0;
	size_t n = count < max ? count : max;

	for (size_t i = 0; i < n; i++)
		out[i] = '*';
	if (out_size)
		out[n] = '\0';
}

static void draw_input(struct canvas *canvas, float x, float y, float width,
		       float height,
		       const char *value, bool focused)
{
	struct color input_bg = rgb(19, 23, 27);
	struct color border = focused ? rgb(77, 189, 209) : rgb(87, 99, 110);
	struct color text_color = rgb(237, 245, 245);
	struct text_bounds bounds;
	float text_x = x + 14.0f;
	float text_y;

	draw_rect(canvas, x, y, width, height, input_bg);
	draw_rect_outline(canvas, x, y, width, height, focused ? 3.0f : 2.0f,
			  border);

	measure_text(value, 22, &bounds);
	text_y = y + (height - (float)bounds.height) / 2.0f;
	if (value[0])
		draw_text(canvas, text_x, text_y, value, 22, text_color);
	if (focused) {
		float cursor_x = text_x + (float)bounds.width + 3.0f;
		draw_rect(canvas, cursor_x, y + 9.0f, 2.0f, height - 18.0f,
			  rgb(77, 189, 209));
	}
}

static void draw_button(struct canvas *canvas, float x, float y, float width,
			float height,
			const char *label, bool focused)
{
	struct color bg = focused ? rgb(33, 87, 99) : rgb(31, 38, 43);
	struct color border = focused ? rgb(77, 189, 209) : rgb(94, 110, 117);
	struct color fg = rgb(242, 250, 250);
	struct text_bounds bounds;

	draw_rect(canvas, x, y, width, height, bg);
	draw_rect_outline(canvas, x, y, width, height, focused ? 3.0f : 2.0f,
			  border);
	measure_text(label, 22, &bounds);
	draw_text(canvas, x + (width - (float)bounds.width) / 2.0f,
		  y + (height - (float)bounds.height) / 2.0f, label, 22, fg);
}

static void render_ui(struct display *display, int buffer_index,
		      const struct ui_state *state)
{
	struct canvas canvas = canvas_for_buffer(display, buffer_index);
	const float w = (float)display->width;
	const float h = (float)display->height;
	const float margin = clampf(w * 0.045f, 28.0f, 72.0f);
	const float header_h = clampf(h * 0.15f, 82.0f, 120.0f);
	const float center_x = w / 2.0f;
	const float title_y = margin + header_h + clampf(h * 0.085f, 44.0f, 86.0f);
	const float form_w = clampf(w * 0.54f, 500.0f, 660.0f);
	const float label_w = 135.0f;
	const float input_w = form_w - label_w - 18.0f;
	const float row_h = 42.0f;
	const float form_x = center_x - form_w / 2.0f;
	const float input_x = form_x + label_w + 18.0f;
	const float user_y = title_y + 126.0f;
	const float pass_y = user_y + 58.0f;
	const float button_y = pass_y + 78.0f;
	const float button_w = 150.0f;
	const float button_h = 44.0f;
	const float button_gap = 42.0f;
	const float continue_button_x = center_x - button_w - button_gap / 2.0f;
	const float exit_button_x = form_x + form_w - button_w;
	char mask[INPUT_LIMIT];
	struct text_bounds label_bounds;
	struct text_bounds tagline_bounds;

	draw_rect(&canvas, 0.0f, 0.0f, w, h, rgb(11, 14, 16));
	draw_rect_outline(&canvas, margin, margin, w - margin * 2.0f,
			  h - margin * 2.0f, 2.0f, rgb(87, 102, 110));
	draw_rect(&canvas, margin + 2.0f, margin + 2.0f,
		  w - margin * 2.0f - 4.0f, header_h - 2.0f, rgb(18, 22, 26));
	draw_rect(&canvas, margin, margin + header_h, w - margin * 2.0f,
		  2.0f, rgb(87, 102, 110));
	draw_text(&canvas, margin + 34.0f, margin + 27.0f, "安得合众", 28,
		  rgb(235, 245, 245));

	draw_text_centered(&canvas, "DiskCrypt登录", 34, center_x, title_y,
			   rgb(235, 245, 245));
	draw_text_centered(&canvas, "用 Tab 切换区域，Enter键确认", 22,
			   center_x, title_y + 58.0f, rgb(171, 191, 194));

	measure_text("用户名称:", 22, &label_bounds);
	draw_text(&canvas, form_x + label_w - (float)label_bounds.width,
		  user_y + (row_h - (float)label_bounds.height) / 2.0f,
		  "用户名称:", 22, rgb(235, 245, 245));
	draw_input(&canvas, input_x, user_y, input_w, row_h, state->username,
		   state->focus == FOCUS_USER);

	make_password_mask(state, mask, sizeof(mask));
	measure_text("用户密码:", 22, &label_bounds);
	draw_text(&canvas, form_x + label_w - (float)label_bounds.width,
		  pass_y + (row_h - (float)label_bounds.height) / 2.0f,
		  "用户密码:", 22, rgb(235, 245, 245));
	draw_input(&canvas, input_x, pass_y, input_w, row_h, mask,
		   state->focus == FOCUS_PASSWORD);

	draw_button(&canvas, continue_button_x, button_y, button_w, button_h,
		    "继续启动", state->focus == FOCUS_CONTINUE);
	draw_button(&canvas, exit_button_x, button_y, button_w, button_h,
		    "退出", state->focus == FOCUS_EXIT);

	draw_rect(&canvas, margin + 1.0f, h - margin - 78.0f,
		  w - margin * 2.0f - 2.0f, 1.0f, rgb(46, 56, 59));
	measure_text("全盘加密  安全无忧", 24, &tagline_bounds);
	draw_text(&canvas, w - margin - 32.0f - (float)tagline_bounds.width,
		  h - margin - 48.0f - (float)tagline_bounds.height,
		  "全盘加密  安全无忧", 24, rgb(179, 224, 171));
}

static bool append_input(char *buffer, size_t *length, size_t capacity,
			 unsigned char ch)
{
	if (*length + 1 >= capacity)
		return false;
	buffer[*length] = (char)ch;
	(*length)++;
	buffer[*length] = '\0';
	return true;
}

static bool handle_key(struct ui_state *state, unsigned char ch)
{
	char *target = NULL;
	size_t *target_len = NULL;

	if (ch == '\t') {
		state->focus = (enum focus_target)((state->focus + 1) % FOCUS_COUNT);
		return true;
	}

	if (ch == 27) {
		state->done = true;
		state->exit_code = 1;
		return true;
	}

	if (ch == '\r' || ch == '\n') {
		if (state->focus == FOCUS_USER)
			state->focus = FOCUS_PASSWORD;
		else if (state->focus == FOCUS_PASSWORD)
			state->focus = FOCUS_CONTINUE;
		else if (state->focus == FOCUS_CONTINUE) {
			state->done = true;
			state->exit_code = 0;
		} else if (state->focus == FOCUS_EXIT) {
			state->done = true;
			state->exit_code = 1;
		}
		return true;
	}

	if (state->focus == FOCUS_USER) {
		target = state->username;
		target_len = &state->username_len;
	} else if (state->focus == FOCUS_PASSWORD) {
		target = state->password;
		target_len = &state->password_len;
	}

	if (!target)
		return false;

	if (ch == 0x7f || ch == 0x08) {
		utf8_trim_last(target, target_len);
		return true;
	}

	if (ch >= 0x20)
		return append_input(target, target_len, INPUT_LIMIT, ch);

	return false;
}

static int terminal_enter_raw(struct terminal_state *terminal)
{
	struct termios raw;

	memset(terminal, 0, sizeof(*terminal));
	if (!isatty(STDIN_FILENO))
		return 0;
	if (tcgetattr(STDIN_FILENO, &terminal->old_termios))
		return -1;

	raw = terminal->old_termios;
	raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &raw))
		return -1;
	terminal->active = true;
	return 0;
}

static void terminal_restore(struct terminal_state *terminal)
{
	if (terminal->active)
		tcsetattr(STDIN_FILENO, TCSANOW, &terminal->old_termios);
}

static int next_back_buffer(const struct display *display)
{
	if (display->front_buffer < 0)
		return 0;
	return 1 - display->front_buffer;
}

static int run_ui(struct display *display)
{
	struct ui_state state = {
		.focus = FOCUS_USER,
	};
	int buffer_index = next_back_buffer(display);

	render_ui(display, buffer_index, &state);
	if (display_present(display, buffer_index))
		return 1;

	while (!state.done && !g_stop_requested) {
		fd_set fds;
		int ret;
		bool dirty = false;

		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, NULL);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "input select failed: %s\n", strerror(errno));
			return 1;
		}

		if (FD_ISSET(STDIN_FILENO, &fds)) {
			unsigned char input[64];
			ssize_t count = read(STDIN_FILENO, input, sizeof(input));

			if (count < 0) {
				if (errno == EINTR || errno == EAGAIN)
					continue;
				fprintf(stderr, "input read failed: %s\n", strerror(errno));
				return 1;
			}
			if (count == 0) {
				state.done = true;
				state.exit_code = 1;
				continue;
			}

			for (ssize_t i = 0; i < count; i++)
				dirty = handle_key(&state, input[i]) || dirty;
		}

		if (dirty && !state.done) {
			buffer_index = next_back_buffer(display);
			render_ui(display, buffer_index, &state);
			if (display_present(display, buffer_index))
				return 1;
		}
	}

	return state.done ? state.exit_code : 1;
}

static void usage(const char *program)
{
	fprintf(stderr,
		"Usage: %s [-D device]\n"
		"\n"
		"Options:\n"
		"  -D device   DRM device, default " DEFAULT_DRM_DEVICE "\n"
		"              Text uses a built-in bitmap glyph atlas\n"
		"  -h          show this help\n",
		program);
}

int main(int argc, char **argv)
{
	const char *device = DEFAULT_DRM_DEVICE;
	struct display display;
	struct terminal_state terminal;
	int opt;
	int ret;

	while ((opt = getopt(argc, argv, "D:h")) != -1) {
		switch (opt) {
		case 'D':
			device = optarg;
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	if (display_init(&display, device))
		return 1;

	if (terminal_enter_raw(&terminal))
		fprintf(stderr, "warning: failed to set raw terminal input\n");

	ret = run_ui(&display);

	terminal_restore(&terminal);
	display_destroy(&display);
	return ret;
}

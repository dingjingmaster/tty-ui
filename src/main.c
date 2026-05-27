#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#ifndef EGL_PLATFORM_GBM_KHR
#define EGL_PLATFORM_GBM_KHR 0x31D7
#endif

#ifndef DRM_FORMAT_XRGB8888
#define DRM_FORMAT_XRGB8888 0x34325258
#endif

#define DEFAULT_DRM_DEVICE "/dev/dri/card0"
#define INPUT_LIMIT 128

extern const unsigned char _binary_fonts_wqy_microhei_ttc_start[];
extern const unsigned char _binary_fonts_wqy_microhei_ttc_end[];

struct color {
	float r;
	float g;
	float b;
	float a;
};

struct drm_fb {
	struct gbm_bo *bo;
	uint32_t fb_id;
};

struct drm_state {
	int fd;
	uint32_t connector_id;
	uint32_t crtc_id;
	drmModeModeInfo mode;
	drmModeCrtc *old_crtc;
};

struct gbm_state {
	struct gbm_device *device;
	struct gbm_surface *surface;
	int width;
	int height;
	uint32_t format;
};

struct egl_state {
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface surface;
};

struct shape_renderer {
	GLuint program;
	GLuint vbo;
	GLint pos_loc;
	GLint screen_loc;
	GLint color_loc;
};

struct text_renderer {
	FT_Library library;
	FT_Face face;
	GLuint program;
	GLuint vbo;
	GLint pos_loc;
	GLint uv_loc;
	GLint screen_loc;
	GLint color_loc;
	GLint tex_loc;
};

struct display {
	struct drm_state drm;
	struct gbm_state gbm;
	struct egl_state egl;
	struct shape_renderer shapes;
	struct text_renderer text;
	struct gbm_bo *front_bo;
	bool mode_set;
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

static void gl_log_shader(GLuint shader, const char *label)
{
	char log[1024];
	GLsizei length = 0;

	glGetShaderInfoLog(shader, sizeof(log), &length, log);
	fprintf(stderr, "%s shader error: %.*s\n", label, (int)length, log);
}

static void gl_log_program(GLuint program)
{
	char log[1024];
	GLsizei length = 0;

	glGetProgramInfoLog(program, sizeof(log), &length, log);
	fprintf(stderr, "program link error: %.*s\n", (int)length, log);
}

static GLuint compile_shader(GLenum type, const char *source, const char *label)
{
	GLuint shader = glCreateShader(type);
	GLint ok = 0;

	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		gl_log_shader(shader, label);
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

static GLuint create_program(const char *vertex_source, const char *fragment_source)
{
	GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source, "vertex");
	GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source, "fragment");
	GLuint program;
	GLint ok = 0;

	if (!vertex || !fragment) {
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		return 0;
	}

	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);

	glGetProgramiv(program, GL_LINK_STATUS, &ok);
	if (!ok) {
		gl_log_program(program);
		glDeleteProgram(program);
		return 0;
	}

	return program;
}

static int init_shapes(struct shape_renderer *renderer)
{
	static const char *vertex_source =
		"attribute vec2 a_pos;\n"
		"uniform vec2 u_screen;\n"
		"void main() {\n"
		"  vec2 p = (a_pos / u_screen) * 2.0 - 1.0;\n"
		"  gl_Position = vec4(p.x, -p.y, 0.0, 1.0);\n"
		"}\n";
	static const char *fragment_source =
		"precision mediump float;\n"
		"uniform vec4 u_color;\n"
		"void main() {\n"
		"  gl_FragColor = u_color;\n"
		"}\n";

	memset(renderer, 0, sizeof(*renderer));
	renderer->program = create_program(vertex_source, fragment_source);
	if (!renderer->program)
		return -1;

	renderer->pos_loc = glGetAttribLocation(renderer->program, "a_pos");
	renderer->screen_loc = glGetUniformLocation(renderer->program, "u_screen");
	renderer->color_loc = glGetUniformLocation(renderer->program, "u_color");
	glGenBuffers(1, &renderer->vbo);
	return 0;
}

static int init_text_renderer(struct text_renderer *renderer)
{
	static const char *vertex_source =
		"attribute vec2 a_pos;\n"
		"attribute vec2 a_uv;\n"
		"uniform vec2 u_screen;\n"
		"varying vec2 v_uv;\n"
		"void main() {\n"
		"  vec2 p = (a_pos / u_screen) * 2.0 - 1.0;\n"
		"  gl_Position = vec4(p.x, -p.y, 0.0, 1.0);\n"
		"  v_uv = a_uv;\n"
		"}\n";
	static const char *fragment_source =
		"precision mediump float;\n"
		"uniform sampler2D u_tex;\n"
		"uniform vec4 u_color;\n"
		"varying vec2 v_uv;\n"
		"void main() {\n"
		"  float alpha = texture2D(u_tex, v_uv).a;\n"
		"  gl_FragColor = vec4(u_color.rgb, u_color.a * alpha);\n"
		"}\n";

	memset(renderer, 0, sizeof(*renderer));
	if (FT_Init_FreeType(&renderer->library)) {
		fprintf(stderr, "failed to initialize freetype\n");
		return -1;
	}

	if (FT_New_Memory_Face(renderer->library,
			       (const FT_Byte *)_binary_fonts_wqy_microhei_ttc_start,
			       (FT_Long)(_binary_fonts_wqy_microhei_ttc_end -
					 _binary_fonts_wqy_microhei_ttc_start),
			       0, &renderer->face)) {
		fprintf(stderr, "failed to load embedded font\n");
		return -1;
	}

	FT_Select_Charmap(renderer->face, FT_ENCODING_UNICODE);

	renderer->program = create_program(vertex_source, fragment_source);
	if (!renderer->program)
		return -1;

	renderer->pos_loc = glGetAttribLocation(renderer->program, "a_pos");
	renderer->uv_loc = glGetAttribLocation(renderer->program, "a_uv");
	renderer->screen_loc = glGetUniformLocation(renderer->program, "u_screen");
	renderer->color_loc = glGetUniformLocation(renderer->program, "u_color");
	renderer->tex_loc = glGetUniformLocation(renderer->program, "u_tex");
	glGenBuffers(1, &renderer->vbo);
	return 0;
}

static void destroy_text_renderer(struct text_renderer *renderer)
{
	if (renderer->vbo)
		glDeleteBuffers(1, &renderer->vbo);
	if (renderer->program)
		glDeleteProgram(renderer->program);
	if (renderer->face)
		FT_Done_Face(renderer->face);
	if (renderer->library)
		FT_Done_FreeType(renderer->library);
}

static void destroy_shapes(struct shape_renderer *renderer)
{
	if (renderer->vbo)
		glDeleteBuffers(1, &renderer->vbo);
	if (renderer->program)
		glDeleteProgram(renderer->program);
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

static int load_glyph(FT_Face face, uint32_t codepoint)
{
	if (!FT_Load_Char(face, (FT_ULong)codepoint, FT_LOAD_RENDER))
		return 0;
	return FT_Load_Char(face, '?', FT_LOAD_RENDER);
}

static int measure_text(struct text_renderer *renderer, const char *text,
			int pixel_size, int *width, int *height)
{
	const char *cursor = text;
	int pen_x = 0;
	int min_x = 0;
	int max_x = 0;
	int min_y = 0;
	int max_y = 0;

	if (FT_Set_Pixel_Sizes(renderer->face, 0, (FT_UInt)pixel_size))
		return -1;

	while (*cursor) {
		uint32_t codepoint = utf8_next(&cursor);
		FT_GlyphSlot glyph;
		int gx;
		int gy;

		if (load_glyph(renderer->face, codepoint))
			continue;

		glyph = renderer->face->glyph;
		gx = pen_x + glyph->bitmap_left;
		gy = -glyph->bitmap_top;

		if (gx < min_x)
			min_x = gx;
		if (gx + (int)glyph->bitmap.width > max_x)
			max_x = gx + (int)glyph->bitmap.width;
		if (gy < min_y)
			min_y = gy;
		if (gy + (int)glyph->bitmap.rows > max_y)
			max_y = gy + (int)glyph->bitmap.rows;

		pen_x += (int)(glyph->advance.x >> 6);
		if (pen_x > max_x)
			max_x = pen_x;
	}

	*width = max_x - min_x;
	*height = max_y - min_y;
	if (*width <= 0)
		*width = 1;
	if (*height <= 0)
		*height = pixel_size;
	return 0;
}

static int render_text_bitmap(struct text_renderer *renderer, const char *text,
			      int pixel_size, unsigned char **pixels,
			      int *width, int *height)
{
	const char *cursor;
	int pen_x;
	int min_x = 0;
	int max_x = 0;
	int min_y = 0;
	int max_y = 0;
	int origin_x;
	int baseline;

	if (measure_text(renderer, text, pixel_size, width, height))
		return -1;

	if (FT_Set_Pixel_Sizes(renderer->face, 0, (FT_UInt)pixel_size))
		return -1;

	cursor = text;
	pen_x = 0;
	while (*cursor) {
		uint32_t codepoint = utf8_next(&cursor);
		FT_GlyphSlot glyph;
		int gx;
		int gy;

		if (load_glyph(renderer->face, codepoint))
			continue;

		glyph = renderer->face->glyph;
		gx = pen_x + glyph->bitmap_left;
		gy = -glyph->bitmap_top;

		if (gx < min_x)
			min_x = gx;
		if (gx + (int)glyph->bitmap.width > max_x)
			max_x = gx + (int)glyph->bitmap.width;
		if (gy < min_y)
			min_y = gy;
		if (gy + (int)glyph->bitmap.rows > max_y)
			max_y = gy + (int)glyph->bitmap.rows;

		pen_x += (int)(glyph->advance.x >> 6);
		if (pen_x > max_x)
			max_x = pen_x;
	}

	origin_x = -min_x;
	baseline = -min_y;
	*pixels = calloc((size_t)(*width) * (size_t)(*height), 1);
	if (!*pixels)
		return -1;

	cursor = text;
	pen_x = 0;
	while (*cursor) {
		uint32_t codepoint = utf8_next(&cursor);
		FT_GlyphSlot glyph;
		FT_Bitmap *bitmap;
		int dst_x;
		int dst_y;

		if (load_glyph(renderer->face, codepoint))
			continue;

		glyph = renderer->face->glyph;
		bitmap = &glyph->bitmap;
		dst_x = origin_x + pen_x + glyph->bitmap_left;
		dst_y = baseline - glyph->bitmap_top;

		if (bitmap->pixel_mode == FT_PIXEL_MODE_GRAY) {
			for (unsigned int y = 0; y < bitmap->rows; y++) {
				const unsigned char *src_row;
				if (bitmap->pitch >= 0)
					src_row = bitmap->buffer + y * (unsigned int)bitmap->pitch;
				else
					src_row = bitmap->buffer +
						(bitmap->rows - 1 - y) *
						(unsigned int)(-bitmap->pitch);

				for (unsigned int x = 0; x < bitmap->width; x++) {
					int px = dst_x + (int)x;
					int py = dst_y + (int)y;
					size_t dst_index;

					if (px < 0 || py < 0 || px >= *width || py >= *height)
						continue;

					dst_index = (size_t)py * (size_t)(*width) + (size_t)px;
					if (src_row[x] > (*pixels)[dst_index])
						(*pixels)[dst_index] = src_row[x];
				}
			}
		}

		pen_x += (int)(glyph->advance.x >> 6);
	}

	return 0;
}

static void draw_rect(struct display *display, float x, float y, float width,
		      float height, struct color color)
{
	struct shape_renderer *renderer = &display->shapes;
	GLfloat vertices[] = {
		x, y,
		x + width, y,
		x, y + height,
		x + width, y,
		x + width, y + height,
		x, y + height,
	};

	glUseProgram(renderer->program);
	glUniform2f(renderer->screen_loc, (float)display->gbm.width,
		    (float)display->gbm.height);
	glUniform4f(renderer->color_loc, color.r, color.g, color.b, color.a);
	glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
	glEnableVertexAttribArray((GLuint)renderer->pos_loc);
	glVertexAttribPointer((GLuint)renderer->pos_loc, 2, GL_FLOAT, GL_FALSE, 0,
			      (const GLvoid *)(uintptr_t)0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void draw_rect_outline(struct display *display, float x, float y,
			      float width, float height, float thickness,
			      struct color color)
{
	draw_rect(display, x, y, width, thickness, color);
	draw_rect(display, x, y + height - thickness, width, thickness, color);
	draw_rect(display, x, y, thickness, height, color);
	draw_rect(display, x + width - thickness, y, thickness, height, color);
}

static int draw_text(struct display *display, float x, float y, const char *text,
		     int pixel_size, struct color color)
{
	struct text_renderer *renderer = &display->text;
	unsigned char *pixels = NULL;
	GLuint texture = 0;
	int width = 0;
	int height = 0;
	GLfloat vertices[24];

	if (render_text_bitmap(renderer, text, pixel_size, &pixels, &width, &height))
		return -1;

	vertices[0] = x;
	vertices[1] = y;
	vertices[2] = 0.0f;
	vertices[3] = 0.0f;
	vertices[4] = x + (float)width;
	vertices[5] = y;
	vertices[6] = 1.0f;
	vertices[7] = 0.0f;
	vertices[8] = x;
	vertices[9] = y + (float)height;
	vertices[10] = 0.0f;
	vertices[11] = 1.0f;
	vertices[12] = x + (float)width;
	vertices[13] = y;
	vertices[14] = 1.0f;
	vertices[15] = 0.0f;
	vertices[16] = x + (float)width;
	vertices[17] = y + (float)height;
	vertices[18] = 1.0f;
	vertices[19] = 1.0f;
	vertices[20] = x;
	vertices[21] = y + (float)height;
	vertices[22] = 0.0f;
	vertices[23] = 1.0f;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, width, height, 0, GL_ALPHA,
		     GL_UNSIGNED_BYTE, pixels);

	glUseProgram(renderer->program);
	glUniform2f(renderer->screen_loc, (float)display->gbm.width,
		    (float)display->gbm.height);
	glUniform4f(renderer->color_loc, color.r, color.g, color.b, color.a);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform1i(renderer->tex_loc, 0);

	glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
	glEnableVertexAttribArray((GLuint)renderer->pos_loc);
	glVertexAttribPointer((GLuint)renderer->pos_loc, 2, GL_FLOAT, GL_FALSE,
			      4 * sizeof(GLfloat), (const GLvoid *)(uintptr_t)0);
	glEnableVertexAttribArray((GLuint)renderer->uv_loc);
	glVertexAttribPointer((GLuint)renderer->uv_loc, 2, GL_FLOAT, GL_FALSE,
			      4 * sizeof(GLfloat),
			      (const GLvoid *)(uintptr_t)(2 * sizeof(GLfloat)));
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDeleteTextures(1, &texture);
	free(pixels);
	return 0;
}

static int draw_text_centered(struct display *display, const char *text,
			      int pixel_size, float center_x, float y,
			      struct color color)
{
	int width = 0;
	int height = 0;

	(void)height;
	if (measure_text(&display->text, text, pixel_size, &width, &height))
		return -1;

	return draw_text(display, center_x - (float)width / 2.0f, y, text,
			 pixel_size, color);
}

static void drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
	struct drm_fb *fb = data;
	int fd = gbm_device_get_fd(gbm_bo_get_device(bo));

	if (fb->fb_id)
		drmModeRmFB(fd, fb->fb_id);
	free(fb);
}

static struct drm_fb *drm_fb_get_from_bo(struct gbm_bo *bo)
{
	struct drm_fb *fb = gbm_bo_get_user_data(bo);
	int fd = gbm_device_get_fd(gbm_bo_get_device(bo));
	uint32_t width = gbm_bo_get_width(bo);
	uint32_t height = gbm_bo_get_height(bo);
	uint32_t format = gbm_bo_get_format(bo);
	uint32_t handles[4] = { gbm_bo_get_handle(bo).u32, 0, 0, 0 };
	uint32_t strides[4] = { gbm_bo_get_stride(bo), 0, 0, 0 };
	uint32_t offsets[4] = { 0, 0, 0, 0 };
	int ret;

	if (fb)
		return fb;

	fb = calloc(1, sizeof(*fb));
	if (!fb)
		return NULL;

	fb->bo = bo;
	ret = drmModeAddFB2(fd, width, height, format, handles, strides, offsets,
			    &fb->fb_id, 0);
	if (ret) {
		ret = drmModeAddFB(fd, width, height, 24, 32, strides[0],
				   handles[0], &fb->fb_id);
	}
	if (ret) {
		fprintf(stderr, "failed to create drm framebuffer: %s\n",
			strerror(errno));
		free(fb);
		return NULL;
	}

	gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);
	return fb;
}

static uint32_t find_crtc_for_encoder(const drmModeRes *resources,
				      const drmModeEncoder *encoder)
{
	for (int i = 0; i < resources->count_crtcs; i++) {
		if (encoder->possible_crtcs & (1u << i))
			return resources->crtcs[i];
	}
	return 0;
}

static uint32_t find_crtc_for_connector(int fd, const drmModeRes *resources,
					const drmModeConnector *connector)
{
	for (int i = 0; i < connector->count_encoders; i++) {
		drmModeEncoder *encoder = drmModeGetEncoder(fd, connector->encoders[i]);
		uint32_t crtc_id = 0;

		if (!encoder)
			continue;
		crtc_id = find_crtc_for_encoder(resources, encoder);
		drmModeFreeEncoder(encoder);
		if (crtc_id)
			return crtc_id;
	}
	return 0;
}

static int init_drm(struct drm_state *drm, const char *device)
{
	drmModeRes *resources = NULL;
	drmModeConnector *connector = NULL;
	drmModeEncoder *encoder = NULL;
	int chosen_mode = -1;
	int largest_mode = -1;
	int largest_area = 0;
	int ret = -1;

	memset(drm, 0, sizeof(*drm));
	drm->fd = -1;

	drm->fd = open(device, O_RDWR | O_CLOEXEC);
	if (drm->fd < 0) {
		fprintf(stderr, "could not open drm device %s: %s\n", device,
			strerror(errno));
		return -1;
	}

	resources = drmModeGetResources(drm->fd);
	if (!resources) {
		fprintf(stderr, "drmModeGetResources failed: %s\n", strerror(errno));
		goto out;
	}

	for (int i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(drm->fd, resources->connectors[i]);
		if (!connector)
			continue;
		if (connector->connection == DRM_MODE_CONNECTED &&
		    connector->count_modes > 0)
			break;
		drmModeFreeConnector(connector);
		connector = NULL;
	}

	if (!connector) {
		fprintf(stderr, "no connected drm connector found\n");
		goto out;
	}

	for (int i = 0; i < connector->count_modes; i++) {
		int area = connector->modes[i].hdisplay * connector->modes[i].vdisplay;

		if ((connector->modes[i].type & DRM_MODE_TYPE_PREFERRED) &&
		    chosen_mode < 0) {
			chosen_mode = i;
		}
		if (area > largest_area) {
			largest_area = area;
			largest_mode = i;
		}
	}
	if (chosen_mode < 0)
		chosen_mode = largest_mode;
	if (chosen_mode < 0)
		goto out;

	drm->mode = connector->modes[chosen_mode];
	drm->connector_id = connector->connector_id;

	if (connector->encoder_id)
		encoder = drmModeGetEncoder(drm->fd, connector->encoder_id);
	if (encoder && encoder->crtc_id)
		drm->crtc_id = encoder->crtc_id;
	else
		drm->crtc_id = find_crtc_for_connector(drm->fd, resources, connector);

	if (!drm->crtc_id) {
		fprintf(stderr, "no usable crtc found\n");
		goto out;
	}

	drm->old_crtc = drmModeGetCrtc(drm->fd, drm->crtc_id);
	ret = 0;

out:
	if (encoder)
		drmModeFreeEncoder(encoder);
	if (connector)
		drmModeFreeConnector(connector);
	if (resources)
		drmModeFreeResources(resources);
	if (ret && drm->fd >= 0) {
		close(drm->fd);
		drm->fd = -1;
	}
	return ret;
}

static int init_gbm(struct gbm_state *gbm, const struct drm_state *drm)
{
	memset(gbm, 0, sizeof(*gbm));
	gbm->width = drm->mode.hdisplay;
	gbm->height = drm->mode.vdisplay;
	gbm->format = DRM_FORMAT_XRGB8888;
	gbm->device = gbm_create_device(drm->fd);
	if (!gbm->device) {
		fprintf(stderr, "failed to create gbm device\n");
		return -1;
	}

	gbm->surface = gbm_surface_create(gbm->device, (uint32_t)gbm->width,
					  (uint32_t)gbm->height, gbm->format,
					  GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (!gbm->surface) {
		fprintf(stderr, "failed to create gbm surface\n");
		return -1;
	}

	return 0;
}

static bool match_config(EGLDisplay display, EGLConfig *configs, EGLint count,
			 EGLint visual_id, EGLConfig *out)
{
	for (EGLint i = 0; i < count; i++) {
		EGLint id = 0;

		if (!eglGetConfigAttrib(display, configs[i], EGL_NATIVE_VISUAL_ID, &id))
			continue;
		if (id == visual_id) {
			*out = configs[i];
			return true;
		}
	}
	return false;
}

static bool choose_egl_config(EGLDisplay display, EGLint visual_id, EGLConfig *out)
{
	const EGLint attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE,
	};
	EGLint count = 0;
	EGLint matched = 0;
	EGLConfig *configs;
	bool ok = false;

	if (!eglGetConfigs(display, NULL, 0, &count) || count <= 0)
		return false;

	configs = calloc((size_t)count, sizeof(*configs));
	if (!configs)
		return false;

	if (eglChooseConfig(display, attribs, configs, count, &matched) && matched) {
		ok = match_config(display, configs, matched, visual_id, out);
		if (!ok) {
			*out = configs[0];
			ok = true;
		}
	}

	free(configs);
	return ok;
}

static int init_egl(struct egl_state *egl, const struct gbm_state *gbm)
{
	PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display;
	const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE,
	};
	EGLint major = 0;
	EGLint minor = 0;

	memset(egl, 0, sizeof(*egl));
	egl->display = EGL_NO_DISPLAY;
	egl->surface = EGL_NO_SURFACE;
	egl->context = EGL_NO_CONTEXT;

	get_platform_display =
		(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
	if (get_platform_display)
		egl->display = get_platform_display(EGL_PLATFORM_GBM_KHR,
						    gbm->device, NULL);
	else
		egl->display = eglGetDisplay((EGLNativeDisplayType)gbm->device);

	if (egl->display == EGL_NO_DISPLAY ||
	    !eglInitialize(egl->display, &major, &minor)) {
		fprintf(stderr, "failed to initialize egl\n");
		return -1;
	}

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		fprintf(stderr, "failed to bind OpenGL ES api\n");
		return -1;
	}

	if (!choose_egl_config(egl->display, (EGLint)gbm->format, &egl->config)) {
		fprintf(stderr, "failed to choose egl config\n");
		return -1;
	}

	egl->context = eglCreateContext(egl->display, egl->config, EGL_NO_CONTEXT,
					context_attribs);
	if (egl->context == EGL_NO_CONTEXT) {
		fprintf(stderr, "failed to create egl context\n");
		return -1;
	}

	egl->surface = eglCreateWindowSurface(egl->display, egl->config,
					      (EGLNativeWindowType)gbm->surface,
					      NULL);
	if (egl->surface == EGL_NO_SURFACE) {
		fprintf(stderr, "failed to create egl window surface\n");
		return -1;
	}

	if (!eglMakeCurrent(egl->display, egl->surface, egl->surface,
			    egl->context)) {
		fprintf(stderr, "failed to make egl context current\n");
		return -1;
	}

	printf("Using EGL %d.%d, GLES renderer: %s\n", major, minor,
	       glGetString(GL_RENDERER));
	return 0;
}

static void display_destroy(struct display *display);

static int display_init(struct display *display, const char *device)
{
	memset(display, 0, sizeof(*display));
	display->drm.fd = -1;

	if (init_drm(&display->drm, device))
		return -1;
	if (init_gbm(&display->gbm, &display->drm))
		goto fail;
	if (init_egl(&display->egl, &display->gbm))
		goto fail;
	if (init_shapes(&display->shapes))
		goto fail;
	if (init_text_renderer(&display->text))
		goto fail;

	glViewport(0, 0, display->gbm.width, display->gbm.height);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	return 0;

fail:
	display_destroy(display);
	return -1;
}

static void display_destroy(struct display *display)
{
	if (display->mode_set && display->drm.fd >= 0 && display->drm.old_crtc) {
		drmModeSetCrtc(display->drm.fd, display->drm.old_crtc->crtc_id,
			       display->drm.old_crtc->buffer_id,
			       display->drm.old_crtc->x, display->drm.old_crtc->y,
			       &display->drm.connector_id, 1,
			       &display->drm.old_crtc->mode);
	}

	if (display->front_bo && display->gbm.surface)
		gbm_surface_release_buffer(display->gbm.surface, display->front_bo);

	destroy_text_renderer(&display->text);
	destroy_shapes(&display->shapes);

	if (display->egl.display != EGL_NO_DISPLAY) {
		eglMakeCurrent(display->egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE,
			       EGL_NO_CONTEXT);
		if (display->egl.surface != EGL_NO_SURFACE)
			eglDestroySurface(display->egl.display, display->egl.surface);
		if (display->egl.context != EGL_NO_CONTEXT)
			eglDestroyContext(display->egl.display, display->egl.context);
		eglTerminate(display->egl.display);
	}

	if (display->gbm.surface)
		gbm_surface_destroy(display->gbm.surface);
	if (display->gbm.device)
		gbm_device_destroy(display->gbm.device);
	if (display->drm.old_crtc)
		drmModeFreeCrtc(display->drm.old_crtc);
	if (display->drm.fd >= 0)
		close(display->drm.fd);
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
	drmEventContext event_context = {
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
			drmHandleEvent(fd, &event_context);
	}

	return g_stop_requested ? -1 : 0;
}

static int display_present(struct display *display)
{
	struct gbm_bo *next_bo;
	struct drm_fb *fb;
	int ret;

	if (!eglSwapBuffers(display->egl.display, display->egl.surface)) {
		fprintf(stderr, "eglSwapBuffers failed\n");
		return -1;
	}

	next_bo = gbm_surface_lock_front_buffer(display->gbm.surface);
	if (!next_bo) {
		fprintf(stderr, "failed to lock gbm front buffer\n");
		return -1;
	}

	fb = drm_fb_get_from_bo(next_bo);
	if (!fb) {
		gbm_surface_release_buffer(display->gbm.surface, next_bo);
		return -1;
	}

	if (!display->mode_set) {
		ret = drmModeSetCrtc(display->drm.fd, display->drm.crtc_id,
				     fb->fb_id, 0, 0, &display->drm.connector_id,
				     1, &display->drm.mode);
		if (ret) {
			fprintf(stderr, "drmModeSetCrtc failed: %s\n",
				strerror(errno));
			gbm_surface_release_buffer(display->gbm.surface, next_bo);
			return -1;
		}
		display->mode_set = true;
	} else {
		bool waiting = true;

		ret = drmModePageFlip(display->drm.fd, display->drm.crtc_id,
				      fb->fb_id, DRM_MODE_PAGE_FLIP_EVENT,
				      &waiting);
		if (ret) {
			fprintf(stderr, "drmModePageFlip failed: %s\n",
				strerror(errno));
			gbm_surface_release_buffer(display->gbm.surface, next_bo);
			return -1;
		}
		if (wait_for_page_flip(display->drm.fd, &waiting)) {
			gbm_surface_release_buffer(display->gbm.surface, next_bo);
			return -1;
		}
		if (display->front_bo)
			gbm_surface_release_buffer(display->gbm.surface,
						   display->front_bo);
	}

	display->front_bo = next_bo;
	return 0;
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

static void draw_input(struct display *display, float x, float y, float width,
		       float height, const char *text, bool focused)
{
	struct color input_bg = { 0.075f, 0.09f, 0.105f, 1.0f };
	struct color border = focused ? (struct color){ 0.30f, 0.74f, 0.82f, 1.0f }
				      : (struct color){ 0.34f, 0.39f, 0.43f, 1.0f };
	struct color text_color = { 0.93f, 0.96f, 0.96f, 1.0f };
	int text_width = 0;
	int text_height = 0;
	float text_x = x + 14.0f;
	float text_y;

	draw_rect(display, x, y, width, height, input_bg);
	draw_rect_outline(display, x, y, width, height, focused ? 3.0f : 2.0f,
			  border);

	measure_text(&display->text, text, 22, &text_width, &text_height);
	text_y = y + (height - (float)text_height) / 2.0f;
	if (text[0])
		draw_text(display, text_x, text_y, text, 22, text_color);
	if (focused) {
		float cursor_x = text_x + (float)text_width + 3.0f;
		draw_rect(display, cursor_x, y + 9.0f, 2.0f, height - 18.0f,
			  (struct color){ 0.30f, 0.74f, 0.82f, 1.0f });
	}
}

static void draw_button(struct display *display, float x, float y, float width,
			float height, const char *label, bool focused)
{
	struct color bg = focused ? (struct color){ 0.13f, 0.34f, 0.39f, 1.0f }
				  : (struct color){ 0.12f, 0.15f, 0.17f, 1.0f };
	struct color border = focused ? (struct color){ 0.30f, 0.74f, 0.82f, 1.0f }
				      : (struct color){ 0.37f, 0.43f, 0.46f, 1.0f };
	struct color fg = { 0.95f, 0.98f, 0.98f, 1.0f };
	int text_width = 0;
	int text_height = 0;

	draw_rect(display, x, y, width, height, bg);
	draw_rect_outline(display, x, y, width, height, focused ? 3.0f : 2.0f,
			  border);
	measure_text(&display->text, label, 22, &text_width, &text_height);
	draw_text(display, x + (width - (float)text_width) / 2.0f,
		  y + (height - (float)text_height) / 2.0f, label, 22, fg);
}

static void render_ui(struct display *display, const struct ui_state *state)
{
	const float w = (float)display->gbm.width;
	const float h = (float)display->gbm.height;
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
	int label_width = 0;
	int label_height = 0;
	int tagline_width = 0;
	int tagline_height = 0;

	struct color bg = { 0.045f, 0.055f, 0.062f, 1.0f };
	struct color panel = { 0.072f, 0.088f, 0.100f, 1.0f };
	struct color line = { 0.34f, 0.40f, 0.43f, 1.0f };
	struct color primary = { 0.92f, 0.96f, 0.96f, 1.0f };
	struct color muted = { 0.67f, 0.75f, 0.76f, 1.0f };
	struct color tagline = { 0.70f, 0.88f, 0.67f, 1.0f };

	glViewport(0, 0, display->gbm.width, display->gbm.height);
	glClearColor(bg.r, bg.g, bg.b, bg.a);
	glClear(GL_COLOR_BUFFER_BIT);

	draw_rect_outline(display, margin, margin, w - margin * 2.0f,
			  h - margin * 2.0f, 2.0f, line);
	draw_rect(display, margin + 2.0f, margin + 2.0f,
		  w - margin * 2.0f - 4.0f, header_h - 2.0f, panel);
	draw_rect(display, margin, margin + header_h, w - margin * 2.0f,
		  2.0f, line);
	draw_text(display, margin + 34.0f, margin + 27.0f, "安得合众", 28,
		  primary);

	draw_text_centered(display, "DiskCrypt登录", 34, center_x, title_y,
			   primary);
	draw_text_centered(display, "用 Tab 切换区域，Enter键确认", 22, center_x,
			   title_y + 58.0f, muted);

	measure_text(&display->text, "用户名称:", 22, &label_width, &label_height);
	draw_text(display, form_x + label_w - (float)label_width,
		  user_y + (row_h - (float)label_height) / 2.0f,
		  "用户名称:", 22, primary);
	draw_input(display, input_x, user_y, input_w, row_h, state->username,
		   state->focus == FOCUS_USER);

	make_password_mask(state, mask, sizeof(mask));
	measure_text(&display->text, "用户密码:", 22, &label_width, &label_height);
	draw_text(display, form_x + label_w - (float)label_width,
		  pass_y + (row_h - (float)label_height) / 2.0f,
		  "用户密码:", 22, primary);
	draw_input(display, input_x, pass_y, input_w, row_h, mask,
		   state->focus == FOCUS_PASSWORD);

	draw_button(display, continue_button_x, button_y, button_w, button_h,
		    "继续启动", state->focus == FOCUS_CONTINUE);
	draw_button(display, exit_button_x, button_y, button_w,
		    button_h, "退出", state->focus == FOCUS_EXIT);

	draw_rect(display, margin + 1.0f, h - margin - 78.0f,
		  w - margin * 2.0f - 2.0f, 1.0f,
		  (struct color){ 0.18f, 0.22f, 0.23f, 1.0f });
	measure_text(&display->text, "全盘加密  安全无忧", 24, &tagline_width,
		     &tagline_height);
	draw_text(display, w - margin - 32.0f - (float)tagline_width,
		  h - margin - 48.0f - (float)tagline_height,
		  "全盘加密  安全无忧", 24, tagline);
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

static int run_ui(struct display *display)
{
	struct ui_state state = {
		.focus = FOCUS_USER,
	};

	render_ui(display, &state);
	if (display_present(display))
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
			render_ui(display, &state);
			if (display_present(display))
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
		"              Font is embedded from fonts/wqy-microhei.ttc\n"
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

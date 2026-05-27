/*
 * Minimal DRM/KMS wrapper used by tty-ui.
 *
 * Portions of the DRM UAPI layouts and ioctl numbers below are derived from
 * libdrm's include/drm/drm.h and include/drm/drm_mode.h.
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * Copyright (c) 2007 Dave Airlie <airlied@linux.ie>
 * Copyright (c) 2007 Jakob Bornecrantz <wallbraker@gmail.com>
 * Copyright (c) 2008 Red Hat Inc.
 * Copyright (c) 2007-2008 Tungsten Graphics, Inc., Cedar Park, TX., USA
 * Copyright (c) 2007-2008 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "kms_drm.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DRM_IOCTL_BASE 'd'
#define DRM_IOWR(nr, type) _IOWR(DRM_IOCTL_BASE, nr, type)
#define DRM_DISPLAY_MODE_LEN 32

#define DRM_IOCTL_MODE_GETRESOURCES DRM_IOWR(0xA0, struct drm_mode_card_res)
#define DRM_IOCTL_MODE_GETCRTC DRM_IOWR(0xA1, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_SETCRTC DRM_IOWR(0xA2, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_GETENCODER DRM_IOWR(0xA6, struct drm_mode_get_encoder)
#define DRM_IOCTL_MODE_GETCONNECTOR DRM_IOWR(0xA7, struct drm_mode_get_connector)
#define DRM_IOCTL_MODE_ADDFB DRM_IOWR(0xAE, struct drm_mode_fb_cmd)
#define DRM_IOCTL_MODE_RMFB DRM_IOWR(0xAF, unsigned int)
#define DRM_IOCTL_MODE_PAGE_FLIP DRM_IOWR(0xB0, struct drm_mode_crtc_page_flip)
#define DRM_IOCTL_MODE_CREATE_DUMB DRM_IOWR(0xB2, struct drm_mode_create_dumb)
#define DRM_IOCTL_MODE_MAP_DUMB DRM_IOWR(0xB3, struct drm_mode_map_dumb)
#define DRM_IOCTL_MODE_DESTROY_DUMB DRM_IOWR(0xB4, struct drm_mode_destroy_dumb)

#define DRM_EVENT_FLIP_COMPLETE 0x02

#define PTR_TO_U64(ptr) ((uint64_t)(uintptr_t)(ptr))
#define U64_TO_PTR(value) ((void *)(uintptr_t)(value))

struct drm_mode_modeinfo {
	uint32_t clock;
	uint16_t hdisplay;
	uint16_t hsync_start;
	uint16_t hsync_end;
	uint16_t htotal;
	uint16_t hskew;
	uint16_t vdisplay;
	uint16_t vsync_start;
	uint16_t vsync_end;
	uint16_t vtotal;
	uint16_t vscan;
	uint32_t vrefresh;
	uint32_t flags;
	uint32_t type;
	char name[DRM_DISPLAY_MODE_LEN];
};

struct drm_mode_card_res {
	uint64_t fb_id_ptr;
	uint64_t crtc_id_ptr;
	uint64_t connector_id_ptr;
	uint64_t encoder_id_ptr;
	uint32_t count_fbs;
	uint32_t count_crtcs;
	uint32_t count_connectors;
	uint32_t count_encoders;
	uint32_t min_width;
	uint32_t max_width;
	uint32_t min_height;
	uint32_t max_height;
};

struct drm_mode_crtc {
	uint64_t set_connectors_ptr;
	uint32_t count_connectors;
	uint32_t crtc_id;
	uint32_t fb_id;
	uint32_t x;
	uint32_t y;
	uint32_t gamma_size;
	uint32_t mode_valid;
	struct drm_mode_modeinfo mode;
};

struct drm_mode_get_encoder {
	uint32_t encoder_id;
	uint32_t encoder_type;
	uint32_t crtc_id;
	uint32_t possible_crtcs;
	uint32_t possible_clones;
};

struct drm_mode_get_connector {
	uint64_t encoders_ptr;
	uint64_t modes_ptr;
	uint64_t props_ptr;
	uint64_t prop_values_ptr;
	uint32_t count_modes;
	uint32_t count_props;
	uint32_t count_encoders;
	uint32_t encoder_id;
	uint32_t connector_id;
	uint32_t connector_type;
	uint32_t connector_type_id;
	uint32_t connection;
	uint32_t mm_width;
	uint32_t mm_height;
	uint32_t subpixel;
	uint32_t pad;
};

struct drm_mode_fb_cmd {
	uint32_t fb_id;
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t bpp;
	uint32_t depth;
	uint32_t handle;
};

struct drm_mode_crtc_page_flip {
	uint32_t crtc_id;
	uint32_t fb_id;
	uint32_t flags;
	uint32_t reserved;
	uint64_t user_data;
};

struct drm_mode_create_dumb {
	uint32_t height;
	uint32_t width;
	uint32_t bpp;
	uint32_t flags;
	uint32_t handle;
	uint32_t pitch;
	uint64_t size;
};

struct drm_mode_map_dumb {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
};

struct drm_mode_destroy_dumb {
	uint32_t handle;
};

struct drm_event {
	uint32_t type;
	uint32_t length;
};

struct drm_event_vblank {
	struct drm_event base;
	uint64_t user_data;
	uint32_t tv_sec;
	uint32_t tv_usec;
	uint32_t sequence;
	uint32_t crtc_id;
};

_Static_assert(sizeof(struct kms_mode_info) == sizeof(struct drm_mode_modeinfo),
	       "DRM mode layout mismatch");

static int kms_ioctl(int fd, unsigned long request, void *arg)
{
	int ret;

	do {
		ret = ioctl(fd, request, arg);
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));

	return ret;
}

static void *alloc_array(uint32_t count, size_t size)
{
	if (count == 0)
		return NULL;
	if (size && count > SIZE_MAX / size) {
		errno = ENOMEM;
		return NULL;
	}
	return malloc((size_t)count * size);
}

static void *copy_array(const void *src, uint32_t count, size_t size)
{
	void *dst;

	if (count == 0)
		return NULL;
	dst = alloc_array(count, size);
	if (!dst)
		return NULL;
	memcpy(dst, src, (size_t)count * size);
	return dst;
}

static int count_to_int(uint32_t count)
{
	return count > (uint32_t)INT_MAX ? INT_MAX : (int)count;
}

struct kms_resources *kms_get_resources(int fd)
{
	struct drm_mode_card_res res;
	struct drm_mode_card_res counts;
	struct kms_resources *out = NULL;

	for (;;) {
		memset(&res, 0, sizeof(res));
		if (kms_ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res))
			return NULL;

		counts = res;
		res.fb_id_ptr = PTR_TO_U64(alloc_array(counts.count_fbs,
						       sizeof(uint32_t)));
		res.crtc_id_ptr = PTR_TO_U64(alloc_array(counts.count_crtcs,
							 sizeof(uint32_t)));
		res.connector_id_ptr =
			PTR_TO_U64(alloc_array(counts.count_connectors,
					       sizeof(uint32_t)));
		res.encoder_id_ptr = PTR_TO_U64(alloc_array(counts.count_encoders,
							    sizeof(uint32_t)));
		if ((counts.count_fbs && !res.fb_id_ptr) ||
		    (counts.count_crtcs && !res.crtc_id_ptr) ||
		    (counts.count_connectors && !res.connector_id_ptr) ||
		    (counts.count_encoders && !res.encoder_id_ptr)) {
			free(U64_TO_PTR(res.fb_id_ptr));
			free(U64_TO_PTR(res.crtc_id_ptr));
			free(U64_TO_PTR(res.connector_id_ptr));
			free(U64_TO_PTR(res.encoder_id_ptr));
			return NULL;
		}

		if (kms_ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res)) {
			free(U64_TO_PTR(res.fb_id_ptr));
			free(U64_TO_PTR(res.crtc_id_ptr));
			free(U64_TO_PTR(res.connector_id_ptr));
			free(U64_TO_PTR(res.encoder_id_ptr));
			return NULL;
		}

		if (counts.count_fbs >= res.count_fbs &&
		    counts.count_crtcs >= res.count_crtcs &&
		    counts.count_connectors >= res.count_connectors &&
		    counts.count_encoders >= res.count_encoders)
			break;

		free(U64_TO_PTR(res.fb_id_ptr));
		free(U64_TO_PTR(res.crtc_id_ptr));
		free(U64_TO_PTR(res.connector_id_ptr));
		free(U64_TO_PTR(res.encoder_id_ptr));
	}

	out = calloc(1, sizeof(*out));
	if (!out)
		goto fail;

	out->min_width = res.min_width;
	out->max_width = res.max_width;
	out->min_height = res.min_height;
	out->max_height = res.max_height;
	out->count_fbs = count_to_int(res.count_fbs);
	out->count_crtcs = count_to_int(res.count_crtcs);
	out->count_connectors = count_to_int(res.count_connectors);
	out->count_encoders = count_to_int(res.count_encoders);
	out->fbs = copy_array(U64_TO_PTR(res.fb_id_ptr), res.count_fbs,
			      sizeof(uint32_t));
	out->crtcs = copy_array(U64_TO_PTR(res.crtc_id_ptr), res.count_crtcs,
				sizeof(uint32_t));
	out->connectors = copy_array(U64_TO_PTR(res.connector_id_ptr),
				     res.count_connectors, sizeof(uint32_t));
	out->encoders = copy_array(U64_TO_PTR(res.encoder_id_ptr),
				   res.count_encoders, sizeof(uint32_t));
	if ((res.count_fbs && !out->fbs) ||
	    (res.count_crtcs && !out->crtcs) ||
	    (res.count_connectors && !out->connectors) ||
	    (res.count_encoders && !out->encoders)) {
		kms_free_resources(out);
		out = NULL;
	}

fail:
	free(U64_TO_PTR(res.fb_id_ptr));
	free(U64_TO_PTR(res.crtc_id_ptr));
	free(U64_TO_PTR(res.connector_id_ptr));
	free(U64_TO_PTR(res.encoder_id_ptr));
	return out;
}

void kms_free_resources(struct kms_resources *resources)
{
	if (!resources)
		return;
	free(resources->fbs);
	free(resources->crtcs);
	free(resources->connectors);
	free(resources->encoders);
	free(resources);
}

struct kms_connector *kms_get_connector(int fd, uint32_t connector_id)
{
	struct drm_mode_get_connector conn;
	struct drm_mode_get_connector counts;
	struct kms_connector *out = NULL;

	for (;;) {
		memset(&conn, 0, sizeof(conn));
		conn.connector_id = connector_id;
		if (kms_ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn))
			return NULL;

		counts = conn;
		conn.props_ptr = PTR_TO_U64(alloc_array(counts.count_props,
							sizeof(uint32_t)));
		conn.prop_values_ptr =
			PTR_TO_U64(alloc_array(counts.count_props,
					       sizeof(uint64_t)));
		conn.modes_ptr = PTR_TO_U64(alloc_array(counts.count_modes,
							sizeof(struct drm_mode_modeinfo)));
		conn.encoders_ptr = PTR_TO_U64(alloc_array(counts.count_encoders,
							   sizeof(uint32_t)));
		if ((counts.count_props && !conn.props_ptr) ||
		    (counts.count_props && !conn.prop_values_ptr) ||
		    (counts.count_modes && !conn.modes_ptr) ||
		    (counts.count_encoders && !conn.encoders_ptr)) {
			free(U64_TO_PTR(conn.props_ptr));
			free(U64_TO_PTR(conn.prop_values_ptr));
			free(U64_TO_PTR(conn.modes_ptr));
			free(U64_TO_PTR(conn.encoders_ptr));
			return NULL;
		}

		if (kms_ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn)) {
			free(U64_TO_PTR(conn.props_ptr));
			free(U64_TO_PTR(conn.prop_values_ptr));
			free(U64_TO_PTR(conn.modes_ptr));
			free(U64_TO_PTR(conn.encoders_ptr));
			return NULL;
		}

		if (counts.count_props >= conn.count_props &&
		    counts.count_modes >= conn.count_modes &&
		    counts.count_encoders >= conn.count_encoders)
			break;

		free(U64_TO_PTR(conn.props_ptr));
		free(U64_TO_PTR(conn.prop_values_ptr));
		free(U64_TO_PTR(conn.modes_ptr));
		free(U64_TO_PTR(conn.encoders_ptr));
	}

	out = calloc(1, sizeof(*out));
	if (!out)
		goto fail;

	out->connector_id = conn.connector_id;
	out->encoder_id = conn.encoder_id;
	out->connector_type = conn.connector_type;
	out->connector_type_id = conn.connector_type_id;
	out->connection = conn.connection;
	out->mm_width = conn.mm_width;
	out->mm_height = conn.mm_height;
	out->subpixel = conn.subpixel;
	out->count_modes = count_to_int(conn.count_modes);
	out->count_props = count_to_int(conn.count_props);
	out->count_encoders = count_to_int(conn.count_encoders);
	out->props = copy_array(U64_TO_PTR(conn.props_ptr), conn.count_props,
				sizeof(uint32_t));
	out->prop_values = copy_array(U64_TO_PTR(conn.prop_values_ptr),
				      conn.count_props, sizeof(uint64_t));
	out->modes = copy_array(U64_TO_PTR(conn.modes_ptr), conn.count_modes,
				sizeof(struct kms_mode_info));
	out->encoders = copy_array(U64_TO_PTR(conn.encoders_ptr),
				   conn.count_encoders, sizeof(uint32_t));
	if ((conn.count_props && !out->props) ||
	    (conn.count_props && !out->prop_values) ||
	    (conn.count_modes && !out->modes) ||
	    (conn.count_encoders && !out->encoders)) {
		kms_free_connector(out);
		out = NULL;
	}

fail:
	free(U64_TO_PTR(conn.props_ptr));
	free(U64_TO_PTR(conn.prop_values_ptr));
	free(U64_TO_PTR(conn.modes_ptr));
	free(U64_TO_PTR(conn.encoders_ptr));
	return out;
}

void kms_free_connector(struct kms_connector *connector)
{
	if (!connector)
		return;
	free(connector->props);
	free(connector->prop_values);
	free(connector->modes);
	free(connector->encoders);
	free(connector);
}

struct kms_encoder *kms_get_encoder(int fd, uint32_t encoder_id)
{
	struct drm_mode_get_encoder enc;
	struct kms_encoder *out;

	memset(&enc, 0, sizeof(enc));
	enc.encoder_id = encoder_id;
	if (kms_ioctl(fd, DRM_IOCTL_MODE_GETENCODER, &enc))
		return NULL;

	out = calloc(1, sizeof(*out));
	if (!out)
		return NULL;
	out->encoder_id = enc.encoder_id;
	out->encoder_type = enc.encoder_type;
	out->crtc_id = enc.crtc_id;
	out->possible_crtcs = enc.possible_crtcs;
	out->possible_clones = enc.possible_clones;
	return out;
}

void kms_free_encoder(struct kms_encoder *encoder)
{
	free(encoder);
}

struct kms_crtc *kms_get_crtc(int fd, uint32_t crtc_id)
{
	struct drm_mode_crtc crtc;
	struct kms_crtc *out;

	memset(&crtc, 0, sizeof(crtc));
	crtc.crtc_id = crtc_id;
	if (kms_ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &crtc))
		return NULL;

	out = calloc(1, sizeof(*out));
	if (!out)
		return NULL;
	out->crtc_id = crtc.crtc_id;
	out->buffer_id = crtc.fb_id;
	out->x = crtc.x;
	out->y = crtc.y;
	out->mode_valid = (int)crtc.mode_valid;
	if (out->mode_valid) {
		memcpy(&out->mode, &crtc.mode, sizeof(out->mode));
		out->width = crtc.mode.hdisplay;
		out->height = crtc.mode.vdisplay;
	}
	out->gamma_size = (int)crtc.gamma_size;
	return out;
}

void kms_free_crtc(struct kms_crtc *crtc)
{
	free(crtc);
}

int kms_mode_add_fb(int fd, uint32_t width, uint32_t height, uint8_t depth,
		    uint8_t bpp, uint32_t pitch, uint32_t bo_handle,
		    uint32_t *fb_id)
{
	struct drm_mode_fb_cmd fb;

	memset(&fb, 0, sizeof(fb));
	fb.width = width;
	fb.height = height;
	fb.pitch = pitch;
	fb.bpp = bpp;
	fb.depth = depth;
	fb.handle = bo_handle;

	if (kms_ioctl(fd, DRM_IOCTL_MODE_ADDFB, &fb))
		return -1;
	*fb_id = fb.fb_id;
	return 0;
}

int kms_mode_rm_fb(int fd, uint32_t fb_id)
{
	return kms_ioctl(fd, DRM_IOCTL_MODE_RMFB, &fb_id);
}

int kms_mode_set_crtc(int fd, uint32_t crtc_id, uint32_t fb_id, uint32_t x,
		      uint32_t y, uint32_t *connectors, int count,
		      const struct kms_mode_info *mode)
{
	struct drm_mode_crtc crtc;

	memset(&crtc, 0, sizeof(crtc));
	crtc.x = x;
	crtc.y = y;
	crtc.crtc_id = crtc_id;
	crtc.fb_id = fb_id;
	crtc.set_connectors_ptr = PTR_TO_U64(connectors);
	crtc.count_connectors = (uint32_t)count;
	if (mode) {
		memcpy(&crtc.mode, mode, sizeof(crtc.mode));
		crtc.mode_valid = 1;
	}

	return kms_ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &crtc);
}

int kms_mode_page_flip(int fd, uint32_t crtc_id, uint32_t fb_id,
		       uint32_t flags, void *user_data)
{
	struct drm_mode_crtc_page_flip flip;

	memset(&flip, 0, sizeof(flip));
	flip.crtc_id = crtc_id;
	flip.fb_id = fb_id;
	flip.flags = flags;
	flip.user_data = PTR_TO_U64(user_data);
	return kms_ioctl(fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip);
}

int kms_handle_event(int fd, const struct kms_event_context *event_context)
{
	char buffer[1024];
	ssize_t len;
	size_t offset = 0;

	do {
		len = read(fd, buffer, sizeof(buffer));
	} while (len < 0 && errno == EINTR);

	if (len == 0)
		return 0;
	if (len < (ssize_t)sizeof(struct drm_event))
		return -1;

	while (offset < (size_t)len) {
		struct drm_event *event;

		if ((size_t)len - offset < sizeof(*event))
			return -1;
		event = (struct drm_event *)(buffer + offset);
		if (event->length < sizeof(*event) ||
		    offset + event->length > (size_t)len)
			return -1;

		if (event->type == DRM_EVENT_FLIP_COMPLETE &&
		    event->length >= sizeof(struct drm_event_vblank) &&
		    event_context && event_context->version >= 2 &&
		    event_context->page_flip_handler) {
			struct drm_event_vblank *vblank =
				(struct drm_event_vblank *)event;
			event_context->page_flip_handler(
				fd, vblank->sequence, vblank->tv_sec,
				vblank->tv_usec, U64_TO_PTR(vblank->user_data));
		}

		offset += event->length;
	}

	return 0;
}

int kms_create_dumb_buffer(int fd, uint32_t width, uint32_t height,
			   uint32_t bpp, uint32_t *handle, uint32_t *pitch,
			   uint64_t *size)
{
	struct drm_mode_create_dumb create;

	memset(&create, 0, sizeof(create));
	create.width = width;
	create.height = height;
	create.bpp = bpp;
	if (kms_ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create))
		return -1;

	*handle = create.handle;
	*pitch = create.pitch;
	*size = create.size;
	return 0;
}

int kms_map_dumb_buffer(int fd, uint32_t handle, uint64_t *offset)
{
	struct drm_mode_map_dumb map;

	memset(&map, 0, sizeof(map));
	map.handle = handle;
	if (kms_ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map))
		return -1;
	*offset = map.offset;
	return 0;
}

int kms_destroy_dumb_buffer(int fd, uint32_t handle)
{
	struct drm_mode_destroy_dumb destroy;

	memset(&destroy, 0, sizeof(destroy));
	destroy.handle = handle;
	return kms_ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

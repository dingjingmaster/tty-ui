#ifndef KMS_DRM_H
#define KMS_DRM_H

#include <stdint.h>

#define KMS_DISPLAY_MODE_NAME_LEN 32
#define KMS_MODE_CONNECTED 1U
#define KMS_MODE_TYPE_PREFERRED (1U << 3)
#define KMS_MODE_PAGE_FLIP_EVENT 0x01U

struct kms_mode_info {
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
	char name[KMS_DISPLAY_MODE_NAME_LEN];
};

struct kms_resources {
	int count_fbs;
	uint32_t *fbs;
	int count_crtcs;
	uint32_t *crtcs;
	int count_connectors;
	uint32_t *connectors;
	int count_encoders;
	uint32_t *encoders;
	uint32_t min_width;
	uint32_t max_width;
	uint32_t min_height;
	uint32_t max_height;
};

struct kms_connector {
	uint32_t connector_id;
	uint32_t encoder_id;
	uint32_t connector_type;
	uint32_t connector_type_id;
	uint32_t connection;
	uint32_t mm_width;
	uint32_t mm_height;
	uint32_t subpixel;
	int count_modes;
	struct kms_mode_info *modes;
	int count_props;
	uint32_t *props;
	uint64_t *prop_values;
	int count_encoders;
	uint32_t *encoders;
};

struct kms_encoder {
	uint32_t encoder_id;
	uint32_t encoder_type;
	uint32_t crtc_id;
	uint32_t possible_crtcs;
	uint32_t possible_clones;
};

struct kms_crtc {
	uint32_t crtc_id;
	uint32_t buffer_id;
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
	int mode_valid;
	struct kms_mode_info mode;
	int gamma_size;
};

typedef void (*kms_page_flip_handler)(int fd, unsigned int frame,
				      unsigned int sec, unsigned int usec,
				      void *data);

struct kms_event_context {
	int version;
	kms_page_flip_handler page_flip_handler;
};

struct kms_resources *kms_get_resources(int fd);
void kms_free_resources(struct kms_resources *resources);

struct kms_connector *kms_get_connector(int fd, uint32_t connector_id);
void kms_free_connector(struct kms_connector *connector);

struct kms_encoder *kms_get_encoder(int fd, uint32_t encoder_id);
void kms_free_encoder(struct kms_encoder *encoder);

struct kms_crtc *kms_get_crtc(int fd, uint32_t crtc_id);
void kms_free_crtc(struct kms_crtc *crtc);

int kms_mode_add_fb(int fd, uint32_t width, uint32_t height, uint8_t depth,
		    uint8_t bpp, uint32_t pitch, uint32_t bo_handle,
		    uint32_t *fb_id);
int kms_mode_rm_fb(int fd, uint32_t fb_id);
int kms_mode_set_crtc(int fd, uint32_t crtc_id, uint32_t fb_id, uint32_t x,
		      uint32_t y, uint32_t *connectors, int count,
		      const struct kms_mode_info *mode);
int kms_mode_page_flip(int fd, uint32_t crtc_id, uint32_t fb_id,
		       uint32_t flags, void *user_data);
int kms_handle_event(int fd, const struct kms_event_context *event_context);

int kms_create_dumb_buffer(int fd, uint32_t width, uint32_t height,
			   uint32_t bpp, uint32_t *handle, uint32_t *pitch,
			   uint64_t *size);
int kms_map_dumb_buffer(int fd, uint32_t handle, uint64_t *offset);
int kms_destroy_dumb_buffer(int fd, uint32_t handle);

#endif

#include <stddef.h>
#include <stdint.h>

/*
 * The embedded WQY Micro Hei TTC uses normal TrueType outlines.  These stubs
 * intentionally make FreeType's optional compressed-font and PNG bitmap paths
 * fail so the runtime binary does not depend on zlib, bzip2, libpng, or Brotli.
 */

#define STUB_ERROR (-1)

int BrotliDecoderDecompress(size_t encoded_size, const uint8_t *encoded_buffer,
			    size_t *decoded_size, uint8_t *decoded_buffer)
{
	(void)encoded_size;
	(void)encoded_buffer;
	(void)decoded_buffer;
	if (decoded_size)
		*decoded_size = 0;
	return 0;
}

int inflate(void *stream, int flush)
{
	(void)stream;
	(void)flush;
	return STUB_ERROR;
}

int inflateReset(void *stream)
{
	(void)stream;
	return STUB_ERROR;
}

int inflateEnd(void *stream)
{
	(void)stream;
	return STUB_ERROR;
}

int inflateInit2_(void *stream, int window_bits, const char *version,
		  int stream_size)
{
	(void)stream;
	(void)window_bits;
	(void)version;
	(void)stream_size;
	return STUB_ERROR;
}

int BZ2_bzDecompress(void *stream)
{
	(void)stream;
	return STUB_ERROR;
}

int BZ2_bzDecompressEnd(void *stream)
{
	(void)stream;
	return STUB_ERROR;
}

int BZ2_bzDecompressInit(void *stream, int verbosity, int small)
{
	(void)stream;
	(void)verbosity;
	(void)small;
	return STUB_ERROR;
}

void *png_create_read_struct(const char *user_png_ver, void *error_ptr,
			     void *error_fn, void *warn_fn)
{
	(void)user_png_ver;
	(void)error_ptr;
	(void)error_fn;
	(void)warn_fn;
	return NULL;
}

void *png_create_info_struct(void *png_ptr)
{
	(void)png_ptr;
	return NULL;
}

void *png_set_longjmp_fn(void *png_ptr, void *longjmp_fn, size_t jmp_buf_size)
{
	(void)png_ptr;
	(void)longjmp_fn;
	(void)jmp_buf_size;
	return NULL;
}

void png_destroy_read_struct(void *png_ptr_ptr, void *info_ptr_ptr,
			     void *end_info_ptr_ptr)
{
	(void)png_ptr_ptr;
	(void)info_ptr_ptr;
	(void)end_info_ptr_ptr;
}

void png_set_read_fn(void *png_ptr, void *io_ptr, void *read_data_fn)
{
	(void)png_ptr;
	(void)io_ptr;
	(void)read_data_fn;
}

void png_read_info(void *png_ptr, void *info_ptr)
{
	(void)png_ptr;
	(void)info_ptr;
}

uint32_t png_get_IHDR(void *png_ptr, void *info_ptr, uint32_t *width,
		      uint32_t *height, int *bit_depth, int *color_type,
		      int *interlace_method, int *compression_method,
		      int *filter_method)
{
	(void)png_ptr;
	(void)info_ptr;
	if (width)
		*width = 0;
	if (height)
		*height = 0;
	if (bit_depth)
		*bit_depth = 0;
	if (color_type)
		*color_type = 0;
	if (interlace_method)
		*interlace_method = 0;
	if (compression_method)
		*compression_method = 0;
	if (filter_method)
		*filter_method = 0;
	return 0;
}

void png_set_expand_gray_1_2_4_to_8(void *png_ptr)
{
	(void)png_ptr;
}

uint32_t png_get_valid(void *png_ptr, void *info_ptr, uint32_t flag)
{
	(void)png_ptr;
	(void)info_ptr;
	(void)flag;
	return 0;
}

int png_set_interlace_handling(void *png_ptr)
{
	(void)png_ptr;
	return 0;
}

void png_set_filler(void *png_ptr, uint32_t filler, int flags)
{
	(void)png_ptr;
	(void)filler;
	(void)flags;
}

void png_read_update_info(void *png_ptr, void *info_ptr)
{
	(void)png_ptr;
	(void)info_ptr;
}

void png_set_read_user_transform_fn(void *png_ptr, void *read_user_transform_fn)
{
	(void)png_ptr;
	(void)read_user_transform_fn;
}

void png_read_image(void *png_ptr, void *image)
{
	(void)png_ptr;
	(void)image;
}

void png_read_end(void *png_ptr, void *info_ptr)
{
	(void)png_ptr;
	(void)info_ptr;
}

void png_set_tRNS_to_alpha(void *png_ptr)
{
	(void)png_ptr;
}

void png_set_gray_to_rgb(void *png_ptr)
{
	(void)png_ptr;
}

void png_set_packing(void *png_ptr)
{
	(void)png_ptr;
}

void png_set_strip_16(void *png_ptr)
{
	(void)png_ptr;
}

void png_set_palette_to_rgb(void *png_ptr)
{
	(void)png_ptr;
}

void *png_get_error_ptr(void *png_ptr)
{
	(void)png_ptr;
	return NULL;
}

void *png_get_io_ptr(void *png_ptr)
{
	(void)png_ptr;
	return NULL;
}

void png_error(void *png_ptr, const char *error_message)
{
	(void)png_ptr;
	(void)error_message;
}

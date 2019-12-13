#ifndef NK_WL_IMAGE_H
#define NK_WL_IMAGE_H

#include <cairo/cairo.h>
#include "nk_wl_internal.h"
//hack for now
/* #include <stb/stb_image.h> */

#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 * image loaders
 *****************************************************************************/

NK_API bool
nk_wl_load_image_for_buffer(const char *path, enum wl_shm_format format,
			    int width, int height, unsigned char *mem)
{
	int w, h, channels;
	cairo_surface_t *src_surf, *image_surf;
	cairo_t *cr;
	cairo_format_t cairo_format = translate_wl_shm_format(format);
	if (cairo_format != CAIRO_FORMAT_ARGB32)
		goto err_format;

	//you have no choice but to load rgba, cairo deal with 32 bits only
	uint32_t *pixels = (uint32_t *)stbi_load(path, &w, &h, &channels,
					  STBI_rgb_alpha);
	if (w == 0 || h == 0 || channels == 0 || pixels == NULL)
		goto err_load;

	taiwins_rgba_t pixel;
	for (int i = 0; i < w * h; i++ ) {
		pixel.code = *(pixels+i);
		double p = pixel.a / 255.0;
		*(pixels+i) = ((uint32_t)pixel.a << 24) +
			((uint32_t)(pixel.r / p) << 16) +
			((uint32_t)(pixel.g / p) << 8) +
			((uint32_t)(pixel.b / p));
	}
	src_surf = cairo_image_surface_create_for_data(
		(unsigned char *)pixels, CAIRO_FORMAT_ARGB32,
		w, h,
		cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, w));
	image_surf = cairo_image_surface_create_for_data(
		(unsigned char *)mem, CAIRO_FORMAT_ARGB32,
		width, height,
		cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width));

	cr = cairo_create(image_surf);
	cairo_scale(cr, (double)width / w, (double)height / h);
	cairo_set_source_surface(cr, src_surf, 0, 0);
	cairo_paint(cr);

	stbi_image_free(pixels);
	cairo_destroy(cr);
	cairo_surface_destroy(image_surf);
	cairo_surface_destroy(src_surf);

	return true;
err_format:
err_load:
	return false;

}

NK_API struct nk_image
nk_wl_load_image(const char *path, enum wl_shm_format format,
		 int width, int height)
{
	struct nk_image image = {0};
	cairo_format_t cairo_format = translate_wl_shm_format(format);
	if (cairo_format != CAIRO_FORMAT_ARGB32)
		goto err_format;

	unsigned char *mem = malloc(width * height * 4);
	if (!nk_wl_load_image_for_buffer(path, format,
					 width, height, mem))
		goto err_load;
	image = nk_subimage_ptr(mem, width, height,
				nk_rect(0,0, width, height));
	return image;
err_load:
	free(mem);
err_format:
	return image;
}


NK_API void
nk_wl_free_image(struct nk_image *im)
{
	free(im->handle.ptr);
}


#ifdef __cplusplus
}
#endif

#endif /* EOF */

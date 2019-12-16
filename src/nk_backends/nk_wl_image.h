#ifndef NK_WL_IMAGE_H
#define NK_WL_IMAGE_H

#include <wayland-client.h>
#include <cairo/cairo.h>
#include <image_cache.h>
#include "nk_wl_internal.h"
//hack for now
/* #include <stb/stb_image.h> */

#ifdef __cplusplus
extern "C" {
#endif

struct nk_wl_image {
	struct wl_list link;
	struct nk_image image;
};

#if defined (NK_EGL_BACKEND)

static struct nk_image
nk_wl_to_gpu_image(const struct nk_image *cpu_image);

static void
nk_wl_free_gpu_image(const struct nk_image *gpu_image);

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
	uint32_t *pixels = (uint32_t *)image_load(path, &w, &h, &channels);
	if (w == 0 || h == 0 || channels == 0 || pixels == NULL)
		goto err_load;

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

	free(pixels);
	cairo_destroy(cr);
	cairo_surface_destroy(image_surf);
	cairo_surface_destroy(src_surf);

	return true;
err_format:
err_load:
	return false;

}

NK_API struct nk_image*
nk_wl_load_image(const char *path, enum wl_shm_format format,
                 struct nk_wl_backend *b)
{
	struct nk_wl_image *image = calloc(1, sizeof(struct nk_wl_image));
	wl_list_init(&image->link);
	cairo_format_t cairo_format = translate_wl_shm_format(format);
	if (cairo_format != CAIRO_FORMAT_ARGB32)
		goto err_format;
	int width, height, nchannels;
	image_info(path, &width, &height, &nchannels);
	if (!width || !height || !nchannels)
		goto err_format;

	unsigned char *mem = image_load(path, &width, &height, &nchannels);
	image->image = nk_subimage_ptr(mem, width, height,
				nk_rect(0,0, width, height));

#if defined (NK_EGL_BACKEND)
	image->image = nk_wl_to_gpu_image(&image->image);
#endif
	wl_list_insert(&b->images, &image->link);
	return &image->image;

err_format:
	free(image);
	return NULL;
}


NK_API void
nk_wl_free_image(struct nk_image *im)
{
	struct nk_wl_image *wl_img =
		container_of(im, struct nk_wl_image, image);
	wl_list_remove(&wl_img->link);

#if defined (NK_EGL_BACKEND)
	nk_wl_free_gpu_image(im);
#else
	free(im->handle.ptr);
#endif
}


#ifdef __cplusplus
}
#endif

#endif /* EOF */

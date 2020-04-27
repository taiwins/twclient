/*
 * nk_wl_image.h - taiwins client nuklear image handling functions
 *
 * Copyright (c) 2019 Xichen Zhou
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef NK_WL_IMAGE_H
#define NK_WL_IMAGE_H

#include <stdbool.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <cairo/cairo.h>
#include <image_cache.h>
#include <wayland-util.h>
#include "nk_wl_internal.h"
#include "ui.h"
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
nk_wl_to_gpu_image(const struct nk_image *, struct nk_wl_backend *);

static void
nk_wl_free_gpu_image(const struct nk_image *);

#endif


static inline struct nk_rect
nk_rect_from_bbox(const struct tw_bbox *box)
{
	return nk_rect(box->x , box->y,
	               box->w * box->s,
	               box->h * box->s);
}

/******************************************************************************
 * image loaders
 *****************************************************************************/

WL_EXPORT struct nk_image *
nk_wl_add_image(struct nk_image img, struct nk_wl_backend *b)
{
	struct nk_wl_image *image =
		calloc(1, sizeof(struct nk_wl_image));

	if (!image)
		return NULL;

	wl_list_init(&image->link);
	image->image = img;

	wl_list_insert(&b->images, &image->link);
	return &image->image;
}

struct nk_image
nk_wl_image_from_buffer(unsigned char *pixels, struct nk_wl_backend *b,
                        unsigned int width, unsigned int height,
                        unsigned int stride, bool take);


WL_EXPORT struct nk_image*
nk_wl_load_image(const char *path, enum wl_shm_format format,
                 struct nk_wl_backend *b)
{
	cairo_format_t cairo_format = translate_wl_shm_format(format);
	if (cairo_format != CAIRO_FORMAT_ARGB32)
		return NULL;
	int width, height, nchannels;
	image_info(path, &width, &height, &nchannels);
	if (!width || !height || !nchannels)
		return NULL;

	unsigned char *mem = image_load(path, &width, &height, &nchannels);

	return nk_wl_add_image(nk_wl_image_from_buffer(mem, b,
	                                               width, height,
	                                               width * 4, true),
	                       b);
}

WL_EXPORT void
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
	free(wl_img);
}


#ifdef __cplusplus
}
#endif

#endif /* EOF */

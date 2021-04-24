/*
 * image_cache.c - image cache generation functions
 *
 * Copyright (c) 2019-2020 Xichen Zhou
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <cairo.h>
#include <librsvg/rsvg.h>
#include <wayland-client-protocol.h>
#include <ctypes/os/file.h>

#include <twclient/image_cache.h>
#define STB_RECT_PACK_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <stb/stb_rect_pack.h>

/******************************************************************************
 * image operations
 *****************************************************************************/
static void
copy_subimage(unsigned char *dst, const unsigned char *src,
	      const uint32_t dst_width, const stbrp_rect *rect)
{
	uint32_t *row = NULL;
	uint32_t *row_src = NULL;
	for (int j = 0; j < rect->h; j++) {
		row = (uint32_t *)dst + (rect->y + j) * dst_width + (rect->x);
		row_src = (uint32_t *)src + j * rect->w;
		memcpy(row, row_src, rect->w * sizeof(uint32_t));
	}
}

/*
 * why do we need to do format transform here? Well, the image loader reads data
 * in exactly r,g,b,a order, stored in bytes! cairo on the other hand, uses
 * argb. Stores them in uint32_t, in byte order, it would become bgra (byte
 * order of cairo format is not that important, unless you upload them to OpenGL).
 */
static void
rgba_to_argb(unsigned char *dst, const unsigned char  *src,
             const int w, const int h)
{
	union argb {
		//this is the rgba byte order in small endian, cairo on the
		//other hand, uses argb in integer, it would be bgra in byte
		//order
		struct {
			uint8_t r, g, b, a;
		} data;
		uint32_t code;
	};
	//cairo alpha is pre-multiplied, we need do the same here
	union argb pixel;
	for (int j = 0; j < h; j++)
		for (int i = 0; i < w; i++) {
			pixel.code = *((uint32_t*)src + j * w + i);
			double alpha = pixel.data.a / 256.0;
			*((uint32_t *)dst + j * w + i) =
				((uint32_t)pixel.data.a << 24) +
				((uint32_t)(pixel.data.r * alpha) << 16) +
				((uint32_t)(pixel.data.g * alpha) << 8) +
				((uint32_t)(pixel.data.b * alpha));
		}
}

WL_EXPORT void
image_info(const char *path, int *w, int *h, int *nchannels)
{
	//take a wild guess on svgs(since most of cases they are icons)
	if (is_file_type(path, ".svg")) {
		*w = 128; *h = 128; *nchannels = 4;
	} else
		stbi_info(path, w, h, nchannels);
}

WL_EXPORT unsigned char *
image_load(const char *path, int *w, int *h, int *nchannels)
{
	unsigned char *imgdata = NULL;
	if (!is_file_exist(path))
		return imgdata;

	if (is_file_type(path, ".svg")) {
		cairo_surface_t *renderimg = NULL;
		cairo_t *cr = NULL;
		RsvgDimensionData dimension;
		RsvgHandle *svg = NULL;

		if ((svg = rsvg_handle_new_from_file(path, NULL)) == NULL)
			return NULL;

		rsvg_handle_get_dimensions(svg, &dimension);
		imgdata = calloc(1, 128 * cairo_format_stride_for_width(
			                      CAIRO_FORMAT_ARGB32, 128));
		renderimg = cairo_image_surface_create_for_data(
			imgdata, CAIRO_FORMAT_ARGB32, 128, 128,
			cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, 128));
		cr = cairo_create(renderimg);
		cairo_scale(cr, 128.0/ dimension.width,
		            128.0 / dimension.height);
		rsvg_handle_render_cairo(svg, cr);
		g_object_unref(svg);
		/* rsvg_handle_close(svg, NULL); */
		cairo_destroy(cr);
		cairo_surface_destroy(renderimg);
	} else {
		unsigned char *pixels =
			stbi_load(path, w, h, nchannels, STBI_rgb_alpha);
		imgdata = malloc(*w * *h * 4);
		rgba_to_argb(imgdata, pixels, *w, *h);
		stbi_image_free(pixels);
	}
	return imgdata;
}


WL_EXPORT bool
image_load_for_buffer(const char *path, enum wl_shm_format format,
                      int width, int height, unsigned char *mem)
{
	int w, h, channels;
	cairo_surface_t *src_surf, *image_surf;
	cairo_t *cr;
	cairo_format_t cairo_format = CAIRO_FORMAT_INVALID;

	//TODO: currently we only support argb888, which is a big waste, it
	//would be preferable to load them into 565.
	if (format != WL_SHM_FORMAT_ARGB8888)
		goto err_format;
	cairo_format = CAIRO_FORMAT_ARGB32;

	//you have no choice but to load rgba, cairo deal with 32 bits only
	uint32_t *pixels = (uint32_t *)image_load(path, &w, &h, &channels);
	if (w == 0 || h == 0 || channels == 0 || pixels == NULL)
		goto err_load;

	src_surf = cairo_image_surface_create_for_data(
		(unsigned char *)pixels, cairo_format,
		w, h,
		cairo_format_stride_for_width(cairo_format, w));
	image_surf = cairo_image_surface_create_for_data(
		(unsigned char *)mem, cairo_format,
		width, height,
		cairo_format_stride_for_width(cairo_format, width));

	cr = cairo_create(image_surf);
	cairo_scale(cr, (double)width / w, (double)height / h);
	cairo_set_source_surface(cr, src_surf, 0, 0);
	cairo_paint(cr);

	free(pixels);
	cairo_destroy(cr);
	cairo_surface_destroy(image_surf);
	cairo_surface_destroy(src_surf);

	return true;
err_load:
	image_surf = cairo_image_surface_create_for_data(
		(unsigned char *)mem, cairo_format,
		width, height,
		cairo_format_stride_for_width(cairo_format, width));
	cr = cairo_create(image_surf);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(image_surf);
err_format:
	return false;
}


/*******************************************************************************
 * image_cache
 ******************************************************************************/
static bool
dummy_filter(const char *input, void *data)
{
	(void)input;
	(void)data;
	return true;
}

WL_EXPORT struct image_cache
image_cache_from_arrays_filtered(const struct wl_array *handle_array,
                                 const struct wl_array *str_array,
                                 void (*convert)(char output[256], const char *input),
                                 bool (*filter)(const char *input, void *data),
                                 void *user_data)
{
	char objname[256];
	struct image_cache cache = {0};
	size_t nimages = handle_array->size / (sizeof(off_t));
	size_t context_height = 0, context_width = 1000;
	int row_x = 0, row_y = 0;
	stbrp_rect *rects = malloc(sizeof(stbrp_rect) * nimages);
	stbrp_node *nodes = malloc(sizeof(stbrp_node) * (context_width + 10));
	stbrp_context context;
	cairo_surface_t *atlas_surface;
	cairo_t *cr = NULL;
	cairo_format_t format = CAIRO_FORMAT_ARGB32;

	//initialize cache.
	wl_array_init(&cache.handles);
	wl_array_init(&cache.strings);
	wl_array_init(&cache.image_boxes);
	//first pass: loading decides atlas size
	for (unsigned i = 0; i < nimages; i++) {
		int w, h, nchannels;
		off_t handle = *((uint64_t *)handle_array->data + i);
		const char *path = (char *)str_array->data + handle;
		if (!filter(path, user_data))
			continue;
		image_info(path, &w, &h, &nchannels);
		rects[i].w = w;
		rects[i].h = h;
		//advance in tile. If current row still works, we go forward.
		//otherwise, start a new row
		row_x = (row_x + w > (signed)context_width) ?
			w : row_x + w;
		row_y = (row_x + w > (signed)context_width) ?
			row_y + h : MAX(row_y, h);
		context_height = row_y + h;
	}
	if (row_x == 0 || row_y == 0 || context_height == 0)
		goto out;

	stbrp_init_target(&context, context_width, context_height, nodes,
			  context_width+10);
	stbrp_setup_allow_out_of_mem(&context, 0);
	stbrp_pack_rects(&context, rects, nimages);
	//create image data
	cache.atlas = calloc(1, context_width * context_height *
			     sizeof(uint32_t));
	atlas_surface = cairo_image_surface_create_for_data(
		cache.atlas, format, context_width, context_height,
		cairo_format_stride_for_width(format, context_width));
	cr = cairo_create(atlas_surface);
	cache.dimension = tw_make_bbox_origin(context_width, context_height, 1);
	//pass 2 copy images
	for (unsigned i = 0; i < nimages; i++) {
		int w, h, nchannels; // channels
		off_t handle = *((uint64_t *)handle_array->data + i);
		const char *path = (const char *)str_array->data + handle;
		unsigned char *pixels;

		if (!filter(path, user_data))
			continue;
		if (rects[i].was_packed == 0)
			continue;
		if (!(pixels = image_load(path, &w, &h, &nchannels)))
			continue;

		copy_subimage(cache.atlas, pixels, context_width, &rects[i]);
		free(pixels);
		//convert the path to other handles if ncessary.
		if (convert) {
			convert(objname, path);
			path = objname;
		}
		//otherwise, copy the image path directly
		char *tocpy = wl_array_add(&cache.strings, strlen(path)+1);
		strcpy(tocpy, path);
		*(off_t *)wl_array_add(&cache.handles, sizeof(off_t)) =
			(tocpy - (char *)cache.strings.data);
		*(struct tw_bbox *)wl_array_add(&cache.image_boxes,
		                            sizeof(struct tw_bbox)) =
			tw_make_bbox(rects[i].x, rects[i].y, w, h, 1);
	}
	cairo_destroy(cr);
	cairo_surface_destroy(atlas_surface);

out:
	free(nodes);
	free(rects);

	return cache;

}

WL_EXPORT struct image_cache
image_cache_from_arrays(const struct wl_array *handle_array,
			const struct wl_array *str_array,
                        void (*convert)(char output[256], const char *input))
{
	return image_cache_from_arrays_filtered(handle_array, str_array,
	                                        convert, dummy_filter, NULL);
}

WL_EXPORT void
image_cache_release(struct image_cache *cache)
{
	if (cache->image_boxes.data)
		wl_array_release(&cache->image_boxes);
	if (cache->handles.data)
		wl_array_release(&cache->handles);
	if (cache->strings.data)
		wl_array_release(&cache->strings);
	if (cache->atlas)
		free(cache->atlas);
}

static cairo_status_t
cairo_write_png(void *closure, const unsigned char *data, unsigned int len)
{
	FILE *file = closure;
	if (fwrite(data, len, 1, file) != 1)
		return CAIRO_STATUS_WRITE_ERROR;
	return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_read_png(void *closure, unsigned char *data, unsigned int len)
{
	FILE *file = closure;
	if (fread(data, len, 1, file) != 1)
		return CAIRO_STATUS_READ_ERROR;
	return CAIRO_STATUS_SUCCESS;
}

WL_EXPORT void
image_cache_to_fd(const struct image_cache *cache, int fd)
{
	//we can simply use cairo writing functions
	if (!cache->atlas || !cache->image_boxes.data ||
	    !cache->handles.data || fd < 0)
		return;

	ssize_t written = -1;
	int fd1 = dup(fd);
	FILE *file = fdopen(fd1, "wb");

	cairo_surface_t *surf =
		cairo_image_surface_create_for_data(
			cache->atlas,
			CAIRO_FORMAT_ARGB32,
			cache->dimension.w, cache->dimension.h,
			cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32,
						      cache->dimension.w));
	uint32_t handle_len = cache->handles.size;
	uint32_t string_len = cache->strings.size;
	uint32_t boxes_len = cache->image_boxes.size;

	fseek(file, 0, SEEK_SET);
	if ((written = fwrite(&handle_len, sizeof(uint32_t), 1, file)) != 1)
		return;
	if ((written = fwrite(&string_len, sizeof(uint32_t), 1, file)) != 1)
		return;
	if ((written = fwrite(&boxes_len, sizeof(uint32_t), 1, file)) != 1)
		return;
	if ((written = fwrite(cache->handles.data, cache->handles.size, 1, file)) != 1)
		return;
	if ((written = fwrite(cache->strings.data, cache->strings.size, 1, file)) != 1)
		return;
	if ((written = fwrite(cache->image_boxes.data, cache->image_boxes.size, 1,
			      file)) != 1)
		return;
	if (cairo_surface_write_to_png_stream(surf, cairo_write_png, file) !=
	    CAIRO_STATUS_SUCCESS)
		fprintf(stderr, "write failed.\n");
	cairo_surface_destroy(surf);
	fclose(file);
}

WL_EXPORT struct image_cache
image_cache_from_fd(int fd)
{
	ssize_t reading = -1;
	struct image_cache cache = {0};
	uint32_t handle_len, string_len, boxes_len;
	int fd1 = dup(fd);
	FILE *file = fdopen(fd1, "rb");
	cairo_surface_t *surf = NULL;

	if (!file)
		goto out;
	fseek(file, 0, SEEK_SET);
	if ((reading = fread(&handle_len, sizeof(uint32_t), 1, file)) != 1)
		goto out;
	if ((reading = fread(&string_len, sizeof(uint32_t), 1, file)) != 1)
		goto out;
	if ((reading = fread(&boxes_len, sizeof(uint32_t), 1, file)) != 1)
		goto out;

	wl_array_add(&cache.handles, handle_len);
	wl_array_add(&cache.strings, string_len);
	wl_array_add(&cache.image_boxes, boxes_len);

	if ((reading = fread(cache.handles.data, handle_len, 1, file)) != 1)
		goto err;
	if ((reading = fread(cache.strings.data, string_len, 1, file)) != 1)
		goto err;
	if ((reading = fread(cache.image_boxes.data, boxes_len, 1, file)) != 1)
		goto err;
	surf =	cairo_image_surface_create_from_png_stream(cairo_read_png,
							   file);
	int w = cairo_image_surface_get_width(surf);
	int h = cairo_image_surface_get_height(surf);
	int s = cairo_image_surface_get_stride(surf);
	if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS)
		goto err;
	cache.dimension = tw_make_bbox_origin(w, h, 1);
	cache.atlas = malloc(h * s);
	memcpy(cache.atlas, cairo_image_surface_get_data(surf), h * s);

	cairo_surface_destroy(surf);
out:
	return cache;
err:
	if (surf)
		cairo_surface_destroy(surf);
	if (cache.atlas)
		free(cache.atlas);
	if (cache.handles.data)
		wl_array_release(&cache.handles);
	if (cache.strings.data)
		wl_array_release(&cache.strings);
	if (cache.image_boxes.data)
		wl_array_release(&cache.image_boxes);
	return cache;
}

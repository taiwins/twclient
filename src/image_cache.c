#include <sys/mman.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <cairo.h>
#include <librsvg/rsvg.h>

#include <image_cache.h>
#include <os/file.h>
#define STB_RECT_PACK_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <stb/stb_rect_pack.h>

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

//cairo has argb in uin32_t, which is bgra in byte order
static void
rgba_to_argb(unsigned char *dst, const unsigned char *src,
             const int w, const int h)
{
	union argb {
		//byte order readed
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

static inline void
image_info(const char *path, int *w, int *h, int *nchannels)
{
	//take a wild guess on svgs(since most of cases they are icons)
	if (is_file_type(path, ".svg")) {
		*w = 128; *h = 128; *nchannels = 4;
	} else
		stbi_info(path, w, h, nchannels);
}

static unsigned char *
image_load(const char *path, int *w, int *h, int *nchannels)
{
	unsigned char *imgdata = NULL;

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

struct image_cache
image_cache_from_arrays(const struct wl_array *handle_array,
			const struct wl_array *str_array,
			void (*convert)(char output[256], const char *input))
{
	char objname[256];
	struct image_cache cache;
	size_t nimages = handle_array->size / (sizeof(off_t));
	size_t context_height, context_width = 1000;
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
	for (int i = 0; i < nimages; i++) {
		int w, h, nchannels;
		off_t handle = *((uint64_t *)handle_array->data + i);
		const char *path = (char *)str_array->data + handle;
		image_info(path, &w, &h, &nchannels);
		rects[i].w = w;
		rects[i].h = h;
		//advance in tile. If current row still works, we go forward.
		//otherwise, start a new row
		row_x = (row_x + w > context_width) ?
			w : row_x + w;
		row_y = (row_x + w > context_width) ?
			row_y + h : MAX(row_y, h);
		//convert the path to other handles if ncessary.
		if (convert) {
			convert(objname, path);
			path = objname;
		}
		//otherwise, copy the image path directly
		char *tocpy = wl_array_add(&cache.strings, sizeof(path)+1);
		*(off_t *)wl_array_add(&cache.handles, sizeof(off_t)) =
			(tocpy - (char *)cache.strings.data);
	}
	context_height = row_y;
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

	cache.dimension = make_bbox_origin(context_width, context_height, 1);
	wl_array_add(&cache.image_boxes, sizeof(struct bbox) * nimages);
	//pass 2 copy images
	for (int i = 0; i < nimages; i++) {
		int w, h, nchannels; // channels
		off_t handle = *((uint64_t *)handle_array->data + i);
		const char *path = (const char *)str_array->data + handle;
		struct bbox *subimage = (struct bbox *)cache.image_boxes.data+i;
		unsigned char *pixels = image_load(path, &w, &h, &nchannels);
		if (!pixels)
			continue;

		copy_subimage(cache.atlas, pixels, context_width, &rects[i]);
		free(pixels);
		/* stbi_image_free(pixels); */
		*subimage = make_bbox(rects[i].x, rects[i].y, w, h, 1);
	}
	cairo_destroy(cr);
	cairo_surface_destroy(atlas_surface);

	free(nodes);
	free(rects);

	return cache;
}

void
image_cache_release(struct image_cache *cache)
{
	wl_array_release(&cache->image_boxes);
	wl_array_release(&cache->handles);
	wl_array_release(&cache->strings);
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

void
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

struct image_cache
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
	cache.dimension = make_bbox_origin(w, h, 1);
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

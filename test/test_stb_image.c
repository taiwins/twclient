#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <cairo/cairo.h>
#include <wayland-util.h>

#define STB_RECT_PACK_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_rect_pack.h>
#include <stb/stb_image.h>
#include <dirent.h>
#include <sys/types.h>
#include <sequential.h>
#include <os/file.h>

union argb {
	//byte order readed
	struct {
		uint8_t r, g, b, a;
	} data;
	uint32_t code;
};

static void
load_image(const char *input_path, const char *output_path)
{
	int w, h, channels;
	//you have no choice but to load rgba, cairo deal with 32 bits only
	unsigned char *pixels = stbi_load(input_path, &w, &h, &channels, STBI_rgb_alpha);


	//whats really loaded here is abgr(in the order of cairo)
	uint32_t *uint_pixels = (uint32_t *)pixels;
	union argb pixel;
	for (int i = 0; i < w * h; i++ ) {
		pixel.code = *(uint_pixels+i);
		*(uint_pixels+i) = ((uint32_t)pixel.data.a << 24) +
			((uint32_t)pixel.data.r << 16) +
			((uint32_t)pixel.data.g << 8) +
			((uint32_t)pixel.data.b);
	}
	cairo_surface_t *image_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 100);
	cairo_surface_t *src = cairo_image_surface_create_for_data(pixels, CAIRO_FORMAT_ARGB32, w, h,
								   cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, w));
	cairo_t *cr = cairo_create(image_surf);
	//clean up surface
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_paint(cr);
	cairo_set_source_surface(cr, src, 0, 0);
	cairo_rectangle(cr, 0, 0, 100, 100);
	cairo_fill(cr);

	cairo_destroy(cr);
	cairo_surface_write_to_png(image_surf, output_path);
	cairo_surface_destroy(image_surf);
	cairo_surface_destroy(src);
	stbi_image_free(pixels);
}

static void
copy_as_subimage(unsigned char *dst, const size_t dst_width,
		 const unsigned char *src, const stbrp_rect *rect)
{
	union argb pixel;
	for (int j = 0; j < rect->h; j++)
		for (int i = 0; i < rect->w; i++) {
			pixel.code = *((uint32_t*)src + j * rect->w + i);
			//cairo alpha is pre-multiplied, so we do the same here
			double alpha = pixel.data.a / 256.0;
			*((uint32_t *)dst + (rect->y+j) * dst_width + (rect->x+i)) =
				((uint32_t)pixel.data.a << 24) +
				((uint32_t)(pixel.data.r * alpha) << 16) +
				((uint32_t)(pixel.data.g * alpha) << 8) +
				((uint32_t)(pixel.data.b * alpha));
		}
}

static void
nk_wl_build_theme_images(struct wl_array *handle_pool, struct wl_array *string_pool,
			 const char *output_path)
{
	size_t nimages = handle_pool->size / sizeof(uint64_t);
	size_t context_height, context_width = 1000;
	int row_x = 0, row_y = 0;

	stbrp_rect *rects = malloc(sizeof(stbrp_rect) * nimages);
	stbrp_node *nodes = malloc(sizeof(stbrp_node) * 2000);
	stbrp_context context;
	cairo_surface_t *image_surface;

	//first pass: get the rects in places.
	for (int i = 0; i < nimages; i++) {
		int pos;
		int w, h, nchannels; //channels
		pos = *((uint64_t *)handle_pool->data + i);
		const char *path =
			(const char *)string_pool->data + pos;
		stbi_info(path, &w, &h, &nchannels);
		rects[i].w = w; rects[i].h = h;

		row_x = (row_x + w > context_width) ?
			w : row_x + w;
		row_y = (row_x + w > context_width) ?
			row_y + h : MAX(row_y, h);
	}
	context_height = row_y;
	stbrp_init_target(&context, context_width, context_height, nodes, 2000);
	stbrp_setup_allow_out_of_mem(&context, 0);

	stbrp_pack_rects(&context, rects, nimages);
	unsigned char *dst = calloc(1, context_width * context_height * 4);

	//second pass: load images
	for (int i = 0; i < nimages; i++) {
		intptr_t pos;
		int x, y, comp; //channels
		pos = *((uint64_t *)handle_pool->data + i);
		const char *path =
			(const char *)string_pool->data + pos;
		unsigned char *pixels = stbi_load(path, &x, &y, &comp, STBI_rgb_alpha);
		copy_as_subimage(dst, context_width, pixels,
				 &rects[i]);
		stbi_image_free(pixels);
	}
	image_surface = cairo_image_surface_create_for_data(
		dst, CAIRO_FORMAT_ARGB32,
		context_width, context_height,
		cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, context_width));
	cairo_surface_write_to_png(image_surface, output_path);
	cairo_surface_destroy(image_surface);
	free(dst);
	free(nodes);
	free(rects);
}

static int filter_image(const struct dirent *file)
{
	if (strstr(file->d_name, ".png"))
		return true;
	return false;
}

static void
build_image_strings(const char *path, struct wl_array *handle_pool, struct wl_array *string_pool)
{
	struct dirent **namelist;
	int n;
	long totallen = 0;

	wl_array_init(handle_pool);
	wl_array_init(string_pool);
	n = scandir(path, &namelist, filter_image, alphasort);
	if (n < 0) {
		perror("scandir");
		return;
	}
	wl_array_add(handle_pool, n * sizeof(uint64_t));
	for (int i = 0; i < n; i++) {
		*((uint64_t *)handle_pool->data + i) = totallen;
		totallen += strlen(path) + 1 + strlen(namelist[i]->d_name) + 1;
		wl_array_add(string_pool, totallen);
		sprintf((char *)string_pool->data + *((uint64_t*)handle_pool->data+i),
			"%s/%s",path, namelist[i]->d_name);
		free(namelist[i]);
	}
	free(namelist);
}

int main(int argc, char *argv[])
{
	/* load_image(argv[1], argv[2]); */
	/* return; */
	struct wl_array handle_pool, string_pool;
	build_image_strings(argv[1], &handle_pool, &string_pool);
	nk_wl_build_theme_images(&handle_pool, &string_pool, argv[2]);

	wl_array_release(&handle_pool);
	wl_array_release(&string_pool);
	return 0;
}

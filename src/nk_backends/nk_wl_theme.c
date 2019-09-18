/* this file is included by backends */
#include <assert.h>
#include <cairo.h>
#include "../../theme/theme.h"
#include "nk_wl_internal.h"


#define STB_RECT_PACK_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_rect_pack.h>
#include <stb/stb_image.h>

static inline struct nk_color
nk_color_from_tw(const taiwins_rgba_t *tc)
{
	struct nk_color nc;
	nc.r = tc->r; nc.g = tc->g;
	nc.b = tc->b; nc.a = tc->a;
	return nc;
}

static inline struct nk_vec2
nk_vec2_from_tw(const taiwins_vec2_t *vec)
{
	struct nk_vec2 v;
	v.x = vec->x;
	v.y = vec->y;
	return v;
}


/* this really needs to be in another frame */
void
nk_wl_build_theme_images(struct taiwins_theme *theme)
{
	size_t nimages = theme->handle_pool.size / sizeof(intptr_t);
	stbrp_context context;
	stbrp_init_target(&context, 2000, 2000, NULL, 2000);
	cairo_surface_t *image_surface;

	stbrp_rect rects[nimages];
	for (int i = 0; i < nimages; i++) {
		intptr_t pos;
		int x, y, comp; //channels
		pos = *((intptr_t *)theme->handle_pool.data + i);
		const char *path =
			(const char *)theme->string_pool.data + pos;
		const char * res =
			stbi_info(path, &x, &y, &comp);
		rects[i].w = x; rects[i].h = y;


	}
	stbrp_pack_rects(&context, rects, nimages);
	image_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 2000, 2000);
	cairo_t *cr = cairo_create(image_surface);

	//second pass: load images
	for (int i = 0; i < nimages; i++) {
		intptr_t pos;
		int x, y, comp; //channels
		pos = *((intptr_t *)theme->handle_pool.data + i);
		const char *path =
			(const char *)theme->string_pool.data + pos;
		char *pixels = stbi_load(path, &x, &y, &comp, STBI_rgb_alpha);
		//now we copy the image to desired location
		cairo_surface_t *source =  cairo_image_surface_create_for_data(
			pixels, CAIRO_FORMAT_ARGB32, x, y,
			cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, x));
		//now use cairo to render to sublocations
		cairo_move_to(cr, rects[i].x, rects[i].h);
		cairo_set_source_surface(cr, source, 0, 0);
		cairo_paint(cr);
		stbi_image_free(pixels);
	}
	cairo_destroy(cr);
}

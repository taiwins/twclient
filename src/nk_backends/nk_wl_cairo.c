#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <linux/input.h>
#include <time.h>
#include <stdbool.h>
#include <wayland-egl.h>
#include <wayland-client.h>
#include <cairo/cairo.h>
//this will pull the freetype headers
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

#define NK_IMPLEMENTATION
#define NK_CAIRO_BACKEND

#define MAX_CMD_SIZE = 64 * 1024

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_DEFAULT_FONT
#define NK_ZERO_COMMAND_MEMORY

#include "nk_wl_internal.h"

//////////////////////////////////// NK_CAIRO FONTS /////////////////////////////////////

//not thread safe
struct nk_wl_ftlib {
	FT_Library ftlib;
	int ref;
} NK_WL_FTLIB = {0};

static inline FT_Library *
nk_wl_ft_ref()
{
	if (!NK_WL_FTLIB.ref)
		FT_Init_FreeType(&NK_WL_FTLIB.ftlib);
	NK_WL_FTLIB.ref++;
	return &NK_WL_FTLIB.ftlib;
}

static inline void
nk_wl_ft_unref(FT_Library *lib)
{
	if (!NK_WL_FTLIB.ref || lib != &NK_WL_FTLIB.ftlib)
		return;
	NK_WL_FTLIB.ref = MAX(0, NK_WL_FTLIB.ref-1);
	if (!NK_WL_FTLIB.ref)
		FT_Done_FreeType(NK_WL_FTLIB.ftlib);
}

struct nk_wl_user_font {
	struct wl_list link;
	int size;
	float scale;
	FT_Face face;
	FT_Library *ft_lib;
	cairo_font_face_t *cairo_face;
	struct nk_user_font nk_font;
};


static float
nk_wl_text_width(nk_handle handle, float height, const char *text,
                 int utf8_len)
{
	struct nk_wl_user_font *user_font = handle.ptr;
	cairo_surface_t *recording = cairo_recording_surface_create(CAIRO_CONTENT_COLOR, NULL);
	cairo_text_extents_t extents;
	cairo_t *cr = cairo_create(recording);
	cairo_set_font_face(cr, user_font->cairo_face);
	cairo_set_font_size(cr, user_font->size);

	int nglyphs;
	cairo_glyph_t *glyphs = NULL;
	/* int nclusters; */
	/* cairo_text_cluster_t *clusters = NULL; */
	/* cairo_text_cluster_flags_t cluster_flags; */

	cairo_scaled_font_t *scaled_font = cairo_get_scaled_font(cr);
	cairo_scaled_font_text_to_glyphs(scaled_font, 0, 0, text,
	                                 utf8_len, &glyphs, &nglyphs,
	                                 NULL, NULL, NULL);

	cairo_scaled_font_glyph_extents(scaled_font, glyphs, nglyphs, &extents);
	cairo_glyph_free(glyphs);
	cairo_destroy(cr);
	cairo_surface_destroy(recording);
	return extents.x_advance;
}

void
nk_wl_render_text(cairo_t *cr, const struct nk_vec2 *pos,
                  struct nk_wl_user_font *font,
                  const char *text, const int utf8_len)
{
	cairo_set_font_face(cr, font->cairo_face);
	cairo_set_font_size(cr, font->size);

	int nglyphs;
	cairo_glyph_t *glyphs = NULL;
	int nclusters;
	cairo_text_cluster_t *clusters = NULL;
	cairo_text_cluster_flags_t cluster_flags;
	cairo_font_extents_t extents;

	cairo_scaled_font_t *scaled_font = cairo_get_scaled_font(cr);
	cairo_scaled_font_extents(scaled_font, &extents);
	cairo_scaled_font_text_to_glyphs(scaled_font, pos->x, pos->y+extents.ascent,
	                                 text, utf8_len, &glyphs, &nglyphs,
	                                 &clusters, &nclusters, &cluster_flags);
	//render
	cairo_show_text_glyphs(cr, text, utf8_len, glyphs,
	                        nglyphs, clusters, nclusters,
	                        cluster_flags);
	cairo_glyph_free(glyphs);
	cairo_text_cluster_free(clusters);

}

struct nk_user_font *
nk_wl_new_font(struct nk_wl_font_config config, struct nk_wl_backend *b)
{
	int error;
	char *font_path = NULL;
	if (!config.name)
		config.name = "Vera";
	if (!config.pix_size)
		config.pix_size = 16;
	if (!config.scale)
		config.scale = 1;
	font_path = nk_wl_find_font(&config);
	if (!font_path)
		return NULL;
	//else, we try to creat
	struct nk_wl_user_font *user_font =
		malloc(sizeof(struct nk_wl_user_font));
	wl_list_init(&user_font->link);
	user_font->ft_lib = nk_wl_ft_ref();
	if (!user_font->ft_lib)
		goto err_lib;

	user_font->size = config.pix_size;
	user_font->scale = config.scale;
	error = FT_New_Face(*user_font->ft_lib, font_path, 0, &user_font->face);
	if (error)
		goto err_face;
	user_font->cairo_face = cairo_ft_font_face_create_for_ft_face(user_font->face, 0);
	if (!user_font->cairo_face)
		goto err_crface;
	user_font->nk_font.height = config.pix_size;
	user_font->nk_font.userdata.ptr = user_font;
	user_font->nk_font.width = nk_wl_text_width;
	free(font_path);
	return &user_font->nk_font;

err_crface:
	FT_Done_Face(user_font->face);
err_face:
	nk_wl_ft_unref(user_font->ft_lib);
err_lib:
	free(user_font);
	free(font_path);
	return NULL;
}

void
nk_wl_destroy_font(struct nk_user_font *font)
{
	struct nk_wl_user_font *cairo_font = font->userdata.ptr;
	wl_list_remove(&cairo_font->link);

	cairo_font_face_destroy(cairo_font->cairo_face);
	FT_Done_Face(cairo_font->face);
	nk_wl_ft_unref(cairo_font->ft_lib);
	free(cairo_font);
}

/////////////////////////////////// NK_CAIRO backend /////////////////////////////////////

struct nk_cairo_backend {
	struct nk_wl_backend base;
	nk_max_cmd_t last_cmds[2];
	struct nk_user_font *default_font;
	//this is a stack we keep right now, all other method failed.
};


typedef void (*nk_cairo_op) (cairo_t *cr, const struct nk_command *cmd);

#ifndef NK_COLOR_TO_FLOAT
#define NK_COLOR_TO_FLOAT(x) ({ (double)x / 255.0; })
#endif

#ifndef NK_CAIRO_DEG_TO_RAD
#define NK_CAIRO_DEG_TO_RAD(x) ({ (double) x * NK_PI / 180.0;})
#endif

static inline void
nk_cairo_set_painter(cairo_t *cr, const struct nk_color *color, unsigned short line_width)
{
	cairo_set_source_rgba(
		cr,
		NK_COLOR_TO_FLOAT(color->r),
		NK_COLOR_TO_FLOAT(color->g),
		NK_COLOR_TO_FLOAT(color->b),
		NK_COLOR_TO_FLOAT(color->a));
	if (line_width != 0)
		cairo_set_line_width(cr, line_width);
}

static inline void
nk_cairo_mesh_pattern_set_corner_color(cairo_pattern_t *pat, int idx, struct nk_color color)
{
	float r, g, b, a;
	r = NK_COLOR_TO_FLOAT(color.r);
	g = NK_COLOR_TO_FLOAT(color.g);
	b = NK_COLOR_TO_FLOAT(color.b);
	a = NK_COLOR_TO_FLOAT(color.a);
	/* nk_cairo_get_color(color, &r, &g, &b, &a); */
	cairo_mesh_pattern_set_corner_color_rgba(pat, idx, r, g, b, a);
}

static void
nk_cairo_noop(cairo_t *cr, const struct nk_command *cmd)
{
	fprintf(stderr, "cairo: no operation applied\n");
}

static void
nk_cairo_scissor(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_scissor *s =
		(const struct nk_command_scissor *) cmd;
	cairo_reset_clip(cr);
	if (s->x >= 0) {
		cairo_rectangle(cr, s->x - 1, s->y - 1,
				s->w+2, s->h+2);
		cairo_clip(cr);
	}
}

static void
nk_cairo_line(cairo_t *cr, const struct nk_command *cmd)
{
       const struct nk_command_line *l =
	       (const struct nk_command_line *) cmd;
       cairo_set_source_rgba(
	       cr,
	       NK_COLOR_TO_FLOAT(l->color.r),
	       NK_COLOR_TO_FLOAT(l->color.g),
	       NK_COLOR_TO_FLOAT(l->color.b),
	       NK_COLOR_TO_FLOAT(l->color.a));
       cairo_set_line_width(cr, l->line_thickness);
       cairo_move_to(cr, l->begin.x, l->begin.y);
       cairo_line_to(cr, l->end.x, l->end.y);
       cairo_stroke(cr);
}

static void
nk_cairo_curve(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_curve *q =
		(const struct nk_command_curve *)cmd;
	nk_cairo_set_painter(cr, &q->color, q->line_thickness);
	cairo_move_to(cr, q->begin.x, q->begin.y);
	cairo_curve_to(cr, q->ctrl[0].x, q->ctrl[0].y,
		       q->ctrl[1].x, q->ctrl[1].y,
		       q->end.x, q->end.y);
	cairo_stroke(cr);
}

static void
nk_cairo_rect(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_rect *r =
		(const struct nk_command_rect *) cmd;
	nk_cairo_set_painter(cr, &r->color, r->line_thickness);
	if (r->rounding == 0)
		cairo_rectangle(cr, r->x, r->y, r->w, r->h);
	else {
		int xl = r->x + r->w - r->rounding;
		int xr = r->x + r->rounding;
		int yl = r->y + r->h - r->rounding;
		int yr = r->y + r->rounding;
		cairo_new_sub_path(cr);
		cairo_arc(cr, xl, yr, r->rounding,
			  NK_CAIRO_DEG_TO_RAD(-90),
			  NK_CAIRO_DEG_TO_RAD(0));
		cairo_arc(cr, xl, yl, r->rounding,
			  NK_CAIRO_DEG_TO_RAD(0),
			  NK_CAIRO_DEG_TO_RAD(90));
		cairo_arc(cr, xr, yl, r->rounding,
			  NK_CAIRO_DEG_TO_RAD(90),
			  NK_CAIRO_DEG_TO_RAD(180));
		cairo_arc(cr, xr, yr, r->rounding,
			  NK_CAIRO_DEG_TO_RAD(180),
			  NK_CAIRO_DEG_TO_RAD(270));
		cairo_close_path(cr);
	}
	cairo_stroke(cr);
}

static void
nk_cairo_rect_filled(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_rect_filled *r =
		(const struct nk_command_rect_filled *)cmd;
	nk_cairo_set_painter(cr, &r->color, 0);
	if (r->rounding == 0)
		cairo_rectangle(cr, r->x, r->y, r->w, r->h);
	else {
		int xl = r->x + r->w - r->rounding;
		int xr = r->x + r->rounding;
		int yl = r->y + r->h - r->rounding;
		int yr = r->y + r->rounding;
		cairo_new_sub_path(cr);
		cairo_arc(cr, xl, yr, r->rounding,
			  NK_CAIRO_DEG_TO_RAD(-90),
			  NK_CAIRO_DEG_TO_RAD(0));
		cairo_arc(cr, xl, yl, r->rounding,
			  NK_CAIRO_DEG_TO_RAD(0),
			  NK_CAIRO_DEG_TO_RAD(90));
		cairo_arc(cr, xr, yl, r->rounding,
			  NK_CAIRO_DEG_TO_RAD(90),
			  NK_CAIRO_DEG_TO_RAD(180));
		cairo_arc(cr, xr, yr, r->rounding,
			  NK_CAIRO_DEG_TO_RAD(180),
			  NK_CAIRO_DEG_TO_RAD(270));
		cairo_close_path(cr);
	}
	cairo_fill(cr);
}

static void
nk_cairo_rect_multi_color(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_rect_multi_color *r =
	   (const struct nk_command_rect_multi_color *) cmd;
	cairo_pattern_t *pat = cairo_pattern_create_mesh();
	if (pat) {
		cairo_mesh_pattern_begin_patch(pat);
		cairo_mesh_pattern_move_to(pat, r->x, r->y);
		cairo_mesh_pattern_line_to(pat, r->x, r->y + r->h);
		cairo_mesh_pattern_line_to(pat, r->x + r->w, r->y + r->h);
		cairo_mesh_pattern_line_to(pat, r->x + r->w, r->y);
		nk_cairo_mesh_pattern_set_corner_color(pat, 0, r->left);
		nk_cairo_mesh_pattern_set_corner_color(pat, 1, r->bottom);
		nk_cairo_mesh_pattern_set_corner_color(pat, 2, r->right);
		nk_cairo_mesh_pattern_set_corner_color(pat, 3, r->top);
		cairo_mesh_pattern_end_patch(pat);

		cairo_rectangle(cr, r->x, r->y, r->w, r->h);
		cairo_set_source(cr, pat);
		cairo_fill(cr);

		cairo_pattern_destroy(pat);
	}
}

static void
nk_cairo_circle(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_circle *c =
		(const struct nk_command_circle *) cmd;
	nk_cairo_set_painter(cr, &c->color, c->line_thickness);
	//based on the doc from cairo, the save here is to avoid the artifacts
	//of non-uniform width size of curve
	cairo_save(cr);
	cairo_translate(cr, c->x+ c->w / 2.0,
		c->y + c->h / 2.0);
	//apply the scaling in a new path
	cairo_new_sub_path(cr);
	cairo_scale(cr, c->w/2.0, c->h/2.0);
	cairo_arc(cr, 0, 0, 1, NK_CAIRO_DEG_TO_RAD(0),
		  NK_CAIRO_DEG_TO_RAD(360));
	cairo_close_path(cr);
	//now we restore the matrix
	cairo_restore(cr);
	cairo_stroke(cr);
}

static void
nk_cairo_circle_filled(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_circle_filled *c =
		(const struct nk_command_circle_filled *)cmd;
	nk_cairo_set_painter(cr, &c->color, 0);
	cairo_save(cr);
	cairo_translate(cr, c->x+c->w/2.0, c->y+c->h/2.0);
	cairo_scale(cr, c->w/2.0, c->h/2.0);
	cairo_new_sub_path(cr);
	cairo_arc(cr, 0, 0, 1, NK_CAIRO_DEG_TO_RAD(0),
			   NK_CAIRO_DEG_TO_RAD(360));
	cairo_close_path(cr);
	cairo_restore(cr);
	cairo_fill(cr);
}

static void
nk_cairo_arc(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_arc *a =
		(const struct nk_command_arc *)cmd;
	nk_cairo_set_painter(cr, &a->color, a->line_thickness);
	cairo_arc(cr, a->cx, a->cy, a->r,
		  NK_CAIRO_DEG_TO_RAD(a->a[0]),
		  NK_CAIRO_DEG_TO_RAD(a->a[1]));
	cairo_stroke(cr);
}

static void
nk_cairo_arc_filled(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_arc_filled *a =
		(const struct nk_command_arc_filled *) cmd;
	nk_cairo_set_painter(cr, &a->color, 0);
	cairo_arc(cr, a->cx, a->cy, a->r,
		  NK_CAIRO_DEG_TO_RAD(a->a[0]),
		  NK_CAIRO_DEG_TO_RAD(a->a[1]));
	cairo_fill(cr);
}

static void
nk_cairo_triangle(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_triangle *t =
		(const struct nk_command_triangle *)cmd;
	nk_cairo_set_painter(cr, &t->color, t->line_thickness);
	cairo_move_to(cr, t->a.x, t->a.y);
	cairo_line_to(cr, t->b.x, t->b.y);
	cairo_line_to(cr, t->c.x, t->c.y);
	cairo_close_path(cr);
	cairo_stroke(cr);
}

static void
nk_cairo_triangle_filled(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_triangle_filled *t =
		(const struct nk_command_triangle_filled *)cmd;
	nk_cairo_set_painter(cr, &t->color, 0);
	cairo_move_to(cr, t->a.x, t->a.y);
	cairo_line_to(cr, t->b.x, t->b.y);
	cairo_line_to(cr, t->c.x, t->c.y);
	cairo_close_path(cr);
	cairo_fill(cr);
}

static void
nk_cairo_polygon(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_polygon *p =
		(const struct nk_command_polygon *)cmd;
	nk_cairo_set_painter(cr, &p->color, p->line_thickness);
	cairo_move_to(cr, p->points[0].x, p->points[0].y);
	for (int i = 1; i < p->point_count; i++)
		cairo_line_to(cr, p->points[i].x, p->points[i].y);
	cairo_close_path(cr);
	cairo_stroke(cr);
}

static void
nk_cairo_polygon_filled(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_polygon_filled *p =
		(const struct nk_command_polygon_filled *)cmd;
	nk_cairo_set_painter(cr, &p->color, 0);
	cairo_move_to(cr, p->points[0].x, p->points[0].y);
	for (int i = 1; i < p->point_count; i++)
		cairo_line_to(cr, p->points[i].x, p->points[i].y);
	cairo_close_path(cr);
	cairo_fill(cr);
}

static void
nk_cairo_polyline(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_polyline *p =
		(const struct nk_command_polyline *)cmd;
	nk_cairo_set_painter(cr, &p->color, p->line_thickness);
	cairo_move_to(cr, p->points[0].x, p->points[0].y);
	for (int i = 1; i < p->point_count; i++)
		cairo_line_to(cr, p->points[i].x, p->points[i].y);
	cairo_stroke(cr);
}

static void
nk_cairo_text(cairo_t *cr, const struct nk_command *cmd)
{
	cairo_matrix_t matrix;
	cairo_get_matrix(cr, &matrix);
	const struct nk_command_text *t =
		(const struct nk_command_text *)cmd;
	/* fprintf(stderr, "before rendering (xx:%f, yy:%f), (x0:%f, y0:%f)\t", */
	/*	matrix.xx, matrix.yy, matrix.x0, matrix.y0); */
	/* /\* fprintf(stderr, "render_pos: (%d, %d)\t", t->x, t->y); *\/ */
	/* fprintf(stderr, "str:%s   ", t->string); */
	/* fprintf(stderr, "color: (%d,%d,%d)\n", t->foreground.r, t->foreground.g, t->foreground.b); */
	cairo_set_source_rgb(cr, NK_COLOR_TO_FLOAT(t->foreground.r),
			     NK_COLOR_TO_FLOAT(t->foreground.g),
			     NK_COLOR_TO_FLOAT(t->foreground.b));
	struct nk_vec2 rpos = nk_vec2(t->x, t->y);
	nk_wl_render_text(cr, &rpos, t->font->userdata.ptr, t->string, t->length);

	/* struct nk_cairo_font *font = t->font->userdata.ptr; */
	/* nk_cairo_render_text(cr, &rpos, font, t->string, t->length); */
}

static void
nk_cairo_image(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_image *im =
		(const struct nk_command_image *)cmd;
	cairo_surface_t *img_surf = im->img.handle.ptr;
	if (!img_surf || cairo_image_surface_get_format(img_surf) == CAIRO_FORMAT_INVALID)
		return;

	double w = cairo_image_surface_get_width(img_surf);
	double h = cairo_image_surface_get_height(img_surf);
	cairo_pattern_t *pat = cairo_pattern_create_for_surface(img_surf);

	/* cairo_paint(cr); */
	cairo_rectangle(cr, im->x, im->y, im->w, im->h);
	cairo_scale(cr, im->w / w, im->h / h);
	cairo_set_source(cr, pat);
	cairo_fill(cr);
	cairo_pattern_destroy(pat);
}

static void
nk_cairo_custom(cairo_t *cr, const struct nk_command *cmd)
{
	const struct nk_command_custom *cu =
		(const struct nk_command_custom *)cmd;
	cu->callback(cr, cu->x, cu->y, cu->w, cu->h,
		     cu->callback_data);
}

static const nk_cairo_op nk_cairo_ops[] = {
	nk_cairo_noop,
	nk_cairo_scissor,
	nk_cairo_line,
	nk_cairo_curve,
	nk_cairo_rect,
	nk_cairo_rect_filled,
	nk_cairo_rect_multi_color,
	nk_cairo_circle,
	nk_cairo_circle_filled,
	nk_cairo_arc,
	nk_cairo_arc_filled,
	nk_cairo_triangle,
	nk_cairo_triangle_filled,
	nk_cairo_polygon,
	nk_cairo_polygon_filled,
	nk_cairo_polyline,
	nk_cairo_text,
	nk_cairo_image,
	nk_cairo_custom,
};

#ifdef _GNU_SOURCE

#define NO_COMMAND "nk_cairo: command mismatch"
_Static_assert(NK_COMMAND_NOP == 0, NO_COMMAND);
_Static_assert(NK_COMMAND_SCISSOR == 1, NO_COMMAND);
_Static_assert(NK_COMMAND_LINE == 2, NO_COMMAND);
_Static_assert(NK_COMMAND_CURVE == 3, NO_COMMAND);
_Static_assert(NK_COMMAND_RECT == 4, NO_COMMAND);
_Static_assert(NK_COMMAND_RECT_FILLED == 5, NO_COMMAND);
_Static_assert(NK_COMMAND_RECT_MULTI_COLOR == 6, NO_COMMAND);
_Static_assert(NK_COMMAND_CIRCLE == 7, NO_COMMAND);
_Static_assert(NK_COMMAND_CIRCLE_FILLED == 8, NO_COMMAND);
_Static_assert(NK_COMMAND_ARC == 9, NO_COMMAND);
_Static_assert(NK_COMMAND_ARC_FILLED == 10, NO_COMMAND);
_Static_assert(NK_COMMAND_TRIANGLE == 11, NO_COMMAND);
_Static_assert(NK_COMMAND_TRIANGLE_FILLED == 12, NO_COMMAND);
_Static_assert(NK_COMMAND_POLYGON == 13, NO_COMMAND);
_Static_assert(NK_COMMAND_POLYGON_FILLED == 14, NO_COMMAND);
_Static_assert(NK_COMMAND_POLYLINE == 15, NO_COMMAND);
_Static_assert(NK_COMMAND_TEXT == 16, NO_COMMAND);
_Static_assert(NK_COMMAND_IMAGE == 17, NO_COMMAND);
_Static_assert(NK_COMMAND_CUSTOM == 18, NO_COMMAND);

#endif



static void
nk_cairo_render(struct wl_buffer *buffer, struct nk_cairo_backend *b,
		struct app_surface *surf)
{
	int w = surf->allocation.w;
	int h = surf->allocation.h;
	int s = surf->allocation.s;

	struct nk_wl_backend *bkend = &b->base;
	cairo_format_t format = translate_wl_shm_format(surf->pool->format);
	cairo_surface_t *image_surface =
		cairo_image_surface_create_for_data(
			shm_pool_buffer_access(buffer),
			format, w * s, h * s,
			cairo_format_stride_for_width(format, w * s));
	cairo_t *cr = cairo_create(image_surface);
	cairo_surface_destroy(image_surface);

	const struct nk_command *cmd = NULL;
	//1) clean this buffer using its background color, or maybe nuklear does
	//that already
	cairo_push_group(cr);
	cairo_set_source_rgb(cr,
			     bkend->main_color.r / 255.0,
			     bkend->main_color.g / 255.0,
			     bkend->main_color.b / 255.0);
	cairo_paint(cr);
	cairo_scale(cr, s, s);
	nk_foreach(cmd, &bkend->ctx) {
		nk_cairo_ops[cmd->type](cr, cmd);
	}
	cairo_pop_group_to_source(cr);
	cairo_paint(cr);
	cairo_surface_flush(cairo_get_target(cr));
	cairo_destroy(cr);

}

static void
nk_wl_render(struct nk_wl_backend *bkend)
{
	struct nk_cairo_backend *b =
		container_of(bkend, struct nk_cairo_backend, base);
	struct app_surface *surf = bkend->app_surface;
	struct wl_buffer *free_buffer = NULL;
	bool *to_commit = NULL;
	bool *to_dirty = NULL;

	for (int i = 0; i < 2; i++) {
		if (surf->committed[i] || surf->dirty[i])
			continue;
		free_buffer = surf->wl_buffer[i];
		to_commit = &surf->committed[i];
		to_dirty = &surf->dirty[i];
		break;
	}
	if (nk_wl_maybe_skip(bkend))
		return;
	if (!free_buffer)
		return;

	*to_dirty = true;

	nk_cairo_render(free_buffer, b, surf);
	wl_surface_attach(surf->wl_surface, free_buffer, 0, 0);
	wl_surface_damage(surf->wl_surface, 0, 0,
			  surf->allocation.w, surf->allocation.h);
	wl_surface_commit(surf->wl_surface);
	*to_commit = true;
	*to_dirty = false;
}


static void
nk_wl_resize(struct app_surface *surf, const struct app_event *e)
{
	shm_buffer_resize(surf, e);
}



static void
nk_cairo_destroy_app_surface(struct app_surface *app)
{
	struct nk_wl_backend *b = app->user_data;
	nk_wl_clean_app_surface(b);
	shm_buffer_destroy_app_surface(app);
}


void
nk_cairo_impl_app_surface(struct app_surface *surf, struct nk_wl_backend *bkend,
			  nk_wl_drawcall_t draw_cb, struct bbox geo)
{
	struct nk_cairo_backend *b =
		container_of(bkend, struct nk_cairo_backend, base);
	struct nk_wl_user_font *user_font = b->default_font->userdata.ptr;

	nk_wl_impl_app_surface(surf, bkend, draw_cb, geo);
	shm_buffer_reallocate(surf, &geo);
	surf->destroy = nk_cairo_destroy_app_surface;
	//change the font size here,
	user_font->size = (int)bkend->row_size;
}


struct nk_wl_backend *
nk_cairo_create_bkend(void)
{
	struct nk_wl_font_config default_config = {
		.name = "nerd",
		.slant = NK_WL_SLANT_ROMAN,
		.pix_size = 16,
		.scale = 1,
		.nranges = 0,
		.ranges = NULL,
		.TTFonly = true,
	};
	struct nk_cairo_backend *b = malloc(sizeof(struct nk_cairo_backend));
	wl_list_init(&b->base.images);
	wl_list_init(&b->base.fonts);
	b->default_font =
		nk_wl_new_font(default_config, &b->base);
	b->base.theme_hash = 0;
	nk_init_default(&b->base.ctx, b->default_font);
	return &b->base;
}


void
nk_cairo_destroy_bkend(struct nk_wl_backend *bkend)
{
	struct nk_wl_user_font *font, *tmp_font = NULL;
	struct nk_wl_image *image, *tmp_img = NULL;
	wl_list_for_each_safe(image, tmp_img, &bkend->images, link)
		nk_wl_free_image(&image->image);
	wl_list_for_each_safe(font, tmp_font, &bkend->fonts, link)
		nk_wl_destroy_font(&font->nk_font);

	nk_free(&bkend->ctx);
	//we make it here to
	//float unused_val = nk_sin(1.0);
	//unused_val = nk_cos(unused_val);
	//unused_val = nk_sqrt(unused_val);
	//(void)unused_val;
}

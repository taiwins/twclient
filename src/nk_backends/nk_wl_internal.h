#ifndef NK_WL_INTERNAL_H
#define NK_WL_INTERNAL_H

/* this file should have contained a an template struct which supposed to be
 * accessible all nuklear_backends here.
 *
 * declare NK_IMPLEMENTATION and other specific maro before INCLUDE this file !
 */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <wayland-client.h>
#include <lua.h>
#include <lauxlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined (__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#elif defined (__clang__)
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

/* we include this so we do not need to deal with namespace collision, in the
 * mean time, for the sake private implementation, all the function declared
 * here should be declared as static.
 */
//#define NK_PRIVATE
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_VARARGS

//this will make our struct various size, so lets put the buffer in the end
#define NK_MAX_CTX_MEM 64 * 1024

#include <ui.h>
#include <client.h>
#include <nuklear/nuklear.h>


//we may in the end include that file so save all the trouble
typedef void (*nk_wl_drawcall_t)(struct nk_context *ctx, float width, float height, struct app_surface *app);
typedef void (*nk_wl_postcall_t)(struct app_surface *app);

#ifndef NK_MAX_CMD_SIZE
#define NK_MAX_CMD_SIZE (sizeof (union {	\
	struct nk_command_scissor s;		\
	struct nk_command_line l;		\
	struct nk_command_curve q;		\
	struct nk_command_rect r;		\
	struct nk_command_rect_filled rf;	\
	struct nk_command_rect_multi_color rmc; \
	struct nk_command_triangle t;		\
	struct nk_command_triangle_filled tf;	\
	struct nk_command_circle c;		\
	struct nk_command_circle_filled cf;	\
	struct nk_command_arc a;		\
	struct nk_command_arc_filled af;	\
	struct nk_command_polygon pg;		\
	struct nk_command_polygon_filled pgf;	\
	struct nk_command_polyline pl;		\
	struct nk_command_image im;		\
	struct nk_command_text txt;		\
	struct nk_command_custom cus;		\
	}) )
typedef unsigned char nk_max_cmd_t[NK_MAX_CMD_SIZE];
#endif /* NK_MAX_CMD_SIZE */

struct nk_wl_backend {
	//I will need to make this ctx a pointer to have nk_wl_backend a
	//consistant size
	struct nk_context ctx;
	struct nk_buffer cmds;
	lua_State *L;
	//theme size
	struct nk_color main_color;
	//we now use this to determine if we are using the same theme
	nk_hash theme_hash;
	nk_rune *unicode_range;
	uint32_t row_size; //current row size for the backend

	//update to date information
	struct app_surface *app_surface;
	nk_wl_drawcall_t frame;
	nk_wl_postcall_t post_cb;
	int32_t nk_flags;


	//look
	struct {
		xkb_keysym_t ckey; //cleaned up every frame
		int32_t cbtn; //clean up every frame
		uint32_t sx;
		uint32_t sy;
	};
};

/******************************** nuklear colors ***********************************/
#include "nk_wl_theme.h"

/********************************* input ****************************************/

#include "nk_wl_input.h"

/******************************** render *******************************************/
static void nk_wl_render(struct nk_wl_backend *bkend);
static void nk_wl_resize(struct app_surface *app, const struct app_event *e);

static void
nk_wl_new_frame(struct app_surface *surf, const struct app_event *e)
{
	bool handled_input = false;
	//here is how we manage the buffer
	struct nk_wl_backend *bkend = surf->user_data;
	int width = surf->allocation.w;
	int height = surf->allocation.h;
	switch (e->type) {
	case TW_FRAME_START:
	case TW_TIMER:
		handled_input = true;
		break;
	case TW_POINTER_MOTION:
		nk_pointron(surf, e);
		handled_input = true;
		break;
	case TW_POINTER_BTN:
		nk_pointrbtn(surf, e);
		handled_input = true;
		break;
	case TW_POINTER_AXIS:
		nk_pointraxis(surf, e);
		handled_input = true;
		break;
	case TW_KEY_BTN:
		nk_keycb(surf, e);
		handled_input = true;
		break;
	case TW_RESIZE:
		handled_input = true;
		nk_wl_resize(surf, e);
		break;
	default:
		break;
	}
	if (!handled_input)
		return;

	if (bkend->nk_flags == -1) {
		bkend->frame(&bkend->ctx, width, height, bkend->app_surface);
	} else {
		if (nk_begin(&bkend->ctx, "cairo_app", nk_rect(0, 0, width, height),
			     bkend->nk_flags)) {
			bkend->frame(&bkend->ctx, width, height, bkend->app_surface);
		} nk_end(&bkend->ctx);
	}

	nk_wl_render(bkend);
	nk_clear(&bkend->ctx);

	if (bkend->post_cb) {
		bkend->post_cb(bkend->app_surface);
		bkend->post_cb = NULL;
	}
	//we will need to remove this
	bkend->ckey = XKB_KEY_NoSymbol;
	bkend->cbtn = -1;
}

static inline bool
nk_wl_need_redraw(struct nk_wl_backend *bkend)
{
	static nk_max_cmd_t nk_last_draw[100] = {0};
	void *cmds = nk_buffer_memory(&bkend->ctx.memory);
	bool need_redraw = memcmp(cmds, nk_last_draw, bkend->ctx.memory.allocated);
	if (need_redraw)
		memcpy(nk_last_draw, cmds, bkend->ctx.memory.allocated);
	return need_redraw;
}

//this function has side effects
static inline bool
nk_wl_maybe_skip(struct nk_wl_backend *bkend)
{
	bool need_redraw = nk_wl_need_redraw(bkend);
	bool need_commit = need_redraw || bkend->app_surface->need_animation;
	if (!need_commit)
		return true;
	if (!need_redraw) {
		wl_surface_commit(bkend->app_surface->wl_surface);
		return true;
	}
	return false;
}


/********************************* setup *******************************************/
static void
nk_wl_impl_app_surface(struct app_surface *surf, struct nk_wl_backend *bkend,
		       nk_wl_drawcall_t draw_cb, const struct bbox box,
		       int32_t flags)
{
	surf->allocation = box;
	surf->pending_allocation = box;
	surf->user_data = bkend;
	surf->do_frame = nk_wl_new_frame;
	//change the current state of the backend
	bkend->frame = draw_cb;
	bkend->cbtn = -1;
	bkend->ckey = XKB_KEY_NoSymbol;
	bkend->app_surface = surf;
	bkend->post_cb = NULL;
	bkend->nk_flags = flags;

	wl_surface_set_buffer_scale(surf->wl_surface, box.s);

	if (surf->wl_globals) {
		nk_wl_apply_color(bkend, &surf->wl_globals->theme);
		//TODO try to apply the font here as well.
	}
}

static void
nk_wl_clean_app_surface(struct nk_wl_backend *bkend)
{
	bkend->frame = NULL;
	bkend->cbtn = -1;
	bkend->ckey = XKB_KEY_NoSymbol;
	bkend->app_surface->do_frame = NULL;
	bkend->app_surface->user_data = NULL;
	bkend->app_surface = NULL;

}

/********************************* shared_api *******************************************/
/* NK_API struct nk_image nk_wl_load_image(const char *path); */

/* NK_API void nk_wl_free_image(struct nk_image *img); */


NK_API xkb_keysym_t
nk_wl_get_keyinput(struct nk_context *ctx)
{
	struct nk_wl_backend *bkend = container_of(ctx, struct nk_wl_backend, ctx);
	return bkend->ckey;
}

NK_API bool
nk_wl_get_btn(struct nk_context *ctx, uint32_t *button, uint32_t *sx, uint32_t *sy)
{
	struct nk_wl_backend *bkend = container_of(ctx, struct nk_wl_backend, ctx);
	*button = (bkend->cbtn >= 0) ? bkend->cbtn : NK_BUTTON_MAX;
	*sx = bkend->sx;
	*sy = bkend->sy;
	return (bkend->cbtn) >= 0;
}

NK_API void
nk_wl_add_idle(struct nk_context *ctx, nk_wl_postcall_t task)
{
	struct nk_wl_backend *bkend = container_of(ctx, struct nk_wl_backend, ctx);
	bkend->post_cb = task;
}

NK_API const struct nk_style *
nk_wl_get_curr_style(struct nk_wl_backend *bkend)
{
	return &bkend->ctx.style;
}

NK_API void
nk_wl_test_draw(struct nk_wl_backend *bkend, struct app_surface *app, nk_wl_drawcall_t draw_call)
{
	//here is how we manage the buffer
	int width = app->allocation.w;
	int height = app->allocation.h;

	if (nk_begin(&bkend->ctx, "cairo_app", nk_rect(0, 0, width, height),
		     NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
		draw_call(&bkend->ctx, width, height, app);
	} nk_end(&bkend->ctx);

	nk_clear(&bkend->ctx);
	bkend->post_cb = NULL;
	bkend->ckey = XKB_KEY_NoSymbol;
	bkend->cbtn = -1;
}


/********************************* unicode ****************************************/

#ifdef NK_INCLUDE_FONT_BAKING
/*
static inline void
union_unicode_range(const nk_rune left[2], const nk_rune right[2], nk_rune out[2])
{
	nk_rune tmp[2];
	tmp[0] = left[0] < right[0] ? left[0] : right[0];
	tmp[1] = left[1] > right[1] ? left[1] : right[1];
	out[0] = tmp[0];
	out[1] = tmp[1];
}

//return true if left and right are insersected, else false
static inline bool
intersect_unicode_range(const nk_rune left[2], const nk_rune right[2])
{
	return (left[0] <= right[1] && left[1] >= right[1]) ||
		(left[0] <= right[0] && left[1] >= right[0]);
}

static inline int
unicode_range_compare(const void *l, const void *r)
{
	const nk_rune *range_left = (const nk_rune *)l;
	const nk_rune *range_right = (const nk_rune *)r;
	return ((int)range_left[0] - (int)range_right[0]);
}

static int
merge_unicode_range(const nk_rune *left, const nk_rune *right, nk_rune *out)
{
	//get the range
	int left_size = 0;
	while(*(left+left_size)) left_size++;
	int right_size = 0;
	while(*(right+right_size)) right_size++;
	//sort the range,
	nk_rune sorted_ranges[left_size+right_size];
	memcpy(sorted_ranges, left, sizeof(nk_rune) * left_size);
	memcpy(sorted_ranges+left_size, right, sizeof(nk_rune) * right_size);
	qsort(sorted_ranges, (left_size+right_size)/2, sizeof(nk_rune) * 2,
	      unicode_range_compare);
	//merge algorithm
	nk_rune merged[left_size+right_size+1];
	merged[0] = sorted_ranges[0];
	merged[1] = sorted_ranges[1];
	int m = 0;
	for (int i = 0; i < (left_size+right_size) / 2; i++) {
		if (intersect_unicode_range(&sorted_ranges[i*2],
					    &merged[2*m]))
			union_unicode_range(&sorted_ranges[i*2], &merged[2*m],
					    &merged[2*m]);
		else {
			m++;
			merged[2*m] = sorted_ranges[2*i];
			merged[2*m+1] = sorted_ranges[2*i+1];
		}
	}
	m++;
	merged[2*m] = 0;

	if (!out)
		return 2*m;
	memcpy(out, merged, (2*m+1) * sizeof(nk_rune));
	return 2*m;
}
*/
#endif /* NK_INCLUDE_FONT_BAKING */


#ifdef __cplusplus
}
#endif


#endif /* EOF */

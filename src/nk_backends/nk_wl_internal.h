/*
 * nk_wl_internal.h - taiwins client nuklear shared implementation functions
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

#include <sequential.h>
#include <ui.h>
#include <client.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#elif defined (__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#pragma clang diagnostic ignored "-Wunused-function"
#endif

//this will make our struct various size, so lets put the buffer in the end
#define NK_MAX_CTX_MEM 64 * 1024
#include <nk_backends.h>


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
	//theme size
	struct nk_color main_color;
	//we now use this to determine if we are using the same theme
	nk_hash theme_hash;
	nk_rune *unicode_range;
	uint32_t row_size; //current row size for the backend
	//resources
	struct wl_list images;
	struct wl_list fonts;

	//look
	struct {
		//update to date information
		struct tw_appsurf *app_surface;
		nk_wl_drawcall_t frame;
		nk_wl_postcall_t post_cb;
		char *internal_clipboard;

		xkb_keysym_t ckey; //cleaned up every frame
		int32_t cbtn; //clean up every frame
		uint32_t sx;
		uint32_t sy;
	};
};

/***************************** nuklear colors *********************************/
#include "nk_wl_theme.h"

/******************************** image ***************************************/

#include "nk_wl_image.h"

/******************************** input ***************************************/

#include "nk_wl_input.h"

/******************************* wl_shell *************************************/

#include "nk_wl_shell.h"

/***************************** nk_wl_font *************************************/

#include "nk_wl_font.h"

/****************************** render ****************************************/


static void nk_wl_render(struct nk_wl_backend *bkend);
static void nk_wl_resize(struct tw_appsurf *app, const struct tw_app_event *e);

static void
nk_wl_new_frame(struct tw_appsurf *surf, const struct tw_app_event *e)
{
	bool handled_input = false;
	//here is how we manage the buffer
	struct nk_wl_backend *bkend = surf->user_data;
	int width = surf->allocation.w;
	int height = surf->allocation.h;
	int nk_flags = 0;
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
	case TW_PASTE:
		nk_wl_copyto_clipboard(surf, e);
		handled_input = true;
		break;
	default:
		break;
	}
	if (!handled_input)
		return;
	if ((surf->flags & TW_APPSURF_COMPOSITE))
		bkend->frame(&bkend->ctx, width, height, bkend->app_surface);
	else {
		switch (surf->type) {
		case TW_APPSURF_BACKGROUND:
			break;
		case TW_APPSURF_PANEL:
		case TW_APPSURF_LOCKER:
			nk_flags = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR;
			break;
		case TW_APPSURF_WIDGET:
			nk_flags = NK_WINDOW_BORDER | NK_WINDOW_SCROLL_AUTO_HIDE;
			break;
		case TW_APPSURF_APP:
			nk_flags = NK_WINDOW_BORDER | NK_WINDOW_CLOSABLE |
			NK_WINDOW_TITLE | NK_WINDOW_SCROLL_AUTO_HIDE;
			break;
		}
		if (nk_begin(&bkend->ctx, "nuklear_app", nk_rect(0, 0, width, height),
			     nk_flags)) {
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
nk_wl_impl_app_surface(struct tw_appsurf *surf, struct nk_wl_backend *bkend,
		       nk_wl_drawcall_t draw_cb, const struct tw_bbox box)
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
	bkend->ctx.clip.copy = nk_wl_clipboard_copy;
	bkend->ctx.clip.paste = nk_wl_clipboard_paste;
	bkend->ctx.clip.userdata = nk_handle_ptr(surf);

	wl_surface_set_buffer_scale(surf->wl_surface, box.s);

	if (surf->tw_globals)
		nk_wl_apply_color(bkend, &surf->tw_globals->theme);
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
	if (bkend->internal_clipboard)
		free(bkend->internal_clipboard);
}

static inline void
nk_wl_release_resources(struct nk_wl_backend *bkend)
{
	struct nk_wl_user_font *font, *tmp_font = NULL;
	struct nk_wl_image *image, *tmp_img = NULL;
	wl_list_for_each_safe(image, tmp_img, &bkend->images, link)
		nk_wl_free_image(&image->image);
	wl_list_for_each_safe(font, tmp_font, &bkend->fonts, link)
		nk_wl_destroy_font(&font->user_font);
}

static inline void
nk_wl_backend_cleanup(struct nk_wl_backend *bkend)
{
	nk_wl_release_resources(bkend);
	nk_buffer_free(&bkend->cmds);
	nk_free(&bkend->ctx);
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
nk_wl_test_draw(struct nk_wl_backend *bkend, struct tw_appsurf *app, nk_wl_drawcall_t draw_call)
{
	//here is how we manage the buffer
	int width = app->allocation.w;
	int height = app->allocation.h;

	if (nk_begin(&bkend->ctx, "nk_app", nk_rect(0, 0, width, height),
		     NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
		draw_call(&bkend->ctx, width, height, app);
	} nk_end(&bkend->ctx);

	nk_clear(&bkend->ctx);
	bkend->post_cb = NULL;
	bkend->ckey = XKB_KEY_NoSymbol;
	bkend->cbtn = -1;
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined (__clang__)
#pragma clang diagnostic pop
#endif

#ifdef __cplusplus
}
#endif


#endif /* EOF */

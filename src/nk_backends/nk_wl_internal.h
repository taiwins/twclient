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

#define NK_IMPLEMENTATION

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

#ifdef NK_API
#undef NK_API
#endif
#define NK_API WL_EXPORT

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#elif defined (__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wmaybe-uninitialized"
#endif

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
	struct nk_context ctx;
	vector_t prev_cmds;
	//theme size
	struct nk_color main_color;
	//we now use this to determine if we are using the same theme
	nk_hash theme_hash;

	//resources
	struct wl_list images;
	struct wl_list fonts;

	//look
	struct {
		//update to date information
		struct tw_appsurf *app_surface;
		nk_wl_drawcall_t frame;
		char *internal_clipboard;

		xkb_keysym_t ckey; //cleaned up every frame
		int32_t cbtn; //clean up every frame
		uint32_t sx;
		uint32_t sy;
	};
};

/******************************** image ***************************************/

#include "nk_wl_image.h"

/***************************** nuklear colors *********************************/
#include "nk_wl_theme.h"

/******************************** input ***************************************/

#include "nk_wl_input.h"

/******************************* wl_shell *************************************/

#include "nk_wl_shell.h"

/***************************** nk_wl_font *************************************/

#include "nk_wl_font.h"

/****************************** render ****************************************/


int NK_WL_DEBUG_CMDS(struct nk_context *ctx, const struct nk_command *c) {
	for ((c) = nk__begin(ctx); (c) != 0; (c) = nk__next(ctx, c))
		fprintf(stderr, "cmd: type: %d, next: %ld\n", c->type, c->next);
	return 0;
}

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
		if (surf->flags & TW_APPSURF_NOINPUT)
			nk_flags = nk_flags | NK_WINDOW_NO_INPUT;
		if (nk_begin(&bkend->ctx, "nuklear_app",
		             nk_rect(0, 0, width, height),
			     nk_flags)) {
			bkend->frame(&bkend->ctx, width, height, bkend->app_surface);
		} nk_end(&bkend->ctx);
	}

	nk_wl_render(bkend);
	nk_clear(&bkend->ctx);

	//we will need to remove this
	bkend->ckey = XKB_KEY_NoSymbol;
	bkend->cbtn = -1;
}

static bool
nk_wl_need_redraw(struct nk_wl_backend *bkend)
{
	struct nk_buffer *cmds;
	bool need_redraw = true;
	bool need_resize = false;
	unsigned size = (unsigned)bkend->prev_cmds.len;

	cmds = &bkend->ctx.memory;

	need_resize = size < cmds->allocated;
	if (need_resize)
		vector_resize(&bkend->prev_cmds,
		              cmds->allocated);

	need_redraw = need_resize ||
		memcmp(nk_buffer_memory(cmds), bkend->prev_cmds.elems,
		       cmds->allocated);
	if (need_redraw)
		memcpy(bkend->prev_cmds.elems,
		       nk_buffer_memory(cmds),
		       cmds->allocated);
	return need_redraw;
}

//this function has side effects
static inline bool
nk_wl_maybe_skip(struct nk_wl_backend *bkend)
{
	const struct nk_command *cmd;
	bool need_redraw = nk_wl_need_redraw(bkend);
	if (!need_redraw) {
		//TODO there is something going
		//on with nk_foreach, it seems you
		//have to call it.
		nk_foreach(cmd, &bkend->ctx);
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
	bkend->ctx.clip.copy = nk_wl_clipboard_copy;
	bkend->ctx.clip.paste = nk_wl_clipboard_paste;
	bkend->ctx.clip.userdata = nk_handle_ptr(surf);
	nk_wl_input_reset(&bkend->ctx);

	wl_surface_set_buffer_scale(surf->wl_surface, box.s);

	//using hash for applying themes
	if (surf->tw_globals) {
		nk_hash new_hash = 0;
		struct tw_globals *globals = surf->tw_globals;

		if (globals->theme)
			new_hash = nk_wl_hash_theme(globals->theme);

		if (new_hash == bkend->theme_hash)
			return;
		else if (globals->theme) {
			nk_wl_apply_theme(bkend, globals->theme);
			bkend->theme_hash = new_hash;
		}
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
	vector_resize(&bkend->prev_cmds, 0);
	//re initialize the backend inputs
	nk_wl_input_reset(&bkend->ctx);

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

static void
nk_wl_backend_init(struct nk_wl_backend *bkend)
{
	memset(bkend, 0, sizeof(struct nk_wl_backend));
	wl_list_init(&bkend->images);
	wl_list_init(&bkend->fonts);

	bkend->theme_hash = 0;
	bkend->cbtn = -1;
	//we are not including font here, need to be setup from implementation
	nk_init_default(&bkend->ctx, NULL);
	vector_init_zero(&bkend->prev_cmds, 1, NULL);

}

static inline void
nk_wl_backend_cleanup(struct nk_wl_backend *bkend)
{
	nk_wl_release_resources(bkend);
	vector_destroy(&bkend->prev_cmds);
	nk_free(&bkend->ctx);
}


/*******************************************************************************
 * shared API
 ******************************************************************************/

WL_EXPORT const struct nk_style *
nk_wl_get_curr_style(struct nk_wl_backend *bkend)
{
	return &bkend->ctx.style;
}

WL_EXPORT void
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

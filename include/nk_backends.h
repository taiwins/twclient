/*
 * nk_backends.h - taiwins client nuklear backends header
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

#ifndef NK_BACKENDS_H
#define NK_BACKENDS_H

#include <stdbool.h>
#include <stdint.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "ui.h"

struct nk_wl_backend;
struct nk_context;
struct nk_style;

#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS

#include "nuklear/nuklear.h"
struct xdg_toplevel;
struct xdg_surface;

typedef void (*nk_wl_drawcall_t)(struct nk_context *ctx, float width, float height, struct tw_appsurf *app);
typedef void (*nk_wl_postcall_t)(struct tw_appsurf *app);

/*******************************************************************************
 * shell implementation
 ******************************************************************************/
NK_API struct xdg_toplevel *
nk_wl_impl_xdg_shell_surface(struct tw_appsurf *app,
                             struct xdg_surface *xdg_surface);

NK_API void
nk_wl_impl_wl_shell_surface(struct tw_appsurf *app,
                            struct wl_shell_surface *protocol);

/*******************************************************************************
 * backends
 ******************************************************************************/

/* cairo_backend */
struct nk_wl_backend *
nk_cairo_create_bkend(void);

void
nk_cairo_destroy_bkend(struct nk_wl_backend *bkend);

void
nk_cairo_impl_app_surface(struct tw_appsurf *surf, struct nk_wl_backend *bkend,
                          nk_wl_drawcall_t draw_cb,  const struct tw_bbox geo);


/* egl_backend */
struct nk_wl_backend*
nk_egl_create_backend(const struct wl_display *display);

void
nk_egl_destroy_backend(struct nk_wl_backend *b);

void
nk_egl_impl_app_surface(struct tw_appsurf *surf, struct nk_wl_backend *bkend,
                        nk_wl_drawcall_t draw_cb, const struct tw_bbox geo);


/* vulkan_backend */
/* struct nk_wl_backend *nk_vulkan_backend_create(void); */
/* struct nk_wl_backend *nk_vulkan_backend_clone(struct nk_wl_backend *b); */
/* void nk_vulkan_backend_destroy(struct nk_wl_backend *b); */
/* void */
/* nk_vulkan_impl_app_surface(struct tw_appsurf *surf, struct nk_wl_backend *bkend, */
/*			   nk_wl_drawcall_t draw_cb, const struct tw_bbox geo); */


NK_API xkb_keysym_t
nk_wl_get_keyinput(struct nk_context *ctx);

NK_API bool
nk_wl_get_btn(struct nk_context *ctx, uint32_t *button, uint32_t *sx, uint32_t *sy);

NK_API void
nk_wl_add_idle(struct nk_context *ctx, nk_wl_postcall_t task);


NK_API const struct nk_style *
nk_wl_get_curr_style(struct nk_wl_backend *bkend);

NK_API void
nk_wl_test_draw(struct nk_wl_backend *bkend, struct tw_appsurf *app,
		nk_wl_drawcall_t draw_call);


/*******************************************************************************
 * image loader
 ******************************************************************************/

NK_API struct nk_image *
nk_wl_load_image(const char *path, enum wl_shm_format format,
                 struct nk_wl_backend *b);

NK_API struct nk_image *
nk_wl_add_image(struct nk_image img, struct nk_wl_backend *b);

NK_API void
nk_wl_free_image(struct nk_image *img);


/*******************************************************************************
 * font loader
 ******************************************************************************/

enum nk_wl_font_slant {
	NK_WL_SLANT_ROMAN,
	NK_WL_SLANT_ITALIC,
	NK_WL_SLANT_OBLIQUE,
};

//we accept only true type font
struct nk_wl_font_config {
	const char *name;
	enum nk_wl_font_slant slant;
	int pix_size, scale;
	int nranges;
	const nk_rune **ranges;

	//fc config offers light, medium, demibold, bold, black
	//demibold, bold and black is true, otherwise false.
	bool bold; //classified as bold
	//private: TTF only
	bool TTFonly;
};


NK_API struct nk_user_font *
nk_wl_new_font(struct nk_wl_font_config config,
               struct nk_wl_backend *backend);

NK_API void
nk_wl_destroy_font(struct nk_user_font *font);

#endif

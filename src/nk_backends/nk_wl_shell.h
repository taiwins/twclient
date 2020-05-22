/*
 * nk_wl_shell.h - taiwins client nuklear shell functions
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

#ifndef NK_WL_SHELL_IMPL_H
#define NK_WL_SHELL_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <wayland-xdg-shell-client-protocol.h>
#include "nk_wl_internal.h"

static void
nk_wl_shell_handle_ping(void *data,
		     struct wl_shell_surface *wl_shell_surface,
		     uint32_t serial)
{
	wl_shell_surface_pong(wl_shell_surface, serial);
}


static void
nk_wl_shell_handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static void
nk_wl_shell_handle_configure(void *data, struct wl_shell_surface *shell_surface,
		 uint32_t edges, int32_t width, int32_t height)
{
	struct tw_appsurf *app = data;
	tw_appsurf_resize(app, width, height, app->allocation.s);
}

static struct wl_shell_surface_listener nk_wl_shell_impl = {
	.ping = nk_wl_shell_handle_ping,
	.configure = nk_wl_shell_handle_configure,
	.popup_done = nk_wl_shell_handle_popup_done
};


WL_EXPORT void
nk_wl_impl_wl_shell_surface(struct tw_appsurf *app,
			    struct wl_shell_surface *protocol)
{
	wl_shell_surface_add_listener(protocol,
				      &nk_wl_shell_impl, app);
}

static void
nk_wl_impl_xdg_configure(void *data,
			 struct xdg_toplevel *xdg_toplevel,
			 int32_t width,
			 int32_t height,
			 struct wl_array *states)
{
	struct tw_appsurf *app = data;

	if (!width || !height)
		return;
	tw_appsurf_resize(app, width, height, app->allocation.s);
}

static void
nk_wl_xdg_toplevel_close(void *data,
			 struct xdg_toplevel *xdg_toplevel)
{
	struct xdg_surface *xdg_surface = data;
	struct tw_appsurf *app =
		xdg_surface_get_user_data(xdg_surface);

	xdg_toplevel_destroy(xdg_toplevel);
	xdg_surface_destroy(xdg_surface);
	tw_appsurf_release(app);
}

static struct xdg_toplevel_listener nk_wl_xdgtoplevel_impl =  {
	.configure = nk_wl_impl_xdg_configure,
	.close = nk_wl_xdg_toplevel_close,
};


WL_EXPORT void
nk_wl_impl_xdg_toplevel(struct tw_appsurf *app,
                        struct xdg_toplevel *toplevel)
{
	xdg_toplevel_add_listener(toplevel, &nk_wl_xdgtoplevel_impl,
	                          app);
}

#ifdef __cplusplus
}
#endif

#endif /* EOF */

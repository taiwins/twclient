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
	struct app_surface *app = data;
	app_surface_resize(app, width, height, app->allocation.s);
}

static struct wl_shell_surface_listener nk_wl_shell_impl = {
	.ping = nk_wl_shell_handle_ping,
	.configure = nk_wl_shell_handle_configure,
	.popup_done = nk_wl_shell_handle_popup_done
};


NK_API void
nk_wl_impl_wl_shell_surface(struct app_surface *app,
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

	struct xdg_surface *xdg_surface = data;
	struct app_surface *app =
		xdg_surface_get_user_data(xdg_surface);
	app_surface_resize(app, width, height, app->allocation.s);
	/* xdg_top */
	xdg_surface_ack_configure(xdg_surface,
				  app->wl_globals->inputs.serial);
}

static void
nk_wl_xdg_toplevel_close(void *data,
			 struct xdg_toplevel *xdg_toplevel)
{
	struct xdg_surface *xdg_surface = data;
	struct app_surface *app =
		xdg_surface_get_user_data(xdg_surface);

	xdg_toplevel_destroy(xdg_toplevel);
	xdg_surface_destroy(xdg_surface);
	app_surface_release(app);
}

static void
nk_wl_xdg_surface_handle_configure(void *data,
				   struct xdg_surface *xdg_surface,
				   uint32_t serial)
{
	struct app_surface *app = data;
	app->wl_globals->inputs.serial = serial;
}

static struct xdg_toplevel_listener nk_wl_xdgtoplevel_impl =  {
	.configure = nk_wl_impl_xdg_configure,
	.close = nk_wl_xdg_toplevel_close,
};

static struct xdg_surface_listener nk_wl_xdgsurface_impl = {
	.configure = nk_wl_xdg_surface_handle_configure,
};

NK_API struct xdg_toplevel *
nk_wl_impl_xdg_shell_surface(struct app_surface *app,
			     struct xdg_surface *xdg_surface)
{
	struct xdg_toplevel *toplevel = xdg_surface_get_toplevel(
		xdg_surface);
	xdg_surface_add_listener(xdg_surface,
				 &nk_wl_xdgsurface_impl, app);
	xdg_toplevel_add_listener(toplevel, &nk_wl_xdgtoplevel_impl,
				  xdg_surface);
	return toplevel;
}

#ifdef __cplusplus
}
#endif

#endif /* EOF */

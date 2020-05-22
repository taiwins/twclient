#include <stdio.h>
#include <wayland-client.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <twclient/ui.h>
#include <twclient/egl.h>
#include <twclient/client.h>
#include <twclient/nk_backends.h>
#include <twclient/theme.h>
#include <wayland-xdg-shell-client-protocol.h>

/* #define STB_IMAGE_IMPLEMENTATION */
/* #include "stb_image.h" */


//create a egl ui

static struct application {
	struct wl_shell *shell;
	struct tw_globals global;
	struct tw_appsurf surface;
	struct nk_wl_backend *bkend;
	struct xdg_wm_base *base;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *toplevel;
	struct wl_shell_surface *shell_surface;
	bool done;
	bool configured;
	struct nk_image image;
	struct nk_user_font *user_font;
	struct tw_theme theme;
} App;

static void
sample_widget(struct nk_context *ctx, float width, float height, struct tw_appsurf *data)
{
	struct application *app = &App;
	nk_style_set_font(ctx, app->user_font);

	enum {EASY, HARD};

	static struct nk_text_edit text_edit;
	static bool inanimation = false;
	static bool init_text_edit = false;
	static char text_buffer[256];
	static struct nk_colorf bg;
	bool last_frame = inanimation;
	static int slider = 10;

	if (!init_text_edit) {
		init_text_edit = true;
		nk_textedit_init_fixed(&text_edit, text_buffer, 256);
	}

	int unicodes[] = {0xf1c1, 'C', 0xf1c2, 0xf1c3, 0xf1c4, 0xf1c5};
	char strings[256];
	int count = 0;
	for (int i = 0; i < 5; i++) {
		count += nk_utf_encode(unicodes[i], strings+count, 256-count);
	}
	strings[count] = '\0';

	nk_layout_row_static(ctx, 30, 80, 2);
	inanimation = (nk_button_label(ctx, strings)) ? !inanimation : inanimation;
	if (inanimation && !last_frame)
		tw_appsurf_request_frame(data);
	else if (!inanimation)
		tw_appsurf_end_frame_request(data);

	/* nk_button_label(ctx, strings); */
	nk_slider_int(ctx, 10, &slider, 20, 1);
	nk_layout_row_dynamic(ctx, 50, 1);
	bg = nk_color_picker(ctx, bg, NK_RGB);
	nk_layout_row_dynamic(ctx, 25, 1);
	bg.r = nk_propertyf(ctx, "#R", 0, bg.r, 1.0f, 0.01f, 0.005f);
	bg.g = nk_propertyf(ctx, "#G:", 0, bg.g, 1.0f, 0.01f,0.005f);
	bg.b = nk_propertyf(ctx, "#B:", 0, bg.b, 1.0f, 0.01f,0.005f);
	bg.a = nk_propertyf(ctx, "#A:", 0, bg.a, 1.0f, 0.01f,0.005f);

	nk_layout_row_dynamic(ctx, 25, 1);
	if (nk_button_label(ctx, "quit")) {
		App.global.event_queue.quit = true;
	}
}

static void
xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                      uint32_t serial)
{
	struct application *_app =
		wl_container_of(data, _app, surface);
	struct tw_appsurf *app = data;
	app->tw_globals->inputs.serial = serial;
	xdg_surface_ack_configure(xdg_surface, serial);

	if (!_app->configured)
		_app->configured = true;
}

static struct xdg_surface_listener xdg_impl = {
	.configure = xdg_surface_configure,
};

static void
xdg_wm_ping(void *data,
            struct xdg_wm_base *xdg_wm_base,
            uint32_t serial)
{
	struct application *app = data;
	xdg_wm_base_pong(app->base, serial);
}

static struct xdg_wm_base_listener xdg_wm_impl = {
	.ping = xdg_wm_ping,
};

static void
global_registry_handler(void *data,
			struct wl_registry *registry, uint32_t id,
			const char *interface, uint32_t version)
{
	if (strcmp(interface, wl_shell_interface.name) == 0) {
		App.shell = wl_registry_bind(registry, id, &wl_shell_interface, version);
		fprintf(stdout, "wl_shell %d announced\n", id);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		App.base = wl_registry_bind(registry, id, &xdg_wm_base_interface,
		                            version);
		xdg_wm_base_add_listener(App.base, &xdg_wm_impl, &App);
		fprintf(stdout, "xdg_wm_base %d is now available\n", id);
	} else {
		tw_globals_announce(&App.global, registry, id, interface, version);
	}
}

static void
global_registry_removal(void *data, struct wl_registry *registry, uint32_t id)
{
	fprintf(stdout, "wl_registry removes global! %d\n", id);
}


static struct wl_registry_listener registry_listener = {
	.global = global_registry_handler,
	.global_remove = global_registry_removal,
};



int main(int argc, char *argv[])
{
	const nk_rune basic_range[] = {0x0020, 0x00ff, 0};
	const nk_rune pua_range[] = {0xe000, 0xf8ff, 0};
	const nk_rune *ranges[] = {
		basic_range,
		pua_range,
	};

        const struct nk_wl_font_config config = {
	        .name = "icons",
	        .slant = NK_WL_SLANT_ROMAN,
	        .pix_size = 16,
	        .scale = 1,
	        .TTFonly = true,
	        .nranges = 2,
	        .ranges = (const nk_rune **)ranges,
        };

        struct wl_display *wl_display = wl_display_connect(NULL);
        if (!wl_display)
		fprintf(stderr, "okay, I don't even have wayland display\n");
	tw_globals_init(&App.global, wl_display);
	tw_theme_init_default(&App.theme);
	App.global.theme = &App.theme;

	struct wl_registry *registry = wl_display_get_registry(wl_display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	App.done = false;

	wl_display_dispatch(wl_display);
	wl_display_roundtrip(wl_display);

	struct wl_surface *wl_surface = wl_compositor_create_surface(App.global.compositor);
	struct xdg_surface *xdg_surface = xdg_wm_base_get_xdg_surface(App.base, wl_surface);
	struct xdg_toplevel *toplevel = xdg_surface_get_toplevel(xdg_surface);
	tw_appsurf_init_default(&App.surface, wl_surface,
			 &App.global);

	xdg_surface_add_listener(xdg_surface, &xdg_impl, &App.surface);
	nk_wl_impl_xdg_toplevel(&App.surface, toplevel);
	App.xdg_surface = xdg_surface;
	App.toplevel = toplevel;

	App.bkend = nk_egl_create_backend(wl_display);
	App.user_font = nk_wl_new_font(config, App.bkend);

	nk_egl_impl_app_surface(&App.surface, App.bkend, sample_widget,
	                        tw_make_bbox_origin(200, 400, 2));

	// wait until configure is done
	// the configure here is important for us to trigger configure events.
	wl_surface_commit(wl_surface);
	wl_display_flush(wl_display);
	while (!App.configured)
		wl_display_dispatch(wl_display);

	tw_appsurf_frame(&App.surface, false);

	tw_globals_dispatch_event_queue(&App.global);

	xdg_toplevel_destroy(toplevel);
	xdg_surface_destroy(xdg_surface);
	tw_appsurf_release(&App.surface);
	nk_egl_destroy_backend(App.bkend);
	tw_globals_release(&App.global);
	wl_registry_destroy(registry);
	wl_display_disconnect(wl_display);
	return 0;
}

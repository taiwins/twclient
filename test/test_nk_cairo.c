#include <twclient/client.h>
#include <twclient/ui.h>
#include <twclient/nk_backends.h>
#include <twclient/theme.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>
#include <wayland-xdg-shell-client-protocol.h>

struct nk_wl_bkend;
struct nk_context;
struct nk_wl_backend * nk_cairo_create_bkend(void);
void nk_cairo_destroy_bkend(struct nk_wl_backend *bkend);

typedef void (*nk_wl_drawcall_t)(struct nk_context *ctx,
                                 float width, float height,
                                 struct tw_appsurf *app);

static struct application {
	struct wl_shell *shell;
	struct xdg_wm_base *base;
	struct tw_globals global;
	struct tw_appsurf surface;
	struct nk_wl_backend *bkend;
	struct wl_shell_surface *shell_surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *toplevel;
	bool configured;
	bool done;
	struct nk_image image;
	struct nk_user_font *user_font;
	struct tw_theme theme;
} App;

void
sample_widget(struct nk_context *ctx, float width, float height, struct tw_appsurf *data)
{
	enum {EASY, HARD};
	static struct nk_text_edit text_edit;
	static bool init_text_edit = false;
	static char text_buffer[256];
	static int op = EASY;
	int unicodes[] = {0xf1c1, 'C', 0xf1c2, 0xf1c3, 0xf1c4, 0xf1c5};
	char strings[256];
	int count = 0;

	nk_style_set_font(ctx, App.user_font);

	if (!init_text_edit) {
		init_text_edit = true;
		nk_textedit_init_fixed(&text_edit, text_buffer, 256);
	}

	for (int i = 0; i < 5; i++) {
		count += nk_utf_encode(unicodes[i], strings+count, 256-count);
	}
	strings[count] = '\0';

	if (nk_begin(ctx, "demo0", nk_rect(0,0, width/2, height/2),
		     NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
		nk_layout_row_dynamic(ctx, 30, 1);
		nk_edit_buffer(ctx, NK_EDIT_FIELD, &text_edit, nk_filter_default);

	} nk_end(ctx);

	if (nk_begin(ctx, "demo1", nk_rect(width/2, 0, width/2, height/2),
		     NK_WINDOW_BORDER)) {
		nk_layout_row_dynamic(ctx, 30, 2);
		if (nk_option_label(ctx, "easy", op == EASY)) op = EASY;
		if (nk_option_label(ctx, "hard", op == HARD)) op = HARD;
		nk_layout_row_dynamic(ctx, 30, 1);
		if (nk_button_label(ctx, strings)) {
			App.global.event_queue.quit = true;
		}
	} nk_end(ctx);

	if (nk_begin(ctx, "demo2", nk_rect(0, height/2, width, height/2),
		     NK_WINDOW_BORDER)) {
		nk_layout_row_dynamic(ctx, 40, 1);
		//one way data-binding
		nk_label(ctx, op == EASY ? "easy" : "hard", NK_TEXT_ALIGN_LEFT);

	} nk_end(ctx);

}

void
single_widget(struct nk_context *ctx, float width, float height, struct tw_appsurf *data)
{
	enum {EASY, HARD};
	//static int op = EASY;
	static struct nk_text_edit text_edit;
	static bool inanimation = false;
	static bool init_text_edit = false;
	static char text_buffer[256];
	static struct nk_colorf bg;
	bool last_frame = inanimation;

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
	nk_label(ctx, "another", NK_TEXT_LEFT);
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
        const struct nk_wl_font_config config = {
	        .name = "icons",
	        .slant = NK_WL_SLANT_ROMAN,
	        .pix_size = 16,
	        .scale = 1,
	        .TTFonly = true,
	        .nranges = 0,
	        .ranges = NULL,
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
	App.configured = false;

	wl_display_dispatch(wl_display);
	wl_display_roundtrip(wl_display);

	struct wl_surface *wl_surface = wl_compositor_create_surface(App.global.compositor);
	struct xdg_surface *xdg_surface = xdg_wm_base_get_xdg_surface(App.base, wl_surface);
	struct xdg_toplevel *toplevel = xdg_surface_get_toplevel(xdg_surface);

	tw_appsurf_init(&App.surface, wl_surface,
			 &App.global, TW_APPSURF_APP,
			 TW_APPSURF_COMPOSITE);
	App.xdg_surface = xdg_surface;
	App.toplevel = toplevel;
	xdg_surface_add_listener(xdg_surface, &xdg_impl, &App.surface);
	nk_wl_impl_xdg_toplevel(&App.surface, toplevel);

	xdg_toplevel_set_title(toplevel, "nuklear cairo test");
	xdg_toplevel_set_app_id(toplevel, "nuklear cairo test");

	App.surface.known_mimes[TW_MIME_TYPE_TEXT] = "text/";
	App.bkend = nk_cairo_create_backend();
	App.user_font = nk_wl_new_font(config, App.bkend);

	nk_cairo_impl_app_surface(&App.surface, App.bkend, sample_widget,
				  tw_make_bbox_origin(200, 400, 2));

	// wait until configure is done
	wl_surface_commit(wl_surface);
	wl_display_flush(wl_display);
	while (!App.configured)
		wl_display_dispatch(wl_display);

	//we shall wait until we got the configure event.
	tw_appsurf_frame(&App.surface, false);

	tw_globals_dispatch_event_queue(&App.global);
	xdg_toplevel_destroy(toplevel);
	xdg_surface_destroy(xdg_surface);
	tw_appsurf_release(&App.surface);
	nk_cairo_destroy_backend(App.bkend);

	tw_globals_release(&App.global);
	wl_registry_destroy(registry);
	wl_display_disconnect(wl_display);

	return 0;
}

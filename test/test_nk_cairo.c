#include <client.h>
#include <ui.h>
#include <nk_backends.h>



struct nk_wl_bkend;
struct nk_context;
struct nk_wl_backend * nk_cairo_create_bkend(void);
void nk_cairo_destroy_bkend(struct nk_wl_backend *bkend);


typedef void (*nk_wl_drawcall_t)(struct nk_context *ctx, float width, float height, struct app_surface *app);


static struct application {
	struct wl_shell *shell;
	struct wl_globals global;
	struct app_surface surface;
	struct nk_wl_backend *bkend;
	struct wl_shell_surface *shell_surface;
	bool done;
	struct nk_image image;
} App;




static void
global_registry_handler(void *data,
			struct wl_registry *registry, uint32_t id,
			const char *interface, uint32_t version)
{
	if (strcmp(interface, wl_shell_interface.name) == 0) {
		App.shell = wl_registry_bind(registry, id, &wl_shell_interface, version);
		fprintf(stdout, "wl_shell %d announced\n", id);
	} else
		wl_globals_announce(&App.global, registry, id, interface, version);
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


void
sample_widget(struct nk_context *ctx, float width, float height, struct app_surface *data)
{
	enum {EASY, HARD};
	static struct nk_text_edit text_edit;
	static bool init_text_edit = false;
	static char text_buffer[256];
	static int op = EASY;
	int unicodes[] = {0xf1c1, 'C', 0xf1c2, 0xf1c3, 0xf1c4, 0xf1c5};
	char strings[256];
	int count = 0;

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
single_widget(struct nk_context *ctx, float width, float height, struct app_surface *data)
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
		app_surface_request_frame(data);
	else if (!inanimation)
		app_surface_end_frame_request(data);

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


int main(int argc, char *argv[])
{
	struct wl_display *wl_display = wl_display_connect(NULL);
	if (!wl_display)
		fprintf(stderr, "okay, I don't even have wayland display\n");
	wl_globals_init(&App.global, wl_display);
	App.global.theme = taiwins_dark_theme;

	struct wl_registry *registry = wl_display_get_registry(wl_display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	App.done = false;
	/* App.image = nk_wl_load_image(argv[1]); */


	wl_display_dispatch(wl_display);
	wl_display_roundtrip(wl_display);

	struct wl_surface *wl_surface = wl_compositor_create_surface(App.global.compositor);
	struct wl_shell_surface *shell_surface = wl_shell_get_shell_surface(App.shell, wl_surface);
	app_surface_init(&App.surface, wl_surface,
			 (struct wl_proxy *)shell_surface, &App.global, APP_SURFACE_APP,
			 APP_SURFACE_COMPOSITE);

	nk_wl_impl_wl_shell_surface(&App.surface);
	wl_shell_surface_set_toplevel(shell_surface);
	App.shell_surface = shell_surface;

	App.bkend = nk_cairo_create_bkend();
	//the drawing is easy, but input handler is not
	nk_cairo_impl_app_surface(&App.surface, App.bkend, sample_widget,
				  make_bbox_origin(200, 400, 2));

	app_surface_frame(&App.surface, false);

	wl_globals_dispatch_event_queue(&App.global);

	app_surface_release(&App.surface);
	nk_cairo_destroy_bkend(App.bkend);

	wl_globals_release(&App.global);
	wl_registry_destroy(registry);
	wl_display_disconnect(wl_display);
	/* nk_wl_free_image(&App.image); */

	return 0;
}

#include <stdlib.h>
#include <client.h>
#include <ui.h>
#include <nk_backends.h>


static struct application {
	struct wl_shell *shell;
	struct wl_globals global;
	struct app_surface surface;
	struct nk_wl_backend *bkend;
	struct wl_shell_surface *shell_surface;
	bool done;
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

static void
sample_widget(struct nk_context *ctx, float width, float height, struct app_surface *data)
{
	//TODO, change the draw function to app->draw_widget(app);
	enum {EASY, HARD};
	static int op = EASY;
	static struct nk_text_edit text_edit;
	static bool init_text_edit = false;
	static char text_buffer[256];
	static struct nk_colorf bg;
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
	nk_button_label(ctx, "label");
	nk_label(ctx, "label", NK_TEXT_LEFT);
	nk_layout_row_dynamic(ctx, 50, 1);
	bg = nk_color_picker(ctx, bg, NK_RGB);
	nk_layout_row_dynamic(ctx, 25, 1);
	bg.r = nk_propertyf(ctx, "#R", 0, bg.r, 1.0f, 0.01f, 0.005f);
	bg.g = nk_propertyf(ctx, "#G:", 0, bg.g, 1.0f, 0.01f,0.005f);
	bg.b = nk_propertyf(ctx, "#B:", 0, bg.b, 1.0f, 0.01f,0.005f);
	bg.a = nk_propertyf(ctx, "#A:", 0, bg.a, 1.0f, 0.01f,0.005f);

	/* nk_layout_row_dynamic(ctx, 30, 2); */
	/* if (nk_option_label(ctx, "easy", op == EASY)) op = EASY; */
	/* if (nk_option_label(ctx, "hard", op == HARD)) op = HARD; */
	/* nk_layout_row_dynamic(ctx, 30, 1); */
	/* if (nk_button_label(ctx, strings)) { */
	/*	App.done = true; */
	/* } */
	/* nk_layout_row_dynamic(ctx, 30, 1); */
	/* nk_edit_buffer(ctx, NK_EDIT_FIELD, &text_edit, nk_filter_default); */
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

	wl_display_dispatch(wl_display);
	wl_display_roundtrip(wl_display);

	struct wl_surface *wl_surface = wl_compositor_create_surface(App.global.compositor);
	struct wl_shell_surface *shell_surface = wl_shell_get_shell_surface(App.shell, wl_surface);
	app_surface_init(&App.surface, wl_surface,
			 (struct wl_proxy *)shell_surface, &App.global);


	struct nk_wl_backend *backend = nk_vulkan_backend_create();
	nk_vulkan_impl_app_surface(&App.surface, backend, sample_widget, 100, 200, 0, 0);

	app_surface_release(&App.surface);
	nk_vulkan_backend_destroy(backend);

	free(backend);
	return 0;
}

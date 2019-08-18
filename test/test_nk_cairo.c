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


static void
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

static
void
handle_ping(void *data,
		     struct wl_shell_surface *wl_shell_surface,
		     uint32_t serial)
{
	fprintf(stderr, "ping!!!\n");
	wl_shell_surface_pong(wl_shell_surface, serial);
}


static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
		 uint32_t edges, int32_t width, int32_t height)
{
	fprintf(stderr, "shell_surface has configure: %d, %d, %d\n", edges, width, height);

}

struct wl_shell_surface_listener pingpong = {
	.ping = handle_ping,
	.configure = handle_configure,
	.popup_done = handle_popup_done
};



/* okay we are gonna try to set the theme for nuklear */

int main(int argc, char *argv[])
{
	/* const char *wayland_socket = getenv("WAYLAND_SOCKET"); */
	/* const int fd = atoi(wayland_socket); */
	/* FILE *log = fopen("/tmp/client-log", "w"); */
	/* if (!log) */
	/*	return -1; */
	/* fprintf(log, "%d\n", fd); */
	/* fflush(log); */
	/* fclose(log); */

	struct wl_display *wl_display = wl_display_connect(NULL);
	if (!wl_display)
		fprintf(stderr, "okay, I don't even have wayland display\n");
	wl_globals_init(&App.global, wl_display);
	App.global.theme = taiwins_dark_theme;

	struct wl_registry *registry = wl_display_get_registry(wl_display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	App.done = false;
	/* App.image = nk_wl_load_image(argv[1]); */

	struct shm_pool pool;

	wl_display_dispatch(wl_display);
	wl_display_roundtrip(wl_display);

	struct wl_surface *wl_surface = wl_compositor_create_surface(App.global.compositor);
	struct wl_shell_surface *shell_surface = wl_shell_get_shell_surface(App.shell, wl_surface);
	app_surface_init(&App.surface, wl_surface, (struct wl_proxy *)shell_surface, &App.global);

	wl_shell_surface_add_listener(shell_surface, &pingpong, NULL);
	wl_shell_surface_set_toplevel(shell_surface);
	App.shell_surface = shell_surface;

	shm_pool_init(&pool, App.global.shm, 4096, App.global.buffer_format);

	App.bkend = nk_cairo_create_bkend();
	//the drawing is easy, but input handler is not
	nk_cairo_impl_app_surface(&App.surface, App.bkend, sample_widget, &pool,
				  make_bbox_origin(200, 400, 2), -1);


	app_surface_frame(&App.surface, false);

	wl_globals_dispatch_event_queue(&App.global);

	app_surface_release(&App.surface);
	nk_cairo_destroy_bkend(App.bkend);
	shm_pool_release(&pool);

	wl_globals_release(&App.global);
	wl_registry_destroy(registry);
	wl_display_disconnect(wl_display);
	/* nk_wl_free_image(&App.image); */

	return 0;
}

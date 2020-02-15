#include <stdio.h>
#include <wayland-client.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <ui.h>
#include <egl.h>
#include <client.h>
#include <nk_backends.h>
/* #define STB_IMAGE_IMPLEMENTATION */
/* #include "stb_image.h" */


//create a egl ui

static struct application {
	struct wl_shell *shell;
	struct tw_globals global;
	struct tw_appsurf surface;
	struct nk_wl_backend *bkend;
	struct wl_shell_surface *shell_surface;
	bool done;
	struct nk_image image;
	struct nk_user_font *user_font;
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
		tw_globals_announce(&App.global, registry, id, interface, version);
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

/*
static struct nk_image
load_texture(const char *filename)
{
    int x,y,n;
    GLuint tex;
    unsigned char *data = stbi_load(filename, &x, &y, &n, 0);
    if (!data) {
	    fprintf(stderr, "[SDL]: failed to load image: %s\n", filename);
	    exit(-1);
    }

    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    return nk_image_id((int)tex);
}
*/

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
	App.global.theme = taiwins_dark_theme;

	struct wl_registry *registry = wl_display_get_registry(wl_display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	App.done = false;

	wl_display_dispatch(wl_display);
	wl_display_roundtrip(wl_display);

	struct wl_surface *wl_surface = wl_compositor_create_surface(App.global.compositor);
	struct wl_shell_surface *shell_surface = wl_shell_get_shell_surface(App.shell, wl_surface);
	tw_appsurf_init_default(&App.surface, wl_surface,
			 &App.global);

	nk_wl_impl_wl_shell_surface(&App.surface, shell_surface);
	wl_shell_surface_set_toplevel(shell_surface);
	App.shell_surface = shell_surface;

	App.bkend = nk_egl_create_backend(wl_display);
	App.user_font = nk_wl_new_font(config, App.bkend);

	nk_egl_impl_app_surface(&App.surface, App.bkend, sample_widget, tw_make_bbox_origin(200, 400, 2));

	tw_appsurf_frame(&App.surface, false);

	tw_globals_dispatch_event_queue(&App.global);

	wl_shell_surface_destroy(shell_surface);
	tw_appsurf_release(&App.surface);
	nk_egl_destroy_backend(App.bkend);
	tw_globals_release(&App.global);
	wl_registry_destroy(registry);
	wl_display_disconnect(wl_display);
	return 0;
}

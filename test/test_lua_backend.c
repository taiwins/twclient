#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <strings.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "../client.h"
#include "../ui.h"
#include "../nk_backends.h"
/* #include "../nk_backends/nk_wl_internal.h" */

#ifndef NK_CAIRO_BACKEND
#define NK_CAIRO_BACKEND
#endif

static lua_State *L = NULL;

struct lua_user_data {
	struct nk_wl_backend *bkend;
	struct nk_context *c;

	int width;
	int height;
};

static struct application {
	struct wl_shell *shell;
	struct wl_globals global;
	struct app_surface surface;
	struct nk_wl_backend *bkend;
	struct wl_shell_surface *shell_surface;
	bool done;
} App;


#define REGISTER_METHOD(l, name, func)		\
	({lua_pushcfunction(l, func);		\
		lua_setfield(l, -2, name);	\
	})

static void
_nk_lua_drawcb(struct nk_context *c, float width, float height,
	       struct app_surface *app)
{
	//now you need to decide whether to include a lua_state in the internal or not
	struct nk_wl_backend *b = app->user_data;
	lua_getglobal(L, "draw_frame");
	lua_getfield(L, LUA_REGISTRYINDEX, "_nk_userdata");
	struct lua_user_data *user_data = (struct lua_user_data *)lua_touserdata(L, -1);
	user_data->c = c;

	lua_pushnumber(L, app->w);
	lua_pushnumber(L, app->h);

	lua_pcall(L, 3, 0, 0);
}


/**
 * think about a sample nuklear lua script, you will have this
 *
 * function draw_frame(nk, w, h):
 *     nk:layout_row_static(30, 80, 2)
 *     if nk:button_label("button") then
 *         then print("sample")
 *     nk:label("another")
 *
 * end
 *
 * we can totally have the nk defined as predefined userdata userdata
 *
 **/
///////////////////////////////// nuklear bindings ///////////////////////////////////////
static int
nk_lua_layout_row(lua_State *L)
{
	/**
	 * nk:layout_row('static', 30, 80, 2)
	 * nk:layout_row('dynamic', 30, 2)
	 * nk:layout_row('dynamic', 30, {0.5, 0.5})
	 * nk:layout_row('static', 30, {40, 40})
	 **/

	int argc = lua_gettop(L);
	assert(argc <= 5 && argc >= 4);
	//get data
	struct lua_user_data *user_data = (struct lua_user_data *)lua_touserdata(L, 1);
	const char *type = lua_tostring(L, 2);
	const int height = lua_tointeger(L, 3);
	enum nk_layout_format layout_format = NK_STATIC;
	int ncols = 0;

	if (strcasecmp(type, "static")) {
		layout_format = NK_STATIC;
		if (argc == 5)
			ncols = lua_tointeger(L, 5);
		else {
			//get the table
		}

	} else if (strcasecmp(type, "dynamic")) {
		layout_format = NK_DYNAMIC;
		assert(argc == 4);
	}
}

static int
nk_lua_button_label(lua_State *L)
{
	int argc = lua_gettop(L);
	//TODO properly handle the error, we should not crash the app because of lua side
	assert(argc == 2);
	struct lua_user_data *user_data = (struct lua_user_data *)lua_touserdata(L, 1);
	const char *string = lua_tostring(L, 2);

	int pressed = nk_button_label(user_data->c, string);
	lua_pushinteger(L, pressed);
	return 1;
}


//////////////////////////////////////////////////////////////////////////////////////////

extern void
nk_cairo_impl_app_surface(struct app_surface *surf, struct nk_wl_backend *bkend,
			  nk_wl_drawcall_t draw_cb, struct shm_pool *pool,
			  uint32_t w, uint32_t h, uint32_t x, uint32_t y,
			  int32_t s);

/* cairo_backend */
struct nk_wl_backend *nk_cairo_create_bkend(void);
void nk_cairo_destroy_bkend(struct nk_wl_backend *bkend);




//the last parameter is only useful for cairo_backend
void nk_lua_impl(struct app_surface *surf, struct nk_wl_backend *bkend,
		 const char *script, uint32_t scale, struct shm_pool *pool)
{
	uint32_t w, h;
	if (L)
		lua_close(L);
	L = luaL_newstate();

	//create the user data with the backend
	luaL_openlibs(L);
	struct lua_user_data *d = (struct lua_user_data *)
		lua_newuserdata(L, sizeof(struct lua_user_data));
	//create the metatable then bind all the functions in
	luaL_newmetatable(L, "_context");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	//TODO register all the methods methods for the context
	REGISTER_METHOD(L, "button_label", nk_lua_button_label);

	lua_setmetatable(L, -2);
	lua_setfield(L, LUA_REGISTRYINDEX, "_nk_userdata");

	//in the end, we need a do file operation
	luaL_dofile(L, script);
	//then get the width, height, and draw_calls
	lua_getglobal(L, "width");
	lua_getglobal(L, "height");
	w = lua_tonumber(L, -2);
	h = lua_tonumber(L, -1);
	lua_pop(L, 2);
	//mostly this is what
#if defined (NK_EGL_BACKEND)
	nk_egl_impl_app_surface(surf, bkend, _nk_lua_drawcb, w, h, 0, 0, scale);
#elif defined (NK_CAIRO_BACKEND)
	nk_cairo_impl_app_surface(surf, bkend, _nk_lua_drawcb, pool, w, h, 0, 0, scale);
#elif defined (NK_VK_BACKEND)
	nk_vulkan_impl_app_surface(surf, bkend, _nk_lua_drawcb, w, h, 0, 0);
#endif

}

/////////////////////////////// application code //////////////////////////////////////

static void
handle_ping(void *data,
		     struct wl_shell_surface *wl_shell_surface,
		     uint32_t serial)
{
	fprintf(stderr, "ping!!!\n");
	wl_shell_surface_pong(wl_shell_surface, serial);
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
		 uint32_t edges, int32_t width, int32_t height)
{
	fprintf(stderr, "shell_surface has configure: %d, %d, %d\n", edges, width, height);
}

static struct wl_shell_surface_listener pingpong = {
	.ping = handle_ping,
	.configure = handle_configure,
	.popup_done = handle_popup_done
};

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
	nk_lua_impl(&App.surface, App.bkend, argv[1], 1, &pool);
	app_surface_frame(&App.surface, false);

	wl_globals_dispatch_event_queue(&App.global);
	app_surface_release(&App.surface);
	nk_cairo_destroy_bkend(App.bkend);
	shm_pool_release(&pool);

	wl_globals_release(&App.global);
	wl_registry_destroy(registry);
	wl_display_disconnect(wl_display);


	return 0;
}

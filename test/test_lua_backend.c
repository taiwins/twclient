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

#define NK_LUA_MAX_LAYOUT 1024

static lua_State *L = NULL;

struct lua_user_data {
	struct nk_wl_backend *bkend;
	struct nk_context *c;
	//we will need a layout count for every frame.
};

static struct application {
	struct wl_shell *shell;
	struct wl_globals global;
	struct app_surface surface;
	struct nk_wl_backend *bkend;
	struct wl_shell_surface *shell_surface;
	bool done;
} App;


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
static bool
nk_lua_assert(lua_State *L, int pass, const char *msg)
{
	if (!pass) {
		lua_Debug ar;
		ar.name = NULL;
		if (lua_getstack(L, 0, &ar))
			lua_getinfo(L, "n", &ar);
		if (ar.name == NULL)
			ar.name = "?";
		luaL_error(L, msg, ar.name);
	}
	return pass;
}

static inline bool
nk_lua_assert_argc(lua_State *L, int pass)
{
	return nk_lua_assert(L, pass, "wrong number of argument with \'%s\'");
}

static inline bool
nk_lua_assert_alloc(lua_State *L, void *mem)
{
	return nk_lua_assert(L, mem != NULL, "out of memory with with \'%s\'");
}

static inline bool
nk_lua_assert_type(lua_State *L, int pass)
{
	return nk_lua_assert(L, pass, "wrong type of lua data \'%s\'");
}


static bool
nk_lua_is_hex(char c)
{
	return (c >= '0' && c <= '9')
		|| (c >= 'a' && c <= 'f')
		|| (c >= 'A' && c <= 'F');
}

static bool
nk_lua_is_color(int index)
{
	if (index < 0)
		index += lua_gettop(L) + 1;
	if (lua_isstring(L, index)) {
		size_t len;
		const char *color_string = lua_tolstring(L, index, &len);
		if ((len == 7 || len == 9) && color_string[0] == '#') {
			int i;
			for (i = 1; i < len; ++i) {
				if (!nk_lua_is_hex(color_string[i]))
					return false;
			}
			return true;
		}
	}
	return false;
}

static struct nk_color
nk_lua_tocolor(int index)
{
	if (index < 0)
		index += lua_gettop(L) + 1;
	size_t len;
	const char *color_string = lua_tolstring(L, index, &len);
	int r, g, b, a = 255;
	sscanf(color_string, "#%02x%02x%02x", &r, &g, &b);
	if (len == 9) {
		sscanf(color_string + 7, "%02x", &a);
	}
	struct nk_color color = {r, g, b, a};
	return color;
}


static enum nk_symbol_type
nk_lua_tosymbol(lua_State *L, int index)
{
	if (index < 0)
		index += lua_gettop(L) + 1;
	const char *s = luaL_checkstring(L, index);
	if (!strcmp(s, "none")) {
		return NK_SYMBOL_NONE;
	} else if (!strcmp(s, "x")) {
		return NK_SYMBOL_X;
	} else if (!strcmp(s, "underscore")) {
		return NK_SYMBOL_UNDERSCORE;
	} else if (!strcmp(s, "circle solid")) {
		return NK_SYMBOL_CIRCLE_SOLID;
	} else if (!strcmp(s, "circle outline")) {
		return NK_SYMBOL_CIRCLE_OUTLINE;
	} else if (!strcmp(s, "rect solid")) {
		return NK_SYMBOL_RECT_SOLID;
	} else if (!strcmp(s, "rect outline")) {
		return NK_SYMBOL_RECT_OUTLINE;
	} else if (!strcmp(s, "triangle up")) {
		return NK_SYMBOL_TRIANGLE_UP;
	} else if (!strcmp(s, "triangle down")) {
		return NK_SYMBOL_TRIANGLE_DOWN;
	} else if (!strcmp(s, "triangle left")) {
		return NK_SYMBOL_TRIANGLE_LEFT;
	} else if (!strcmp(s, "triangle right")) {
		return NK_SYMBOL_TRIANGLE_RIGHT;
	} else if (!strcmp(s, "plus")) {
		return NK_SYMBOL_PLUS;
	} else if (!strcmp(s, "minus")) {
		return NK_SYMBOL_MINUS;
	} else if (!strcmp(s, "max")) {
		return NK_SYMBOL_MAX;
	} else {
		const char *msg = lua_pushfstring(L, "unrecognized symbol type '%s'", s);
		return luaL_argerror(L, index, msg);
	}
}

static nk_flags
nk_lua_toalign(lua_State *L, int index)
{
	if (index < 0)
		index += lua_gettop(L) + 1;
	const char *s = luaL_checkstring(L, index);
	if (!strcmp(s, "left")) {
		return NK_TEXT_LEFT;
	} else if (!strcmp(s, "centered")) {
		return NK_TEXT_CENTERED;
	} else if (!strcmp(s, "right")) {
		return NK_TEXT_RIGHT;
	} else if (!strcmp(s, "top left")) {
		return NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_LEFT;
	} else if (!strcmp(s, "top centered")) {
		return NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_CENTERED;
	} else if (!strcmp(s, "top right")) {
		return NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_RIGHT;
	} else if (!strcmp(s, "bottom left")) {
		return NK_TEXT_ALIGN_BOTTOM | NK_TEXT_ALIGN_LEFT;
	} else if (!strcmp(s, "bottom centered")) {
		return NK_TEXT_ALIGN_BOTTOM | NK_TEXT_ALIGN_CENTERED;
	} else if (!strcmp(s, "bottom right")) {
		return NK_TEXT_ALIGN_BOTTOM | NK_TEXT_ALIGN_RIGHT;
	} else {
		const char *msg = lua_pushfstring(L, "unrecognized alignment '%s'", s);
		return luaL_argerror(L, index, msg);
	}
}

static nk_flags
nk_lua_windowflag(lua_State *L, int flags_begin, int flags_end)
{
	int i;
	if (flags_begin == flags_end && lua_istable(L, flags_begin)) {
		size_t flagCount = lua_objlen(L, flags_begin);
		nk_lua_assert(L, lua_checkstack(L, flagCount), "%s: failed to allocate stack space");
		for (i = 1; i <= flagCount; ++i) {
			lua_rawgeti(L, flags_begin, i);
		}
		lua_remove(L, flags_begin);
		flags_end = flags_begin + flagCount - 1;
	}
	nk_flags flags = NK_WINDOW_NO_SCROLLBAR;
	for (i = flags_begin; i <= flags_end; ++i) {
		const char *flag = luaL_checkstring(L, i);
		if (!strcmp(flag, "border"))
			flags |= NK_WINDOW_BORDER;
		else if (!strcmp(flag, "movable"))
			flags |= NK_WINDOW_MOVABLE;
		else if (!strcmp(flag, "scalable"))
			flags |= NK_WINDOW_SCALABLE;
		else if (!strcmp(flag, "closable"))
			flags |= NK_WINDOW_CLOSABLE;
		else if (!strcmp(flag, "minimizable"))
			flags |= NK_WINDOW_MINIMIZABLE;
		else if (!strcmp(flag, "scrollbar"))
			flags &= ~NK_WINDOW_NO_SCROLLBAR;
		else if (!strcmp(flag, "title"))
			flags |= NK_WINDOW_TITLE;
		else if (!strcmp(flag, "scroll auto hide"))
			flags |= NK_WINDOW_SCROLL_AUTO_HIDE;
		else if (!strcmp(flag, "background"))
			flags |= NK_WINDOW_BACKGROUND;
		else {
			const char *msg = lua_pushfstring(L, "unrecognized window flag '%s'", flag);
			return luaL_argerror(L, i, msg);
		}
	}
	return flags;
}

static nk_flags
nk_lua_toedittype(lua_State *L, int index)
{
	if (index < 0)
		index += lua_gettop(L) + 1;
	const char *type_string = luaL_checkstring(L, index);
	if (!strcmp(type_string, "simple")) {
		return NK_EDIT_SIMPLE;
	} else if (!strcmp(type_string, "field")) {
		return NK_EDIT_FIELD;
	} else if (!strcmp(type_string, "box")) {
		return NK_EDIT_BOX;
	} else {
		const char *msg = lua_pushfstring(L, "unrecognized edit type '%s'", type_string);
		return luaL_argerror(L, index, msg);
	}
}

////////////////////////// layout ///////////////////////////////

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
	const char *type = luaL_checkstring(L, 2);
	const float height = luaL_checkint(L, 3);
	enum nk_layout_format layout_format = NK_STATIC;
	int ncols = 0; bool use_ratio = 0; int ratio_param = -1;

	if (strcasecmp(type, "static") == 0) {
		layout_format = NK_STATIC;
		if (argc == 5) {
			ncols = luaL_checkint(L, 5);
			use_ratio = false;
			ratio_param = 4;
		}
		else {
			ncols = lua_objlen(L, 4);
			use_ratio = true;
			ratio_param = 4;
		}
	} else if (strcasecmp(type, "dynamic") == 0) {
		layout_format = NK_DYNAMIC;
		assert(argc == 4);
		if (lua_isnumber(L, 4)) {
			use_ratio = false;
			ncols = luaL_checkint(L, 4);

		} else if (lua_istable(L, 4)) {
			ncols = lua_objlen(L, 4);
			use_ratio = true;
			ratio_param = 4;
		}
	}
	float spans[ncols+1];
	for (int i = 0; i < ncols; i++) {
		if (use_ratio) {
			lua_rawgeti(L, ratio_param, i);
			float ratio = (float)lua_tonumber(L, -1);
			lua_pop(L, 1);
			spans[i] = ratio;
		} else {
			float ratio = (layout_format == NK_STATIC) ? (float)lua_tonumber(L, ratio_param) : 1.0 / ncols;
			spans[i] = ratio;
		}
	}
	//TODO this won't work because the spans has to be present util the frame is done
	nk_layout_row_dynamic(user_data->c, height, ncols);

	return 0;
}

/////////////////////// group/tree //////////////////////////////
static int
nk_lua_group_begin(lua_State *L)
{
	const char *title;
	struct lua_user_data *ud;
	nk_flags flags = 0;

	int argc = lua_gettop(L);
	nk_lua_assert_argc(L, argc >=2);
	nk_lua_assert_type(L, lua_isuserdata(L, 1));
	nk_lua_assert_type(L, lua_isstring(L, 2));
	ud = lua_touserdata(L, 1);
	title = lua_tostring(L, 2);
	if (argc > 2) {
		flags = nk_lua_windowflag(L, 3, argc);
	}
	int open = nk_group_begin(ud->c, title, flags);

	lua_pushboolean(L, open);
	return 1;
}

static int
nk_lua_group_end(lua_State *L)
{
	nk_lua_assert_argc(L, lua_gettop(L) == 1);
	nk_lua_assert_type(L, lua_isuserdata(L, 1));
	struct lua_user_data *d = lua_touserdata(L, 1);
	nk_group_end(d->c);
	return 0;
}


/////////////////////// simple widgets /////////////////////////

static int
nk_lua_button_label(lua_State *L)
{
	//TODO support symbol and alignment
	int argc = lua_gettop(L);

	if (!nk_lua_assert_argc(L, argc == 2))
		goto err_button;
	struct lua_user_data *user_data = (struct lua_user_data *)lua_touserdata(L, 1);
	if (!nk_lua_assert_type(L, lua_isstring(L, 2)))
		goto err_button;
	const char *string = lua_tostring(L, 2);

	int pressed = nk_button_label(user_data->c, string);
	lua_pushinteger(L, pressed);

	return 1;
err_button:
	return 0;
}

static int
nk_lua_button_color(lua_State *L)
{
	int pressed = false;
	struct nk_color color;
	struct lua_user_data *user_data;
	int argc = lua_gettop(L);
	if (!nk_lua_assert_argc(L, argc == 2))
		goto err_button;
	if (!nk_lua_assert_type(L, lua_isuserdata(L, 1)))
		goto err_button;
	user_data = (struct lua_user_data *)lua_touserdata(L, 1);
	if (!nk_lua_assert_type(L, lua_isstring(L, 2)))
		goto err_button;
	if (!nk_lua_is_color(2))
		goto err_button;
	color = nk_lua_tocolor(2);

	pressed = nk_button_color(user_data->c, color);
	lua_pushinteger(L, pressed);

	return 1;
err_button:
	return 0;
}

static int
nk_lua_checkbox(lua_State *L)
{
	bool pressed = false;
	int argc = lua_gettop(L);
	struct lua_user_data *user_data;
	const char *string;

	if (!nk_lua_assert_argc(L, argc == 3))
		goto err_checkbox;
	if (!nk_lua_assert_type(L, lua_isuserdata(L, 1)))
		goto err_checkbox;
	if (!nk_lua_assert_type(L, lua_isstring(L, 2)))
		goto err_checkbox;
	if (!nk_lua_assert_type(L, lua_isboolean(L, 3)))
		goto err_checkbox;

	user_data = (struct lua_user_data *)lua_touserdata(L, 1);
	string = lua_tostring(L, 2);
	pressed = lua_toboolean(L, 3);

	pressed = nk_check_label(user_data->c, string, pressed);
	lua_pushboolean(L, pressed);
	return 1;
err_checkbox:
	return 0;
}

static int
nk_lua_radio(lua_State *L)
{
	struct lua_user_data *user_data;
	const char *string;
	bool is_active;

	int argc = lua_gettop(L);
	if (!nk_lua_assert_argc(L, argc == 3))
		goto err_radio;
	if (!nk_lua_assert_type(L, lua_isuserdata(L, 1)))
		goto err_radio;
	if (!nk_lua_assert_type(L, lua_isstring(L, 2)))
		goto err_radio;
	if (!nk_lua_assert_type(L, lua_isboolean(L, 3)))
		goto err_radio;

	user_data = (struct lua_user_data *)lua_touserdata(L, 1);
	string = lua_tostring(L, 2);
	is_active = lua_toboolean(L, 3);
	is_active = nk_option_label(user_data->c, string, is_active);
err_radio:
	return 0;
}

static int
nk_lua_selectable(lua_State *L)
{
	/**
	 * selected = nk:selectable(text, align, selected)
	 * selected = nk:selectable(text, symbol, align, selected)
	 */
	//we would like to allow symbol this time
	const char *string;
	bool selected = false;
	struct lua_user_data *user_data;

	int argc = lua_gettop(L);
	if (!nk_lua_assert_argc(L, argc == 4 || argc == 5))
		goto err_select;
	if (!nk_lua_assert_type(L, lua_isuserdata(L, 1)))
		goto err_select;
	if (!nk_lua_assert_type(L, lua_isstring(L, 2)))
		goto err_select;

	user_data = (struct lua_user_data *)lua_touserdata(L, 1);
	string = lua_tostring(L, 2);
	if (argc == 5) {
		nk_flags align = NK_TEXT_CENTERED;
		enum nk_symbol_type symbol;
		if (!nk_lua_assert_type(L, lua_isstring(L, 3)))
			goto err_select;
		if (!nk_lua_assert_type(L, lua_isstring(L, 4)))
			goto err_select;
		if (!nk_lua_assert_type(L, lua_isboolean(L, 5)))
			goto err_select;
		selected = lua_toboolean(L, 5);
		align = nk_lua_toalign(L, 4);
		symbol = nk_lua_tosymbol(L, 3);
		selected = nk_select_symbol_label(user_data->c, symbol, string, align, selected);
	} else {
		nk_flags align = NK_TEXT_CENTERED;
		if (!nk_lua_assert_type(L, lua_isstring(L, 3)))
			goto err_select;
		if (!nk_lua_assert_type(L, lua_isboolean(L, 4)))
			goto err_select;
		selected = lua_toboolean(L, 4);
		align = nk_lua_toalign(L, 3);
		selected = nk_select_label(user_data->c, string, align, selected);
	}
	lua_pushboolean(L, selected);
	return 1;
err_select:
	return 0;
}

//because we have assert(setjmp and longjmps), those argc is useless.
static int
nk_lua_slider(lua_State *L)
{
	struct lua_user_data *user_data;
	float min, max, current, step;

	int argc = lua_gettop(L);
	nk_lua_assert_argc(L, argc == 5);
	nk_lua_assert_type(L, lua_isuserdata(L, 1));
	nk_lua_assert_type(L, lua_isnumber(L, 2));
	nk_lua_assert_type(L, lua_isnumber(L, 3));
	nk_lua_assert_type(L, lua_isnumber(L, 4));
	nk_lua_assert_type(L, lua_isnumber(L, 5));

	user_data = (struct lua_user_data *)lua_touserdata(L, 1);
	min = lua_tonumber(L, 2);
	max = lua_tonumber(L, 4);
	current = lua_tonumber(L, 3);
	step = lua_tonumber(L, 5);

	current = nk_slide_float(user_data->c, min, current, max, step);
	lua_pushnumber(L, current);
	return 1;
}

static int
nk_lua_progress(lua_State *L)
{
	struct lua_user_data *user_data;
	nk_size current, max;
	bool modifiable = false;

	int argc = lua_gettop(L);

	nk_lua_assert_argc(L, argc == 3 || argc == 4);
	nk_lua_assert_type(L, lua_isuserdata(L, 1));
	nk_lua_assert_type(L, lua_isnumber(L, 2));
	nk_lua_assert_type(L, lua_isnumber(L, 3));
	if (argc == 4) {
		nk_lua_assert_type(L, lua_isboolean(L, 4));
		modifiable = lua_toboolean(L, 4);
	}
	user_data = (struct lua_user_data *)lua_touserdata(L, 1);
	current = lua_tointeger(L, 2);
	max = lua_tointeger(L, 3);

	current = nk_prog(user_data->c, current, max, modifiable);
	lua_pushnumber(L, current);
	return 1;
}

static int
nk_lua_color_pick(lua_State *L)
{
	struct lua_user_data *user_data;
	struct nk_colorf colorf;
	float c[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	const char *rgba[] =  {"r", "g", "b", "a"};

	int argc = lua_gettop(L);
	nk_lua_assert_argc(L, argc == 2);
	nk_lua_assert_type(L, lua_isuserdata(L, 1));
	nk_lua_assert_type(L, lua_istable(L, 2));

	user_data = (struct lua_user_data *)lua_touserdata(L, 1);

	for (int i = 0; i < 4; i++) {
		lua_pushstring(L, rgba[i]);
		lua_rawget(L, 2);
		c[i] = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
	c[3] = (c[3] == 0.0f) ? 1.0f : c[3];

	colorf.r = c[0]; colorf.g = c[1];
	colorf.b = c[2]; colorf.a = c[3];
	colorf = nk_color_picker(user_data->c, colorf, NK_RGBA);
	c[0] = colorf.r; c[1] = colorf.g;
	c[2] = colorf.b; c[3] = colorf.a;

	for (int i = 0; i < 4; i++) {
		lua_pushstring(L, rgba[i]);
		lua_pushnumber(L, c[i]);
		lua_rawset(L, 2);
	}

	return 0;
}

static int
nk_lua_property(lua_State *L)
{
	struct lua_user_data *ud;
	const char *name;
	double min, val, max, step, inc;

	int argc = lua_gettop(L);
	nk_lua_assert_argc(L, argc == 7);
	nk_lua_assert_type(L, lua_isuserdata(L, 1));
	nk_lua_assert_type(L, lua_isstring(L, 2));
	nk_lua_assert_type(L, lua_isnumber(L, 3));
	nk_lua_assert_type(L, lua_isnumber(L, 4));
	nk_lua_assert_type(L, lua_isnumber(L, 5));
	nk_lua_assert_type(L, lua_isnumber(L, 6));
	nk_lua_assert_type(L, lua_isnumber(L, 7));
	ud = lua_touserdata(L, 1);
	name = lua_tostring(L, 2);
	min = lua_tonumber(L, 3);
	val = lua_tonumber(L, 4);
	max = lua_tonumber(L, 5);
	step = lua_tonumber(L, 6);
	inc = lua_tonumber(L, 7);

	val = nk_propertyd(ud->c, name, min, val, max, step, inc);
	lua_pushnumber(L, val);
	return 1;
}


static int
nk_lua_edit(lua_State *L)
{
	struct lua_user_data *ud;
	const char *string;
	char edit_buffer[1024];

	int argc = lua_gettop(L);
	nk_lua_assert_argc(L, argc == 3);
	nk_lua_assert_type(L, lua_isuserdata(L, 1));
	nk_lua_assert_type(L, lua_isstring(L, 2));
	nk_lua_assert_type(L, lua_istable(L, 3));
	ud = lua_touserdata(L, 1);
	nk_flags flags = nk_lua_toedittype(L, 2);

	lua_getfield(L, 3, "value");
	string = lua_tostring(L, -1);
	lua_pop(L, 1);
	int len = strlen(string) < 1023 ? strlen(string) : 1023;
	edit_buffer[len] = '\0';

	nk_flags event =
		nk_edit_string_zero_terminated(ud->c, flags, edit_buffer, 1023, nk_filter_default);
	bool changed = !strcmp(edit_buffer, string);
	lua_pushstring(L, edit_buffer);
	lua_setfield(L, 3, "value");

	if (event & NK_EDIT_COMMITED)
		lua_pushstring(L, "commited");
	else if (event & NK_EDIT_ACTIVATED)
		lua_pushstring(L, "activated");
	else if (event & NK_EDIT_DEACTIVATED)
		lua_pushstring(L, "deactivated");
	else if (event & NK_EDIT_ACTIVE)
		lua_pushstring(L, "active");
	else if (event & NK_EDIT_INACTIVE)
		lua_pushstring(L, "inactive");
	else
		lua_pushnil(L);
	lua_pushboolean(L, changed);
	return 2;
}
/////////////////////// complex widgets /////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////

extern void
nk_cairo_impl_app_surface(struct app_surface *surf, struct nk_wl_backend *bkend,
			  nk_wl_drawcall_t draw_cb, struct shm_pool *pool,
			  uint32_t w, uint32_t h, uint32_t x, uint32_t y,
			  int32_t s);

/* cairo_backend */
struct nk_wl_backend *nk_cairo_create_bkend(void);
void nk_cairo_destroy_bkend(struct nk_wl_backend *bkend);

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

	if (lua_pcall(L, 3, 0, 0)) {
		const char *err_str = lua_tostring(L, -1);
		fprintf(stderr, "-- lua runtime err: %s\n", err_str);
		lua_pop(L, 1);
		nk_clear(c);
	}
}

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

	//groups
	REGISTER_METHOD(L, "group_begin", nk_lua_group_begin);
	REGISTER_METHOD(L, "group_end", nk_lua_group_end);
	//TODO simple widgets
	REGISTER_METHOD(L, "layout_row", nk_lua_layout_row);
	REGISTER_METHOD(L, "button_label", nk_lua_button_label);
	REGISTER_METHOD(L, "button_color", nk_lua_button_color);
	REGISTER_METHOD(L, "checkbox", nk_lua_checkbox);
	REGISTER_METHOD(L, "radio_label", nk_lua_radio);
	REGISTER_METHOD(L, "selectable", nk_lua_selectable);
	REGISTER_METHOD(L, "slider", nk_lua_slider);
	REGISTER_METHOD(L, "progress", nk_lua_progress);
	REGISTER_METHOD(L, "color_pick", nk_lua_color_pick);
	REGISTER_METHOD(L, "property", nk_lua_property);
	REGISTER_METHOD(L, "edit", nk_lua_edit);

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

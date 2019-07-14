#ifndef NK_LUA_H
#define NK_LUA_H

#include <math.h>
#include <stdio.h>
#include <stdint.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "../ui.h"
#include "../client.h"
#include "nk_wl_internal.h"

//how do you want to use the lua bindings
//this could be an example:
//create backend using for example nk_egl_create_bkend();
//implement nuklear surface using a single script

// nk_lua_impl(bkend, script_path). About anchor of widget? I guess I have to
// implement another [ const char *lua_widget_anchor(script) ]

//we need a constant state to store lua stacks,

struct lua_user_data {
	struct nk_wl_backend *bkend;
	int width;
	int height;
};


static void
_nk_lua_drawcb(struct nk_context *c, float width, float height,
	       struct app_surface *app)
{
	//now you need to decide whether to include a lua_state in the internal or not
	struct nk_wl_backend *b = app->user_data;

	lua_getglobal(b->L, "draw_frame");
	luaL_getmetatable(b->L, "_context");

	lua_pushnumber(b->L, app->w);
	lua_pushnumber(b->L, app->h);

	lua_pcall(b->L, 3, 0, 0);
}


//the last parameter is only useful for cairo_backend
void nk_lua_impl(struct app_surface *surf, struct nk_wl_backend *bkend,
		 const char *script, uint32_t scale, struct shm_pool *pool)
{
	uint32_t w, h;
	if (bkend->L)
		lua_close(bkend->L);
	bkend->L = luaL_newstate();
	lua_State *L = bkend->L;

	//create the user data with the backend
	luaL_openlibs(L);
	struct lua_user_data *d = (struct lua_user_data *)
		lua_newuserdata(bkend->L, sizeof(struct lua_user_data));
	//create the metatable then bind all the functions in
	luaL_getmetatble(L, "what");
	lua_setmetatable(L, -2);

	//in the end, we need a do file operation
	luaL_dofile(L, script);
	//then get the width, height, and draw_calls
	lua_getglobal(L, "width");
	lua_getglobal(L, "height");
	w = lua_tonumber(L, -2);
	h = lua_tonumber(L, -1);
	lua_pop(L, 2);
	//mostly this is what
	if (is_egl_backend(bkend))
		nk_egl_impl_app_surface(surf, bkend, _nk_lua_drawcb, w, h, 0, 0, scale);
	else if (is_cairo_backend(bkend))
		nk_cairo_impl_app_surface(surf, bkend, _nk_lua_drawcb, pool, w, h, 0, 0, scale);
	else
		nk_vulkan_impl_app_surfae(surf, bkend, _nk_lua_drawcb, w, h, 0, 0);
	//
}



#endif /* EOF */

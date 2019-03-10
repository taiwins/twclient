#ifndef NK_LUA_H
#define NK_LUA_H

#include <math.h>
#include <stdio.h>
#include <stdint.h>

#include <lua.h>
#include <lauxlib.h>


#include "nk_wl_internal.h"

//how do you want to use the lua bindings
//this could be an example:
//create backend using for example nk_egl_create_bkend();
//implement nuklear surface using a single script

// nk_lua_impl(bkend, script_path). About anchor of widget? I guess I have to
// implement another [ const char *lua_widget_anchor(script) ]

//we need a constant state to store lua stacks,

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
		 const char *script, struct shm_pool *pool)
{
	if (!bkend->L)
		bkend->L = luaL_newstate();
	else {
		//destroy whatever you have in the bkend and recreate one
	}
}



#endif /* EOF */

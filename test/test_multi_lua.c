#include <stdio.h>
#include <assert.h>
#include <luajit-2.1/lua.h>
#include <luajit-2.1/lualib.h>
#include <luajit-2.1/lauxlib.h>

static const char *test_string0 =
	"\
function random_function(a) \
   return(a*9 + a/9) \
end \
";

static const char *test_string1 = "\
function random_function(a) \
   return(a*8 + a/8) \
end \
";

static const char *test_string2 = "\
function random_function(a) \
   return(a*7 + a/7) \
end \
";

static inline void pstack(lua_State *L)
{
	fprintf(stderr, "lua current stack %d\n", lua_gettop(L));
}


int main(int argc, char *argv[])
{
	lua_State *L;
	int err;

	//this works for now, but it seems you can create tables to sandbox it

	L = luaL_newstate();
	luaL_openlibs(L);

	assert( (err = luaL_dostring(L, test_string0)) == LUA_OK);
	lua_getglobal(L, "random_function");
	lua_setfield(L, LUA_REGISTRYINDEX, "r0");

	assert( (err = luaL_dostring(L, test_string1)) == LUA_OK);
	lua_getglobal(L, "random_function");
	lua_setfield(L, LUA_REGISTRYINDEX, "r1");

	assert( (err = luaL_dostring(L, test_string2)) == LUA_OK);
	lua_getglobal(L, "random_function");
	lua_setfield(L, LUA_REGISTRYINDEX, "r2");


	lua_getfield(L, LUA_REGISTRYINDEX, "r0");
	lua_pushnumber(L, 1.0);
	lua_pcall(L, 1, 1, 0);
	printf("%f\n", lua_tonumber(L, -1));
	lua_pop(L, 1);


	lua_getfield(L, LUA_REGISTRYINDEX, "r1");
	lua_pushnumber(L, 1.0);
	lua_pcall(L, 1, 1, 0);
	printf("%f\n", lua_tonumber(L, -1));
	lua_pop(L, 1);

	lua_getfield(L, LUA_REGISTRYINDEX, "r2");
	lua_pushnumber(L, 1.0);
	lua_pcall(L, 1, 1, 0);
	printf("%f\n", lua_tonumber(L, -1));
	lua_pop(L, 1);


	lua_close(L);
	return 0;

}

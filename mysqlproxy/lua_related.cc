#include <lua5.1/lua.hpp>
#include <string>

std::string
xlua_tolstring(lua_State *const, int);
void
xlua_pushlstring(lua_State *const, const std::string &);


std::string
xlua_tolstring(lua_State *const l, int index){
    size_t len;
    char const *const s = lua_tolstring(l, index, &len);
    return std::string(s, len);
}

void
xlua_pushlstring(lua_State *const l, const std::string &s){
    lua_pushlstring(l, s.data(), s.length());
}




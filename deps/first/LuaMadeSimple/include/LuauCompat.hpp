// Luau Compatibility Layer for Lua 5.4 API
// This header provides compatibility shims to make Luau work with code written for Lua 5.4
#pragma once

// Define to indicate we're using Luau instead of PUC-Rio Lua
#define USING_LUAU 1

// Include Luau headers
#include <lua.h>
#include <lualib.h>

// Include Luau compiler for bytecode compilation
#include <Luau/Compiler.h>

#include <string>
#include <cstring>
#include <cstdio>
#include <functional>
#include <unordered_map>
#include <mutex>

// ============================================================================
// Lua 5.4 Constants/Types that don't exist in Luau
// ============================================================================

// Luau uses int for lua_Integer, but some code expects int64_t
// We keep lua_Integer as Luau defines it (int) for compatibility

// LUA_ERRFILE doesn't exist in Luau
#ifndef LUA_ERRFILE
#define LUA_ERRFILE 6
#endif

// Hook event constants (for compatibility even though Luau doesn't use them)
#ifndef LUA_HOOKCALL
#define LUA_HOOKCALL    0
#define LUA_HOOKRET     1
#define LUA_HOOKLINE    2
#define LUA_HOOKCOUNT   3
#define LUA_HOOKTAILCALL 4
#endif

// ============================================================================
// lua_pushglobaltable - doesn't exist in Luau, use LUA_GLOBALSINDEX
// ============================================================================
inline void lua_pushglobaltable(lua_State* L)
{
    lua_pushvalue(L, LUA_GLOBALSINDEX);
}

// ============================================================================
// lua_geti / lua_seti - Luau has these but with int index, not lua_Integer
// In Lua 5.3+, these take lua_Integer; Luau uses int
// ============================================================================
inline int lua_geti(lua_State* L, int idx, lua_Integer n)
{
    return lua_rawgeti(L, idx, static_cast<int>(n));
}

inline void lua_seti(lua_State* L, int idx, lua_Integer n)
{
    lua_rawseti(L, idx, static_cast<int>(n));
}

// ============================================================================
// luaL_len - get length of value at index as integer
// ============================================================================
inline lua_Integer luaL_len(lua_State* L, int idx)
{
    return static_cast<lua_Integer>(lua_objlen(L, idx));
}

// ============================================================================
// lua_getstack - Luau has a different debug API
// In Lua: lua_getstack(L, level, ar) fills ar with info about stack level
// In Luau: lua_stackdepth(L) returns depth, lua_getinfo(L, level, what, ar)
// ============================================================================
// ============================================================================
// Debug API Compatibility Layer
// Luau's debug API is fundamentally different from Lua 5.x:
// - lua_getstack doesn't exist (use lua_stackdepth + lua_getinfo)
// - lua_getinfo has different signature: (L, level, what, ar) vs (L, what, ar)
// - lua_Debug struct is different (missing namewhat, event, etc.)
// - lua_getlocal has different signature: (L, level, n) vs (L, ar, n)
//
// We provide compatibility wrappers but some code may need manual updates.
// ============================================================================

// Thread-local storage to track level between lua_getstack and lua_getinfo calls
namespace LuauCompat {
    inline thread_local int s_current_debug_level = 0;
}

inline int lua_getstack(lua_State* L, int level, lua_Debug* ar)
{
    // Check if the level is valid
    int depth = lua_stackdepth(L);
    if (level < 0 || level >= depth)
    {
        return 0; // Invalid level
    }

    // Store the level for subsequent lua_getinfo call
    LuauCompat::s_current_debug_level = level;

    // Initialize ar with some defaults
    ar->name = nullptr;
    ar->what = nullptr;
    ar->source = nullptr;
    ar->short_src = nullptr;
    ar->linedefined = -1;
    ar->currentline = -1;

    return 1;
}

// Override lua_getinfo to use our stored level
// This is a 3-argument version that wraps Luau's 4-argument version
inline int luau_getinfo_3arg(lua_State* L, const char* what, lua_Debug* ar)
{
    return lua_getinfo(L, LuauCompat::s_current_debug_level, what, ar);
}

// lua_getlocal compatibility - use stored level
inline const char* luau_getlocal_3arg(lua_State* L, lua_Debug* ar, int n)
{
    (void)ar; // ar is not used in Luau's version
    return lua_getlocal(L, LuauCompat::s_current_debug_level, n);
}

// Macros to redirect 3-arg calls to our wrappers
// IMPORTANT: These macros use __VA_ARGS__ to handle the different arg counts
// For lua_getinfo: Lua 5.x uses 3 args, Luau uses 4 args
// We detect which version based on arg count

// Helper to count arguments
#define LUAU_COMPAT_GET_ARG_COUNT(...) LUAU_COMPAT_GET_ARG_COUNT_IMPL(__VA_ARGS__, 4, 3, 2, 1, 0)
#define LUAU_COMPAT_GET_ARG_COUNT_IMPL(_1, _2, _3, _4, N, ...) N

// Provide namewhat field emulation via a separate query
// (namewhat indicates whether 'name' is a global, local, method, field, etc.)
// Luau doesn't provide this directly, so we return empty string
#define LUAU_COMPAT_NAMEWHAT ""

// ============================================================================
// 64-bit Integer Compatibility
// CRITICAL: Luau defines lua_Integer as 'int' (32-bit), but UE4SS needs 64-bit
// for memory addresses. We use lua_Number (double) which can precisely represent
// integers up to 2^53, sufficient for most userspace addresses.
// ============================================================================

// lua_isinteger - check if value is a number that represents an integer
inline int lua_isinteger(lua_State* L, int idx)
{
    if (!lua_isnumber(L, idx))
        return 0;

    double n = lua_tonumber(L, idx);
    // Check if the double represents a whole number within int64_t range
    return n == static_cast<double>(static_cast<int64_t>(n));
}

// Override lua_pushinteger to use lua_pushnumber for 64-bit support
// This prevents truncation of addresses larger than 32 bits
#undef lua_pushinteger
inline void lua_pushinteger_64(lua_State* L, int64_t n)
{
    lua_pushnumber(L, static_cast<double>(n));
}
#define lua_pushinteger(L, n) lua_pushinteger_64(L, static_cast<int64_t>(n))

// Override lua_tointeger to use lua_tonumber and cast to int64_t
// This allows retrieving 64-bit values that were pushed as numbers
#undef lua_tointeger
inline int64_t lua_tointeger_64(lua_State* L, int idx)
{
    return static_cast<int64_t>(lua_tonumber(L, idx));
}
#define lua_tointeger(L, idx) lua_tointeger_64(L, idx)

// Also provide lua_tointegerx for code that uses it
inline int64_t lua_tointegerx_64(lua_State* L, int idx, int* isnum)
{
    double n = lua_tonumber(L, idx);
    if (isnum)
    {
        *isnum = lua_isnumber(L, idx) && (n == static_cast<double>(static_cast<int64_t>(n)));
    }
    return static_cast<int64_t>(n);
}
#define lua_tointegerx(L, idx, isnum) lua_tointegerx_64(L, idx, isnum)

// ============================================================================
// lua_rotate - Luau doesn't have this, implement it
// ============================================================================
inline void lua_rotate(lua_State* L, int idx, int n)
{
    int t = lua_gettop(L);
    idx = lua_absindex(L, idx);

    int m = (n >= 0) ? (t - n) : (-n - 1);

    if (n > 0)
    {
        // Rotate elements up
        for (int i = 0; i < n; i++)
        {
            lua_pushvalue(L, idx);
            lua_remove(L, idx);
            lua_insert(L, t);
        }
    }
    else if (n < 0)
    {
        // Rotate elements down
        n = -n;
        for (int i = 0; i < n; i++)
        {
            lua_pushvalue(L, t);
            lua_remove(L, t);
            lua_insert(L, idx);
        }
    }
}

// ============================================================================
// lua_copy - Luau doesn't have this
// ============================================================================
inline void lua_copy(lua_State* L, int fromidx, int toidx)
{
    lua_pushvalue(L, fromidx);
    lua_replace(L, toidx);
}

// ============================================================================
// User Values Emulation
// Luau doesn't support multiple user values on userdata like Lua 5.4
// We emulate this using environment tables
// ============================================================================

// Create userdata with user values support
// In Luau, we use the environment table to store user values
inline void* lua_newuserdatauv(lua_State* L, size_t sz, int nuvalue)
{
    void* ud = lua_newuserdata(L, sz);

    if (nuvalue > 0)
    {
        // Create a table to hold user values
        lua_createtable(L, nuvalue, 0);
        // Set it as the environment for the userdata
        lua_setfenv(L, -2);
    }

    return ud;
}

// Get user value from userdata
// Returns the type of the value
inline int lua_getiuservalue(lua_State* L, int idx, int n)
{
    idx = lua_absindex(L, idx);

    // Get the environment table
    lua_getfenv(L, idx);

    if (lua_isnil(L, -1))
    {
        // No environment table, return nil
        return LUA_TNIL;
    }

    // Get the nth value from the environment table
    lua_rawgeti(L, -1, n);

    // Remove the environment table, keep only the value
    lua_remove(L, -2);

    return lua_type(L, -1);
}

// Set user value on userdata
// Returns 1 on success, 0 on failure
inline int lua_setiuservalue(lua_State* L, int idx, int n)
{
    idx = lua_absindex(L, idx);

    // Value to set is at top of stack
    // Get the environment table
    lua_getfenv(L, idx);

    if (lua_isnil(L, -1))
    {
        // No environment table, create one
        lua_pop(L, 1);
        lua_createtable(L, 4, 0); // Pre-allocate for typical usage
        lua_pushvalue(L, -1);
        lua_setfenv(L, idx);
    }

    // Stack: [value] [env_table]
    lua_pushvalue(L, -2); // Copy value to top
    lua_rawseti(L, -2, n); // env_table[n] = value
    lua_pop(L, 2); // Pop env_table and original value

    return 1;
}

// ============================================================================
// lua_pushcfunction / lua_pushcclosure compatibility
// Luau requires a debug name for C functions
// In standard Lua: lua_pushcclosure(L, fn, nup)
// In Luau: lua_pushcclosurek(L, fn, debugname, nup, cont)
// ============================================================================

// Redefine lua_pushcfunction to use the Luau signature with a default debug name
// Original Lua signature: lua_pushcfunction(L, fn)
// We use the stringified function name as the debug name
#undef lua_pushcfunction
#define lua_pushcfunction(L, fn) lua_pushcclosurek(L, fn, #fn, 0, NULL)

// For closures with upvalues
// Original Lua signature: lua_pushcclosure(L, fn, nup)
// We use "closure" as the debug name since we don't have the original function name
#undef lua_pushcclosure
#define lua_pushcclosure(L, fn, nup) lua_pushcclosurek(L, fn, "closure", nup, NULL)

// ============================================================================
// luaL_loadfile / luaL_loadstring / luaL_loadbuffer compatibility
// Luau requires bytecode compilation before loading
// ============================================================================

inline int luaL_loadstring(lua_State* L, const char* s)
{
    size_t len = strlen(s);

    // Compile the source to bytecode
    std::string bytecode = Luau::compile(std::string(s, len));

    // Check for compilation errors (bytecode starting with 0 is an error)
    if (bytecode.empty() || bytecode[0] == 0)
    {
        // Push error message
        if (bytecode.size() > 1)
            lua_pushstring(L, bytecode.c_str() + 1);
        else
            lua_pushstring(L, "compilation failed");
        return LUA_ERRSYNTAX;
    }

    // Load the bytecode
    int result = luau_load(L, "chunk", bytecode.data(), bytecode.size(), 0);
    return result;
}

inline int luaL_loadbuffer(lua_State* L, const char* buff, size_t sz, const char* name)
{
    // Compile the source to bytecode
    std::string bytecode = Luau::compile(std::string(buff, sz));

    // Check for compilation errors
    if (bytecode.empty() || bytecode[0] == 0)
    {
        if (bytecode.size() > 1)
            lua_pushstring(L, bytecode.c_str() + 1);
        else
            lua_pushstring(L, "compilation failed");
        return LUA_ERRSYNTAX;
    }

    // Load the bytecode
    return luau_load(L, name, bytecode.data(), bytecode.size(), 0);
}

inline int luaL_loadfile(lua_State* L, const char* filename)
{
    // Read file content
    FILE* f = fopen(filename, "rb");
    if (!f)
    {
        lua_pushfstring(L, "cannot open %s", filename);
        return LUA_ERRFILE;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string content(size, '\0');
    size_t read = fread(&content[0], 1, size, f);
    fclose(f);

    if (read != static_cast<size_t>(size))
    {
        lua_pushfstring(L, "cannot read %s", filename);
        return LUA_ERRFILE;
    }

    return luaL_loadbuffer(L, content.data(), content.size(), filename);
}

// ============================================================================
// luaL_ref / luaL_unref compatibility
// These work slightly differently in Luau
// ============================================================================

#ifndef LUA_NOREF
#define LUA_NOREF LUA_NOREF
#endif

#ifndef LUA_REFNIL
#define LUA_REFNIL LUA_REFNIL
#endif

inline int luaL_ref(lua_State* L, int t)
{
    t = lua_absindex(L, t);

    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        return LUA_REFNIL;
    }

    // Use lua_ref which is Luau's reference function
    return lua_ref(L, -1);
}

inline void luaL_unref(lua_State* L, int t, int ref)
{
    if (ref >= 0)
    {
        lua_unref(L, ref);
    }
}

// ============================================================================
// Additional compatibility macros
// ============================================================================

// luaL_dostring - execute a string
#ifndef luaL_dostring
#define luaL_dostring(L, s) \
    (luaL_loadstring(L, s) || lua_pcall(L, 0, LUA_MULTRET, 0))
#endif

// luaL_dofile - execute a file
#ifndef luaL_dofile
#define luaL_dofile(L, fn) \
    (luaL_loadfile(L, fn) || lua_pcall(L, 0, LUA_MULTRET, 0))
#endif

// lua_rawlen doesn't exist in Luau, use lua_objlen
#ifndef lua_rawlen
#define lua_rawlen(L, idx) lua_objlen(L, idx)
#endif

// ============================================================================
// Debug hooks compatibility
// Luau has a different debugging model using lua_Callbacks
// lua_sethook doesn't exist - we provide a no-op for compilation
// ============================================================================

// Hook masks (defined for compatibility even though not used)
#ifndef LUA_MASKCALL
#define LUA_MASKCALL  (1 << 0)
#define LUA_MASKRET   (1 << 1)
#define LUA_MASKLINE  (1 << 2)
#define LUA_MASKCOUNT (1 << 3)
#endif

// lua_Hook type for compatibility
typedef void (*lua_Hook_compat)(lua_State* L, lua_Debug* ar);

// lua_sethook - no-op in Luau (Luau uses lua_callbacks()->debugstep instead)
inline int lua_sethook(lua_State* L, lua_Hook_compat hook, int mask, int count)
{
    // Luau doesn't support traditional Lua hooks
    // For debugging, use lua_callbacks(L)->debugstep, debugbreak, etc.
    (void)L;
    (void)hook;
    (void)mask;
    (void)count;
    return 0;
}

// lua_gethook - no-op in Luau
inline lua_Hook_compat lua_gethook(lua_State* L)
{
    (void)L;
    return nullptr;
}

// lua_gethookmask - no-op in Luau
inline int lua_gethookmask(lua_State* L)
{
    (void)L;
    return 0;
}

// lua_gethookcount - no-op in Luau
inline int lua_gethookcount(lua_State* L)
{
    (void)L;
    return 0;
}

// ============================================================================
// Userdata destructor support
// Luau doesn't support __gc metamethod - use lua_newuserdatadtor instead
// ============================================================================

// Helper template for type-safe destructor registration
template<typename T>
inline void* luau_newuserdata_with_dtor(lua_State* L)
{
    return lua_newuserdatadtor(L, sizeof(T), [](void* ud) {
        static_cast<T*>(ud)->~T();
    });
}

// ============================================================================
// __gc metamethod emulation using tagged userdata
// Since Luau doesn't support __gc, we use a tagged userdata system
// ============================================================================

// Global destructor registry - maps type tags to destructors
// This allows us to register destructors for different userdata types
namespace LuauCompat
{
    // Maximum number of userdata tags we support
    constexpr int MAX_USERDATA_TAGS = 256;

    // Get or create a tag for a type (based on metatable name)
    inline int get_or_create_tag(lua_State* L, const char* metatable_name)
    {
        // Use a simple counter stored in the registry
        static std::unordered_map<std::string, int> tag_map;
        static std::mutex tag_mutex;
        static int next_tag = 1; // Tag 0 is reserved for untagged userdata

        std::lock_guard<std::mutex> lock(tag_mutex);

        auto it = tag_map.find(metatable_name);
        if (it != tag_map.end())
        {
            return it->second;
        }

        int tag = next_tag++;
        if (tag >= MAX_USERDATA_TAGS)
        {
            // Fallback to untagged
            return 0;
        }

        tag_map[metatable_name] = tag;
        return tag;
    }

    // Destructor function type that matches Luau's expected signature
    using DestructorFn = void (*)(lua_State*, void*);

    // Register a destructor for a tag
    inline void register_destructor(lua_State* L, int tag, DestructorFn dtor)
    {
        if (tag > 0 && tag < MAX_USERDATA_TAGS)
        {
            lua_setuserdatadtor(L, tag, dtor);
        }
    }

    // Create userdata with a registered destructor tag
    inline void* newuserdata_tagged(lua_State* L, size_t sz, int tag)
    {
        if (tag > 0)
        {
            return lua_newuserdatatagged(L, sz, tag);
        }
        return lua_newuserdata(L, sz);
    }
}

// ============================================================================
// lua.hpp equivalent - include all Lua headers
// ============================================================================
// Note: In Luau there's no separate lua.hpp, we just include lua.h and lualib.h
// which we've already done at the top of this file

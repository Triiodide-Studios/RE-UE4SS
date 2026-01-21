#pragma once

/**
 * @file LuauCompat.hpp
 * @brief Luau compatibility layer for LuaMadeSimple
 *
 * This header centralizes all Luau-specific compatibility code, making it easier
 * to maintain the differences between standard Lua 5.x and Luau.
 *
 * ## Key Differences Between Lua 5.x and Luau:
 *
 * 1. **Garbage Collection / Destructors**
 *    - Lua 5.x: Uses `__gc` metamethod for cleanup
 *    - Luau: Uses `lua_newuserdatadtor()` to register destructor at creation time
 *
 * 2. **User Values / Environment Tables**
 *    - Lua 5.x: `lua_setuservalue()` / `lua_getuservalue()` with multiple user values
 *    - Luau: `lua_setfenv()` / `lua_getfenv()` with environment tables
 *    - Our approach: Store metadata on metatables instead (more portable)
 *
 * 3. **Bitwise Operators**
 *    - Lua 5.3+: Native `|`, `&`, `~`, `>>`, `<<` operators
 *    - Luau: Uses `bit32.bor()`, `bit32.band()`, `bit32.bnot()`, etc.
 *
 * 4. **Standard Libraries**
 *    - Lua 5.x: Full `io`, `os`, `debug`, `package` libraries
 *    - Luau: No `io` or `os` by default (sandboxed), limited `debug`
 *    - `package.searchers` doesn't exist; require() must be overridden
 *
 * 5. **Module Loading**
 *    - Lua 5.x: Uses `package.searchers` table for custom loaders
 *    - Luau: Must override global `require()` function directly
 *
 * ## Metatable Storage Convention:
 *
 * We store the following on metatables for userdata:
 * - `__member_funcs`: Table of member functions (method dispatch)
 * - `__user_metamethods`: Userdata containing MetaMethodContainer
 * - `__is_polymorphic`: Boolean flag for polymorphic types
 *
 * This avoids using environment tables which behave differently between
 * Lua versions and allows the metatable to be shared across instances.
 */

#include <lua.hpp>
#include <string>

namespace RC::LuaMadeSimple::Luau
{
    // ============================================================================
    // Metatable Key Constants
    // ============================================================================
    // These keys are used to store metadata on metatables for userdata objects.
    // Using named constants prevents typos and makes refactoring easier.

    /// Key for storing member functions table on metatable
    /// The table maps method names to their implementations
    constexpr const char* MT_KEY_MEMBER_FUNCS = "__member_funcs";

    /// Key for storing user-defined metamethods container on metatable
    /// Contains a MetaMethodContainer userdata with custom __index, __newindex, etc.
    constexpr const char* MT_KEY_USER_METAMETHODS = "__user_metamethods";

    /// Key for storing polymorphic type flag on metatable
    /// Used to prevent unsafe operations on polymorphic types (like GetAddress)
    constexpr const char* MT_KEY_IS_POLYMORPHIC = "__is_polymorphic";

    // ============================================================================
    // Helper Functions
    // ============================================================================

    /**
     * @brief Stores a value on a metatable if not already present
     *
     * This helper implements the "store once" pattern used for metatable metadata.
     * Since metatables are shared across instances, we only need to store metadata
     * once per metatable type.
     *
     * @param L Lua state
     * @param metatable_index Stack index of the metatable
     * @param key The key to store the value under
     * @param push_value Function that pushes the value onto the stack
     * @return true if the value was stored, false if already present
     *
     * Example:
     * @code
     * store_on_metatable_if_absent(L, -1, MT_KEY_IS_POLYMORPHIC, [&]() {
     *     lua_pushboolean(L, true);
     * });
     * @endcode
     */
    template<typename PushValueFunc>
    inline bool store_on_metatable_if_absent(lua_State* L, int metatable_index, const char* key, PushValueFunc push_value)
    {
        // Convert to absolute index if negative
        if (metatable_index < 0 && metatable_index > LUA_REGISTRYINDEX)
        {
            metatable_index = lua_gettop(L) + metatable_index + 1;
        }

        // Check if key already exists
        lua_pushstring(L, key);
        lua_rawget(L, metatable_index);

        if (!lua_isnil(L, -1))
        {
            lua_pop(L, 1); // Pop existing value
            return false;  // Already present
        }

        lua_pop(L, 1); // Pop nil

        // Store the new value
        lua_pushstring(L, key);
        push_value();
        lua_rawset(L, metatable_index);

        return true;
    }

    /**
     * @brief Gets a value from a metatable
     *
     * @param L Lua state
     * @param metatable_index Stack index of the metatable
     * @param key The key to retrieve
     * @return true if the value was found and pushed, false if nil (nil is still pushed)
     */
    inline bool get_from_metatable(lua_State* L, int metatable_index, const char* key)
    {
        // Convert to absolute index if negative
        if (metatable_index < 0 && metatable_index > LUA_REGISTRYINDEX)
        {
            metatable_index = lua_gettop(L) + metatable_index + 1;
        }

        lua_pushstring(L, key);
        lua_rawget(L, metatable_index);

        return !lua_isnil(L, -1);
    }

    /**
     * @brief Gets member functions table from a userdata's metatable
     *
     * Looks up the member functions table first at MT_KEY_MEMBER_FUNCS,
     * then falls back to metatable[1] for backwards compatibility.
     *
     * @param L Lua state
     * @param userdata_index Stack index of the userdata
     * @return true if member functions table is now at top of stack, false otherwise
     *
     * On success: Pushes the member functions table
     * On failure: Pushes nil
     */
    inline bool get_member_funcs_table(lua_State* L, int userdata_index)
    {
        // Convert to absolute index if negative
        if (userdata_index < 0 && userdata_index > LUA_REGISTRYINDEX)
        {
            userdata_index = lua_gettop(L) + userdata_index + 1;
        }

        // Get the metatable
        if (!lua_getmetatable(L, userdata_index))
        {
            lua_pushnil(L);
            return false;
        }

        // Try MT_KEY_MEMBER_FUNCS first
        lua_pushstring(L, MT_KEY_MEMBER_FUNCS);
        lua_rawget(L, -2);

        if (lua_istable(L, -1))
        {
            lua_remove(L, -2); // Remove metatable, keep member funcs
            return true;
        }

        // Fallback: try metatable[1] for backwards compatibility
        lua_pop(L, 1); // Pop nil
        lua_rawgeti(L, -1, 1);

        if (lua_istable(L, -1))
        {
            lua_remove(L, -2); // Remove metatable, keep member funcs
            return true;
        }

        // Not found
        lua_pop(L, 2); // Pop nil and metatable
        lua_pushnil(L);
        return false;
    }

    /**
     * @brief Gets user metamethods container from a userdata's metatable
     *
     * @param L Lua state
     * @param userdata_index Stack index of the userdata
     * @return true if user metamethods userdata is now at top of stack, false otherwise
     */
    inline bool get_user_metamethods(lua_State* L, int userdata_index)
    {
        // Convert to absolute index if negative
        if (userdata_index < 0 && userdata_index > LUA_REGISTRYINDEX)
        {
            userdata_index = lua_gettop(L) + userdata_index + 1;
        }

        // Get the metatable
        if (!lua_getmetatable(L, userdata_index))
        {
            lua_pushnil(L);
            return false;
        }

        lua_pushstring(L, MT_KEY_USER_METAMETHODS);
        lua_rawget(L, -2);
        lua_remove(L, -2); // Remove metatable

        return lua_isuserdata(L, -1);
    }

    /**
     * @brief Checks if a userdata's type is marked as polymorphic
     *
     * Polymorphic types require special handling for operations like GetAddress
     * because the base pointer may not be the same as the derived pointer.
     *
     * @param L Lua state
     * @param userdata_index Stack index of the userdata
     * @return true if the type is polymorphic, false otherwise
     */
    inline bool is_polymorphic_type(lua_State* L, int userdata_index)
    {
        // Convert to absolute index if negative
        if (userdata_index < 0 && userdata_index > LUA_REGISTRYINDEX)
        {
            userdata_index = lua_gettop(L) + userdata_index + 1;
        }

        if (!lua_getmetatable(L, userdata_index))
        {
            return false;
        }

        lua_pushstring(L, MT_KEY_IS_POLYMORPHIC);
        lua_rawget(L, -2);
        bool result = lua_toboolean(L, -1);
        lua_pop(L, 2); // Pop boolean and metatable

        return result;
    }

    /**
     * @brief Formats a Lua stack value for diagnostic messages
     *
     * @param L Lua state
     * @param index Stack index of the value
     * @return A string description of the value (for error messages)
     */
    inline std::string format_value_for_diagnostics(lua_State* L, int index)
    {
        int type = lua_type(L, index);

        switch (type)
        {
            case LUA_TSTRING:
                return lua_tostring(L, index);
            case LUA_TNUMBER:
                return "number:" + std::to_string(lua_tonumber(L, index));
            case LUA_TBOOLEAN:
                return lua_toboolean(L, index) ? "true" : "false";
            case LUA_TNIL:
                return "nil";
            default:
                return std::string("type:") + lua_typename(L, type);
        }
    }

} // namespace RC::LuaMadeSimple::Luau

# Luau Compatibility Guide for UE4SS

This document details the compatibility layer implemented to support Luau (Roblox's Lua variant) as the scripting runtime for UE4SS, replacing standard Lua 5.x.

## Table of Contents
1. [Overview](#overview)
2. [Key Differences Between Lua 5.x and Luau](#key-differences-between-lua-5x-and-luau)
3. [Architecture Changes](#architecture-changes)
4. [File Reference](#file-reference)
5. [Common Patterns](#common-patterns)
6. [Lua Script Compatibility](#lua-script-compatibility)
7. [Troubleshooting](#troubleshooting)
8. [Future Considerations](#future-considerations)

---

## Overview

UE4SS has been ported from Lua 5.x to Luau to take advantage of Luau's:
- Better performance (faster VM, type inference)
- Enhanced safety features
- Modern language features (type annotations, continue statement)
- Active development by Roblox

The port required changes to the C++ binding layer (LuaMadeSimple) and some adjustments to Lua scripts.

---

## Key Differences Between Lua 5.x and Luau

### 1. Garbage Collection / Destructors

**Lua 5.x:**
```cpp
// Add __gc metamethod to metatable
lua_pushcfunction(L, destructor_func);
lua_setfield(L, -2, "__gc");
```

**Luau:**
```cpp
// Register destructor at userdata creation time
auto* ud = lua_newuserdatadtor(L, sizeof(Object), [](void* ptr) {
    static_cast<Object*>(ptr)->~Object();
});
```

**Rationale:** Luau doesn't support `__gc` metamethods for sandboxing reasons. Destructors must be registered when creating userdata using `lua_newuserdatadtor()`.

### 2. User Values / Environment Tables

**Lua 5.x:**
```cpp
// Multiple user values per userdata
lua_setiuservalue(L, index, n);  // Set nth user value
lua_getiuservalue(L, index, n);  // Get nth user value
```

**Luau:**
```cpp
// Single environment table per userdata
lua_setfenv(L, index);  // Set environment table
lua_getfenv(L, index);  // Get environment table
```

**Our Approach:** Instead of using environment tables (which differ between versions), we store metadata directly on metatables using well-known keys. This is more portable and metatables are shared across instances anyway.

Keys used (defined in `LuauCompat.hpp`):
- `__member_funcs` - Table of member functions for method dispatch
- `__user_metamethods` - Userdata containing custom metamethod handlers
- `__is_polymorphic` - Boolean flag for polymorphic C++ types

### 3. Bitwise Operators

**Lua 5.3+:**
```lua
local flags = FLAG_A | FLAG_B  -- bitwise OR
local masked = value & MASK    -- bitwise AND
local inverted = ~value        -- bitwise NOT
```

**Luau:**
```lua
local flags = bit32.bor(FLAG_A, FLAG_B)  -- bitwise OR
local masked = bit32.band(value, MASK)   -- bitwise AND
local inverted = bit32.bnot(value)       -- bitwise NOT
```

**Impact:** Any Lua scripts using `|`, `&`, `~`, `>>`, `<<` operators need to be updated to use `bit32.*` functions.

### 4. Standard Libraries

**Lua 5.x provides:**
- `io` - File I/O
- `os` - Operating system facilities
- `package` - Module loading
- `debug` - Debug facilities

**Luau provides:**
- No `io` library (sandboxed)
- No `os` library (sandboxed)
- No `package.searchers` (different module system)
- Limited `debug` library

**Our Solutions:**
- Custom `io` library implemented in `LuauIOLibrary.cpp`
- Custom `require` function that replaces Luau's built-in
- `package` table created manually with `path`, `cpath`, `loaded`, `searchers`

### 5. Module Loading

**Lua 5.x:**
```cpp
// Insert custom searcher into package.searchers
lua_getglobal(L, "package");
lua_getfield(L, -1, "searchers");
// ... insert function at searchers[1]
```

**Luau:**
```cpp
// Replace global require function entirely
lua_pushcclosure(L, custom_require_function, 1);
lua_setglobal(L, "require");
```

**Rationale:** Luau's built-in `require` doesn't use `package.searchers`. The simplest solution is to replace `require` with our own implementation.

---

## Architecture Changes

### Metatable Storage Pattern

Instead of storing per-instance data in environment tables, we store shared data on metatables:

```
Metatable
├── __member_funcs     → Table { method_name → function, ... }
├── __user_metamethods → Userdata (MetaMethodContainer)
├── __is_polymorphic   → boolean
├── __index            → function (dispatch handler)
├── __newindex         → function (if needed)
└── __metatable        → false (hide from Lua)
```

This pattern is implemented in `transfer_stack_object()` and `new_metatable()` in `LuaMadeSimple.hpp`.

### __index Metamethod Flow

The `__index` metamethod in `LuaMadeSimple.cpp` follows this logic:

```
1. Check if we have userdata at stack position 1
2. Get key info for diagnostics (save early!)
3. Try to find key in member functions table:
   a. Get metatable from userdata
   b. Look for __member_funcs key
   c. If found, look up the key in that table
   d. If method found, return it
4. If not found, try user __index:
   a. Get __user_metamethods from metatable
   b. Extract MetaMethodContainer
   c. Call user's index handler if present
5. If still not found, throw error with key info
```

### Helper Functions (LuauCompat.hpp)

```cpp
namespace RC::LuaMadeSimple::Luau {
    // Metatable key constants
    constexpr const char* MT_KEY_MEMBER_FUNCS = "__member_funcs";
    constexpr const char* MT_KEY_USER_METAMETHODS = "__user_metamethods";
    constexpr const char* MT_KEY_IS_POLYMORPHIC = "__is_polymorphic";

    // Store value on metatable if not already present
    template<typename PushValueFunc>
    bool store_on_metatable_if_absent(lua_State* L, int mt_idx, const char* key, PushValueFunc push);

    // Get value from metatable
    bool get_from_metatable(lua_State* L, int mt_idx, const char* key);

    // Get member functions table from userdata's metatable
    bool get_member_funcs_table(lua_State* L, int ud_idx);

    // Get user metamethods container
    bool get_user_metamethods(lua_State* L, int ud_idx);

    // Check if type is marked polymorphic
    bool is_polymorphic_type(lua_State* L, int ud_idx);

    // Format value for error messages
    std::string format_value_for_diagnostics(lua_State* L, int idx);
}
```

---

## File Reference

### Core Luau Compatibility

| File | Purpose |
|------|---------|
| `deps/first/LuaMadeSimple/include/LuaMadeSimple/LuauCompat.hpp` | Constants, helper functions, documentation |
| `deps/first/LuaMadeSimple/include/LuaMadeSimple/LuaMadeSimple.hpp` | Core Lua bindings (modified for Luau) |
| `deps/first/LuaMadeSimple/src/LuaMadeSimple.cpp` | Implementation (modified for Luau) |
| `deps/first/LuaMadeSimple/include/LuaMadeSimple/LuaObject.hpp` | Base object types (modified) |

### UE4SS Specific

| File | Purpose |
|------|---------|
| `UE4SS/include/Mod/LuauIOLibrary.hpp` | io library header |
| `UE4SS/src/Mod/LuauIOLibrary.cpp` | io library implementation |
| `UE4SS/src/Mod/LuaMod.cpp` | Module loading, Lua setup (modified) |

### Modified Lua Scripts

| File | Change |
|------|--------|
| `assets/Mods/ConsoleCommandsMod/Scripts/set.lua` | `\|` → `bit32.bor()` |

---

## Common Patterns

### Creating Userdata with Destructor

```cpp
// Luau pattern
auto* ud = static_cast<MyType*>(lua_newuserdatadtor(L, sizeof(MyType), [](void* ptr) {
    static_cast<MyType*>(ptr)->~MyType();
}));
new (ud) MyType(std::move(source));

// Set metatable
luaL_getmetatable(L, "MyTypeMT");
lua_setmetatable(L, -2);
```

### Storing Metadata on Metatable

```cpp
// Use the helper function
Luau::store_on_metatable_if_absent(L, metatable_idx, Luau::MT_KEY_IS_POLYMORPHIC, [&]() {
    lua_pushboolean(L, std::is_polymorphic_v<MyType>);
});
```

### Checking Polymorphic Type

```cpp
if (Luau::is_polymorphic_type(L, 1)) {
    luaL_error(L, "Operation not supported on polymorphic types");
}
```

### Custom require Implementation

```cpp
int custom_require_function(lua_State* L) {
    const char* module_name = luaL_checkstring(L, 1);

    // Check package.loaded cache
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_pushstring(L, module_name);
    lua_gettable(L, -2);
    if (!lua_isnil(L, -1)) {
        return 1;  // Return cached module
    }
    lua_pop(L, 3);

    // Search paths and load...
    // ...

    // Cache in package.loaded
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_pushstring(L, module_name);
    lua_pushvalue(L, -4);  // Copy module value
    lua_settable(L, -3);
    lua_pop(L, 2);

    return 1;
}
```

---

## Lua Script Compatibility

### Required Changes for Existing Scripts

1. **Bitwise operators** - Replace with bit32 functions:
   ```lua
   -- Before
   local flags = A | B | C
   local masked = value & MASK

   -- After
   local flags = bit32.bor(A, B, C)
   local masked = bit32.band(value, MASK)
   ```

2. **goto statements** - Not supported in Luau, refactor to use loops/returns

3. **setfenv/getfenv on functions** - Limited support, avoid if possible

4. **debug.setmetatable** - Not available, use standard metatable functions

### Supported Luau Features

Scripts can now use Luau-specific features:

```lua
-- Type annotations (ignored at runtime, useful for tooling)
local function greet(name: string): string
    return "Hello, " .. name
end

-- Continue statement
for i = 1, 10 do
    if i % 2 == 0 then
        continue
    end
    print(i)
end

-- Compound assignment
local x = 5
x += 10  -- x = x + 10
```

---

## Troubleshooting

### Common Errors

**Error: `Expected identifier when parsing expression, got '|'`**
- Cause: Script uses Lua 5.3+ bitwise operators
- Fix: Replace `|` with `bit32.bor()`, `&` with `bit32.band()`, etc.

**Error: `[__index] Member 'X' not found`**
- Cause: Method lookup failed and no user __index handler
- Check: Is the metatable properly set up? Is __member_funcs populated?

**Error: `[transfer_stack_object] Metatable 'X' not found`**
- Cause: new_metatable() wasn't called before transfer_stack_object()
- Fix: Ensure metatable is created first

**Error: `Call to RemoteObject:GetAddress on polymorphic type`**
- Cause: Trying to get address of polymorphic C++ type
- Fix: Override GetAddress in the derived type's Lua binding

### Debugging Tips

1. **Stack dumps**: Use `lua.dump_stack("label")` to see current stack state
2. **Key diagnostics**: Error messages include the key being accessed
3. **Metatable inspection**: Check metatable contents in Lua with:
   ```lua
   local mt = getmetatable(obj)
   for k, v in pairs(mt) do print(k, type(v)) end
   ```

---

## Future Considerations

### Potential Improvements

1. **Type checking**: Luau supports gradual typing - consider adding type annotations to Lua APIs

2. **Performance**: Luau's type inference can optimize hot paths - ensure critical code is type-stable

3. **Sandboxing**: Consider implementing security restrictions using Luau's sandboxing features

4. **Native codegen**: Luau supports native code generation - investigate enabling for performance

### Maintaining Compatibility

When updating Luau or making changes:

1. Run all existing mods to verify compatibility
2. Check for new Luau features that might conflict
3. Update this document with any new patterns or changes
4. Keep the `LuauCompat.hpp` helpers updated

### Testing Checklist

- [ ] Basic mod loading works
- [ ] UObject property access works
- [ ] Method calls on userdata work
- [ ] Custom __index handlers work (UObject property lookup)
- [ ] File I/O works (io.open, io.lines)
- [ ] Module require works (including shared modules)
- [ ] Garbage collection properly destructs userdata
- [ ] Bitwise operations work via bit32

---

## References

- [Luau Documentation](https://luau-lang.org/)
- [Luau GitHub](https://github.com/Roblox/luau)
- [Lua 5.4 Reference Manual](https://www.lua.org/manual/5.4/) (for comparison)
- [UE4SS Documentation](https://docs.ue4ss.com/)

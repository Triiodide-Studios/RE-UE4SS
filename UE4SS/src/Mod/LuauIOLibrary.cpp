/**
 * @file LuauIOLibrary.cpp
 * @brief Minimal io library implementation for Luau compatibility
 *
 * Luau (Roblox's Lua variant) doesn't include the standard `io` library by default
 * for sandboxing reasons. This file provides a minimal implementation of the most
 * commonly used io functions to maintain compatibility with existing Lua scripts.
 *
 * ## Supported Functions:
 * - io.open(filename, mode) - Opens a file and returns a file handle
 * - io.lines(filename) - Returns an iterator over lines in a file
 *
 * ## File Handle Methods:
 * - file:read(format) - Reads from file (*a, *l, *n, or number of bytes)
 * - file:write(...) - Writes strings to file
 * - file:close() - Closes the file
 * - file:lines() - Returns an iterator over lines
 *
 * ## Notes:
 * - File paths are converted to wide strings for Windows compatibility
 * - Uses std::fstream for portable file I/O
 * - Destructors are handled via lua_newuserdatadtor (Luau-specific)
 */

#include <Mod/LuauIOLibrary.hpp>
#include <Helpers/String.hpp>

#include <lua.hpp>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>

namespace RC
{
    // File handle userdata structure - stores the file stream and state
    struct LuaFileHandle
    {
        std::fstream file;
        bool is_open{false};
        std::string mode;
    };

    // Forward declaration for io.lines to use
    static int lua_io_open(lua_State* L);

    /**
     * @brief Creates and configures the LuaFileHandle metatable
     *
     * The metatable provides:
     * - close() method
     * - read() method with format support
     * - write() method with multiple arguments
     * - lines() method returning an iterator
     * - __index pointing to self for method lookup
     */
    static void create_file_handle_metatable(lua_State* L)
    {
        luaL_newmetatable(L, "LuaFileHandle");

        // file:close()
        lua_pushcclosurek(L, [](lua_State* L) -> int {
            auto* h = static_cast<LuaFileHandle*>(luaL_checkudata(L, 1, "LuaFileHandle"));
            if (h && h->is_open)
            {
                h->file.close();
                h->is_open = false;
            }
            lua_pushboolean(L, 1);
            return 1;
        }, "file:close", 0, nullptr);
        lua_setfield(L, -2, "close");

        // file:read(format)
        // Supports: "*a" (all), "*l" (line), "*n" (number), or integer (n bytes)
        lua_pushcclosurek(L, [](lua_State* L) -> int {
            auto* h = static_cast<LuaFileHandle*>(luaL_checkudata(L, 1, "LuaFileHandle"));
            if (!h || !h->is_open)
            {
                lua_pushnil(L);
                return 1;
            }

            const char* format = luaL_optstring(L, 2, "*l");

            if (strcmp(format, "*a") == 0 || strcmp(format, "*all") == 0)
            {
                // Read entire file from current position
                std::stringstream buffer;
                buffer << h->file.rdbuf();
                lua_pushstring(L, buffer.str().c_str());
            }
            else if (strcmp(format, "*l") == 0 || strcmp(format, "*line") == 0)
            {
                // Read single line (without newline)
                std::string line;
                if (std::getline(h->file, line))
                {
                    lua_pushstring(L, line.c_str());
                }
                else
                {
                    lua_pushnil(L);
                }
            }
            else if (strcmp(format, "*n") == 0)
            {
                // Read a number
                double num;
                if (h->file >> num)
                {
                    lua_pushnumber(L, num);
                }
                else
                {
                    lua_pushnil(L);
                }
            }
            else
            {
                // Assume it's a number - read n bytes
                int n = luaL_optinteger(L, 2, 0);
                if (n > 0)
                {
                    std::vector<char> buffer(n);
                    h->file.read(buffer.data(), n);
                    auto read_count = h->file.gcount();
                    if (read_count > 0)
                    {
                        lua_pushlstring(L, buffer.data(), static_cast<size_t>(read_count));
                    }
                    else
                    {
                        lua_pushnil(L);
                    }
                }
                else
                {
                    lua_pushnil(L);
                }
            }
            return 1;
        }, "file:read", 0, nullptr);
        lua_setfield(L, -2, "read");

        // file:write(...)
        // Writes all string arguments to the file
        lua_pushcclosurek(L, [](lua_State* L) -> int {
            auto* h = static_cast<LuaFileHandle*>(luaL_checkudata(L, 1, "LuaFileHandle"));
            if (!h || !h->is_open)
            {
                lua_pushnil(L);
                lua_pushstring(L, "file is closed");
                return 2;
            }

            int nargs = lua_gettop(L);
            for (int i = 2; i <= nargs; ++i)
            {
                size_t len;
                const char* str = luaL_checklstring(L, i, &len);
                h->file.write(str, len);
            }

            // Return file handle for method chaining
            lua_pushvalue(L, 1);
            return 1;
        }, "file:write", 0, nullptr);
        lua_setfield(L, -2, "write");

        // file:lines() - returns an iterator function
        lua_pushcclosurek(L, [](lua_State* L) -> int {
            luaL_checkudata(L, 1, "LuaFileHandle");

            // Create iterator function with file handle as upvalue
            lua_pushvalue(L, 1);
            lua_pushcclosure(L, [](lua_State* L) -> int {
                auto* h = static_cast<LuaFileHandle*>(lua_touserdata(L, lua_upvalueindex(1)));
                if (!h || !h->is_open)
                {
                    lua_pushnil(L);
                    return 1;
                }

                std::string line;
                if (std::getline(h->file, line))
                {
                    lua_pushstring(L, line.c_str());
                    return 1;
                }

                lua_pushnil(L);
                return 1;
            }, 1);

            return 1;
        }, "file:lines", 0, nullptr);
        lua_setfield(L, -2, "lines");

        // Set __index to self for method lookup
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
    }

    /**
     * @brief io.open(filename [, mode]) implementation
     *
     * Opens a file in the specified mode and returns a file handle.
     *
     * @param filename Path to the file (UTF-8 encoded)
     * @param mode Open mode: "r" (read), "w" (write), "a" (append), "r+", "w+", "a+", with optional "b" for binary
     * @return File handle on success, or nil + error message on failure
     */
    static int lua_io_open(lua_State* L)
    {
        const char* filename = luaL_checkstring(L, 1);
        const char* mode = luaL_optstring(L, 2, "r");

        // Create file handle userdata with destructor
        auto* handle = static_cast<LuaFileHandle*>(lua_newuserdatadtor(L, sizeof(LuaFileHandle), [](void* ud) {
            auto* h = static_cast<LuaFileHandle*>(ud);
            if (h->is_open)
            {
                h->file.close();
            }
            h->~LuaFileHandle();
        }));
        new (handle) LuaFileHandle();

        // Parse mode string and build std::ios flags
        std::ios_base::openmode open_mode{};
        handle->mode = mode;

        bool has_read = false;
        bool has_write = false;
        bool has_binary = false;

        for (const char* m = mode; *m; ++m)
        {
            switch (*m)
            {
            case 'r':
                has_read = true;
                break;
            case 'w':
                has_write = true;
                open_mode |= std::ios::trunc;
                break;
            case 'a':
                has_write = true;
                open_mode |= std::ios::app;
                break;
            case 'b':
                has_binary = true;
                break;
            case '+':
                has_read = true;
                has_write = true;
                break;
            }
        }

        if (has_read) open_mode |= std::ios::in;
        if (has_write) open_mode |= std::ios::out;
        if (has_binary) open_mode |= std::ios::binary;

        // Convert UTF-8 filename to wide string for Windows
        std::wstring wide_filename = utf8_to_wpath(filename);
        handle->file.open(wide_filename, open_mode);

        if (!handle->file.is_open())
        {
            lua_pop(L, 1); // Pop the userdata
            lua_pushnil(L);
            lua_pushstring(L, "cannot open file");
            return 2;
        }

        handle->is_open = true;

        // Get or create the file handle metatable
        luaL_getmetatable(L, "LuaFileHandle");
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            create_file_handle_metatable(L);
        }
        lua_setmetatable(L, -2);

        return 1;
    }

    /**
     * @brief io.lines(filename) implementation
     *
     * Opens a file for reading and returns an iterator function that reads lines.
     * The file is automatically closed when the iterator reaches EOF.
     *
     * @param filename Path to the file (UTF-8 encoded)
     * @return Iterator function, or raises an error if file cannot be opened
     */
    static int lua_io_lines(lua_State* L)
    {
        const char* filename = luaL_checkstring(L, 1);

        // Open the file using io.open
        lua_pushcfunction(L, lua_io_open);
        lua_pushstring(L, filename);
        lua_pushstring(L, "r");
        lua_call(L, 2, 2);

        if (lua_isnil(L, -2))
        {
            luaL_error(L, "cannot open file '%s'", filename);
            return 0;
        }
        lua_pop(L, 1); // Pop the nil error message

        // Create iterator that auto-closes on EOF
        lua_pushcclosure(L, [](lua_State* L) -> int {
            auto* h = static_cast<LuaFileHandle*>(lua_touserdata(L, lua_upvalueindex(1)));
            if (!h || !h->is_open)
            {
                lua_pushnil(L);
                return 1;
            }

            std::string line;
            if (std::getline(h->file, line))
            {
                lua_pushstring(L, line.c_str());
                return 1;
            }

            // EOF reached - close the file
            h->file.close();
            h->is_open = false;
            lua_pushnil(L);
            return 1;
        }, 1);

        return 1;
    }

    void setup_luau_io_library(lua_State* L)
    {
        // Create the io table
        lua_newtable(L);

        // Register io.open
        lua_pushcfunction(L, lua_io_open);
        lua_setfield(L, -2, "open");

        // Register io.lines
        lua_pushcfunction(L, lua_io_lines);
        lua_setfield(L, -2, "lines");

        // Set as global "io"
        lua_setglobal(L, "io");
    }

} // namespace RC

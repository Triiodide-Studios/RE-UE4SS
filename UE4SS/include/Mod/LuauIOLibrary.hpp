#pragma once

/**
 * @file LuauIOLibrary.hpp
 * @brief Minimal io library for Luau compatibility
 *
 * Luau doesn't include the standard io library by default (for sandboxing).
 * This provides commonly used io functions for script compatibility.
 *
 * See LuauIOLibrary.cpp for implementation details.
 */

struct lua_State;

namespace RC
{
    /**
     * @brief Sets up the minimal io library in the Lua state
     *
     * Creates a global "io" table with:
     * - io.open(filename, mode) -> file handle or nil, error
     * - io.lines(filename) -> line iterator
     *
     * @param L The Lua state to set up the library in
     */
    void setup_luau_io_library(lua_State* L);

} // namespace RC

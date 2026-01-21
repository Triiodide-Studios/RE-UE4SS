// lua.hpp wrapper for Luau compatibility
// This header provides a drop-in replacement for the standard Lua lua.hpp
// by including the Luau headers and our compatibility layer

#pragma once

// Include the Luau compatibility layer which provides:
// - Luau's lua.h and lualib.h
// - Compatibility shims for Lua 5.4 features
// - Bytecode compilation support
#include <LuauCompat.hpp>
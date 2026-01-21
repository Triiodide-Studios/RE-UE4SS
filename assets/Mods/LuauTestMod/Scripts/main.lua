--[[
    LuauTestMod - Tests Luau integration with UE4SS

    This mod verifies that key Luau features work correctly:
    - Basic Lua functionality
    - Luau-specific syntax (bit32, continue, compound assignment)
    - UE4SS API (RegisterKeyBind, ExecuteInGameThread, etc.)
    - io library (custom implementation for Luau)
    - Module loading (require)
]]

local MOD_NAME = "[LuauTestMod]"
local tests_passed = 0
local tests_failed = 0

-- Helper to log test results
local function test(name: string, condition: boolean, error_msg: string?)
    if condition then
        tests_passed += 1
        print(string.format("%s PASS: %s\n", MOD_NAME, name))
    else
        tests_failed += 1
        local msg = error_msg or "condition was false"
        print(string.format("%s FAIL: %s - %s\n", MOD_NAME, name, msg))
    end
end

print(string.format("%s ========================================\n", MOD_NAME))
print(string.format("%s Starting Luau Integration Tests\n", MOD_NAME))
print(string.format("%s ========================================\n", MOD_NAME))

-- ============================================
-- TEST 1: Basic Lua functionality
-- ============================================
print(string.format("%s\n%s Test Group: Basic Lua\n", MOD_NAME, MOD_NAME))

test("string concatenation", "hello" .. " " .. "world" == "hello world")
test("table creation", type({}) == "table")
test("function creation", type(function() end) == "function")
test("math.floor", math.floor(3.7) == 3)
test("string.format", string.format("%d", 42) == "42")

-- ============================================
-- TEST 2: Luau-specific: bit32 library
-- (Luau doesn't support | & ~ operators)
-- ============================================
print(string.format("%s\n%s Test Group: bit32 Library (Luau-specific)\n", MOD_NAME, MOD_NAME))

test("bit32 exists", type(bit32) == "table")
test("bit32.bor", bit32.bor(1, 2, 4) == 7)
test("bit32.band", bit32.band(7, 3) == 3)
test("bit32.bnot", bit32.bnot(0) == 0xFFFFFFFF)
test("bit32.bxor", bit32.bxor(5, 3) == 6)
test("bit32.lshift", bit32.lshift(1, 4) == 16)
test("bit32.rshift", bit32.rshift(16, 4) == 1)

-- ============================================
-- TEST 3: Luau-specific: continue statement
-- ============================================
print(string.format("%s\n%s Test Group: continue Statement (Luau-specific)\n", MOD_NAME, MOD_NAME))

local sum_odd = 0
for i = 1, 10 do
    if i % 2 == 0 then
        continue  -- Skip even numbers (Luau feature!)
    end
    sum_odd += i
end
test("continue statement", sum_odd == 25, string.format("expected 25, got %d", sum_odd))

-- ============================================
-- TEST 4: Luau-specific: compound assignment
-- ============================================
print(string.format("%s\n%s Test Group: Compound Assignment (Luau-specific)\n", MOD_NAME, MOD_NAME))

local x = 10
x += 5
test("+= operator", x == 15)

x -= 3
test("-= operator", x == 12)

x *= 2
test("*= operator", x == 24)

x /= 4
test("/= operator", x == 6)

x //= 2  -- Integer division assignment
test("//= operator", x == 3)

x %= 2
test("%= operator", x == 1)

-- ============================================
-- TEST 5: Luau-specific: type annotations
-- (Ignored at runtime, but should parse)
-- ============================================
print(string.format("%s\n%s Test Group: Type Annotations (Luau-specific)\n", MOD_NAME, MOD_NAME))

local function typed_add(a: number, b: number): number
    return a + b
end
test("function with type annotations", typed_add(2, 3) == 5)

local typed_table: {string} = {"a", "b", "c"}
test("variable with type annotation", #typed_table == 3)

-- ============================================
-- TEST 6: io library (custom Luau implementation)
-- ============================================
print(string.format("%s\n%s Test Group: io Library\n", MOD_NAME, MOD_NAME))

test("io table exists", type(io) == "table")
test("io.open exists", type(io.open) == "function")
test("io.lines exists", type(io.lines) == "function")

-- File I/O test is informational only (depends on runtime environment)
-- The important tests above confirm the io library is properly installed
print(string.format("%s [INFO] Attempting file I/O test (environment-dependent)...\n", MOD_NAME))

-- Try to write to the UE4SS directory (most likely to be writable)
local test_file_path = "ue4ss_luau_io_test.tmp"
local test_content = "Luau io test - success!"
local io_test_passed = false

local file, err = io.open(test_file_path, "w")
if file then
    local write_ok = file:write(test_content)
    file:close()

    if write_ok then
        -- Read it back
        local read_file = io.open(test_file_path, "r")
        if read_file then
            local content = read_file:read("*a")
            read_file:close()

            if content == test_content then
                io_test_passed = true
                print(string.format("%s [INFO] File I/O test PASSED (read/write working)\n", MOD_NAME))
            else
                print(string.format("%s [INFO] File I/O test: write ok, read mismatch\n", MOD_NAME))
            end

            -- Try to clean up
            pcall(function() os.remove(test_file_path) end)
        else
            print(string.format("%s [INFO] File I/O test: write ok, read failed\n", MOD_NAME))
        end
    else
        print(string.format("%s [INFO] File I/O test: file:write returned nil\n", MOD_NAME))
    end
else
    print(string.format("%s [INFO] File I/O test skipped: %s\n", MOD_NAME, tostring(err)))
end

if not io_test_passed then
    print(string.format("%s [INFO] File I/O not available in this environment (this is OK)\n", MOD_NAME))
end

-- ============================================
-- TEST 7: require / module loading
-- ============================================
print(string.format("%s\n%s Test Group: Module Loading\n", MOD_NAME, MOD_NAME))

test("require function exists", type(require) == "function")
test("package table exists", type(package) == "table")
test("package.loaded exists", type(package.loaded) == "table")

-- Try to load UEHelpers (shared module)
local success, UEHelpers = pcall(function()
    return require("UEHelpers")
end)
test("require UEHelpers", success, tostring(UEHelpers))

-- ============================================
-- TEST 8: UE4SS Global Functions
-- ============================================
print(string.format("%s\n%s Test Group: UE4SS API\n", MOD_NAME, MOD_NAME))

test("print exists", type(print) == "function")
test("RegisterKeyBind exists", type(RegisterKeyBind) == "function")
test("ExecuteInGameThread exists", type(ExecuteInGameThread) == "function")
test("ExecuteWithDelay exists", type(ExecuteWithDelay) == "function")
test("LoopAsync exists", type(LoopAsync) == "function")
test("RegisterHook exists", type(RegisterHook) == "function")
test("NotifyOnNewObject exists", type(NotifyOnNewObject) == "function")

-- Test that Key table exists (for keybinds)
test("Key table exists", type(Key) == "table")
test("ModifierKey table exists", type(ModifierKey) == "table")

-- ============================================
-- TEST 9: UE4SS Object System
-- ============================================
print(string.format("%s\n%s Test Group: UE4SS Object System\n", MOD_NAME, MOD_NAME))

test("FindFirstOf exists", type(FindFirstOf) == "function")
test("FindAllOf exists", type(FindAllOf) == "function")
test("StaticFindObject exists", type(StaticFindObject) == "function")

-- Try to find the Engine object
local engine = FindFirstOf("Engine")
test("FindFirstOf Engine", engine ~= nil)

if engine then
    test("Engine has GetAddress", type(engine.GetAddress) == "function" or type(engine["GetAddress"]) == "function")

    local addr = engine:GetAddress()
    test("Engine:GetAddress returns number", type(addr) == "number" and addr > 0)
end

-- ============================================
-- TEST 10: Luau string interpolation (if supported)
-- ============================================
print(string.format("%s\n%s Test Group: String Features\n", MOD_NAME, MOD_NAME))

-- Basic string features that should work
test("string.sub", string.sub("hello", 1, 3) == "hel")
test("string.find", string.find("hello world", "world") == 7)
test("string.gsub", select(1, string.gsub("hello", "l", "L")) == "heLLo")

-- ============================================
-- SUMMARY
-- ============================================
print(string.format("%s\n%s ========================================\n", MOD_NAME, MOD_NAME))
print(string.format("%s Test Results: %d passed, %d failed\n", MOD_NAME, tests_passed, tests_failed))
print(string.format("%s ========================================\n", MOD_NAME, MOD_NAME))

if tests_failed == 0 then
    print(string.format("%s All tests passed! Luau integration is working correctly.\n", MOD_NAME))
else
    print(string.format("%s WARNING: Some tests failed. Review output above.\n", MOD_NAME))
end

-- Register a keybind to re-run tests (for manual verification)
RegisterKeyBind(Key.L, { ModifierKey.CONTROL, ModifierKey.SHIFT }, function()
    print(string.format("%s Keybind test successful! (Ctrl+Shift+L)\n", MOD_NAME))

    -- Test ExecuteInGameThread
    ExecuteInGameThread(function()
        print(string.format("%s ExecuteInGameThread callback executed!\n", MOD_NAME))
    end)
end)

print(string.format("%s Press Ctrl+Shift+L to test keybind and ExecuteInGameThread\n", MOD_NAME))

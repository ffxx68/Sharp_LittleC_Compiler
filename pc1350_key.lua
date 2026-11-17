-- PC-1350 input ports: MAME src/mame/sharp/pocketc.cpp.
assert(manager.machine.system.name == "pc1350", "pc1350_key.lua requires PC-1350")

local keys = {}
local function bind(name, port, mask, shift)
    keys[name] = {port, mask, shift}
end

local rows = {
    {"QWERTYUIOP", 0x40, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}},
    {"ASDFGHJKL", 0x20, {1, 2, 3, 4, 5, 6, 7, 8, 9}},
    {"ZXCVBNM", 0x10, {1, 2, 3, 4, 5, 6, 7}}
}
for _, row in ipairs(rows) do
    for i, port in ipairs(row[3]) do
        bind(row[1]:sub(i, i), ":KEY" .. port, row[2])
    end
end
for port, digits in pairs({[2] = "963", [3] = "852", [4] = "741"}) do
    for i = 1, 3 do bind(digits:sub(i, i), ":KEY" .. port, 2 ^ (i - 1)) end
end
bind("0", ":KEY4", 0x08)
bind(")", ":KEY0", 0x01)
bind(":", ":KEY0", 0x02)
bind(";", ":KEY0", 0x04)
bind(",", ":KEY0", 0x08)
bind("(", ":KEY1", 0x01)
bind("/", ":KEY1", 0x02)
bind("*", ":KEY1", 0x04)
bind("-", ":KEY1", 0x08)
bind("+", ":KEY2", 0x08)
bind(".", ":KEY3", 0x08)
bind(" ", ":KEY8", 0x10)
bind("=", ":KEY10", 0x20)
for i, char in ipairs({"!", '"', "#", "$", "%", "&", "?"}) do
    bind(char, ":KEY" .. i, 0x40, true)
end
bind("<", ":KEY1", 0x01, true)
bind(">", ":KEY0", 0x01, true)
bind("^", ":KEY1", 0x08, true)
bind("SML", ":KEY0", 0x10)
bind("DEF", ":KEY0", 0x20)
bind("SHIFT", ":KEY0", 0x40)
bind("UP", ":KEY5", 0x01)
bind("DOWN", ":KEY5", 0x02)
bind("LEFT", ":KEY5", 0x04)
bind("RIGHT", ":KEY5", 0x08)
bind("INS", ":KEY6", 0x04)
bind("DEL", ":KEY6", 0x08)
bind("MODE", ":KEY7", 0x08)
bind("CLS", ":KEY8", 0x08)
bind("ENTER", ":KEY9", 0x10)
bind("\r", ":KEY9", 0x10)
bind("\n", ":KEY9", 0x10)
bind("BRK", ":EXTRA", 0x01)

local function field_for(port, mask)
    local input = assert(manager.machine.ioport.ports[port], "Input port not found: " .. port)
    for _, field in pairs(input.fields) do
        if field.mask == mask then return field end
    end
    error(string.format("Input field not found: %s mask 0x%02X", port, mask))
end

-- Call these helpers from a coroutine: emu.wait yields between presses.
function key_press(port, mask)
    local field = field_for(port, mask)
    field:set_value(1)
    emu.wait(0.05)
    field:set_value(0)
    emu.wait(0.1)
end

local function lookup(char)
    assert(type(char) == "string", "Expected a character or named key")
    return assert(keys[char:upper()], "Unsupported PC-1350 key: " .. char)
end

function key(char)
    local entry = lookup(char)
    if #char == 1 and char:match("%a") then
        local small = manager.machine.devices[":maincpu"].spaces.program:read_u8(0x783c) >= 128
        if small ~= (char:match("%l") ~= nil) then key_press(":KEY0", 0x10) end
    end
    if entry[3] then key_press(":KEY0", 0x40) end
    key_press(entry[1], entry[2])
end

function keyline(text)
    assert(type(text) == "string", "Expected a line of text")
    for i = 1, #text do lookup(text:sub(i, i)) end
    print(text)
    for i = 1, #text do key(text:sub(i, i)) end
    key("ENTER")
    emu.wait(0.2)
end

function keyfile(path)
    local file = assert(io.open(path, "rb"))
    local text = assert(file:read("*a"))
    file:close()
    text = text:gsub("\r\n", "\n"):gsub("\r", "\n")
    if #text > 0 and text:sub(-1) ~= "\n" then text = text .. "\n" end
    for line in text:gmatch("(.-)\n") do keyline(line) end
end

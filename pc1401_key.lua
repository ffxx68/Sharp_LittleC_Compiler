-- PC-1401 input ports: MAME src/mame/sharp/pocketc.cpp.
assert(manager.machine.system.name == "pc1401", "pc1401_key.lua requires PC-1401")

local keys = {}
local function bind(name, port, mask, shift)
    keys[name] = {port, mask, shift}
end

for _, row in ipairs({
    {"QWE", 0x20, {0, 1, 2}},
    {"RTYUI", 0x20, {6, 7, 8, 9, 10}},
    {"ASD", 0x40, {0, 1, 2}},
    {"FGHJKL", 0x40, {6, 7, 8, 9, 10, 11}},
    {"ZXC", 0x80, {0, 1, 2}},
    {"VBNM", 0x80, {6, 7, 8, 9}}
}) do
    for i, port in ipairs(row[3]) do
        bind(row[1]:sub(i, i), ":KEY" .. port, row[2])
    end
end
bind("O", ":KEY8", 0x08)
bind("P", ":KEY7", 0x08)
for port, digits in pairs({[6] = "714", [0] = "825", [1] = "936"}) do
    for i = 1, 3 do bind(digits:sub(i, i), ":KEY" .. port, 2 ^ i) end
end
bind("0", ":KEY12", 0x80)
bind(".", ":KEY1", 0x01)
bind("+", ":KEY2", 0x01)
bind("/", ":KEY2", 0x02)
bind("-", ":KEY2", 0x04)
bind("*", ":KEY2", 0x08)
bind(")", ":KEY3", 0x01)
bind("(", ":KEY3", 0x02)
bind("=", ":KEY3", 0x80)
bind(",", ":KEY7", 0x04)
bind(" ", ":KEY10", 0x80)
for char, letter in pairs({
    ["!"] = "Q", ['"'] = "W", ["#"] = "E", ["$"] = "R",
    ["%"] = "T", ["&"] = "Y", ["?"] = "U", ["@"] = "I",
    [":"] = "O", [";"] = "P"
}) do
    local entry = keys[letter]
    bind(char, entry[1], entry[2], true)
end
bind("^", ":KEY2", 0x01, true)
bind(">", ":KEY2", 0x04, true)
bind("<", ":KEY2", 0x08, true)
bind("CAL", ":KEY0", 0x10)
bind("_", ":KEY0", 0x10)
bind("BASIC", ":KEY1", 0x10)
bind("|", ":KEY1", 0x10)
bind("DEF", ":KEY2", 0x10)
bind("SHIFT", ":KEY5", 0x40)
bind("CLS", ":KEY5", 0x01)
bind("C-CE", ":KEY5", 0x01)
bind("UP", ":KEY7", 0x10)
bind("DOWN", ":KEY6", 0x10)
bind("LEFT", ":KEY8", 0x10)
bind("RIGHT", ":KEY9", 0x10)
bind("DEL", ":KEY8", 0x10, true)
bind("INS", ":KEY9", 0x10, true)
bind("ENTER", ":KEY11", 0x80)
bind("\r", ":KEY11", 0x80)
bind("\n", ":KEY11", 0x80)
bind("BRK", ":EXTRA", 0x01)
bind("ON", ":EXTRA", 0x01)

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
    assert(not (#char == 1 and char:match("%l")),
        "PC-1401 has no lowercase input: " .. char)
    return assert(keys[char:upper()], "Unsupported PC-1401 key: " .. char)
end

function key(char)
    local entry = lookup(char)
    if entry[3] then key_press(":KEY5", 0x40) end
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

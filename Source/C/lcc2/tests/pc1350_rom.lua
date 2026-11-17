-- Disposable PC-1350 session: overwrites BASIC, firmware scratch and test RAM.
function lcc2_pc1350_rom_test(binary_path, listing_path)
    assert(manager.machine.system.name == "pc1350", "PC-1350 required")
    if lcc2_pc1350_rom_subscription then lcc2_pc1350_rom_subscription:unsubscribe() end
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces.program
    local ram = emu.item(cpu.items["0/m_ram"])
    local function load(path)
        local file = assert(io.open(path, "rb"))
        local data = file:read("*a")
        file:close()
        return data
    end
    local binary, listing = load(binary_path), load(listing_path)
    local origin, trap = 0x6030, 0x6700
    assert(origin + #binary < trap, "binary overlaps return trap/test RAM")
    assert(space:read_u8(0xfff0) == 3, "version-1 ROM required")
    for i = 1, #binary do space:write_u8(origin + i - 1, binary:byte(i)) end
    space:write_u8(trap, 0x79)
    space:write_u8(trap + 1, 0x67)
    space:write_u8(trap + 2, 0)
    local function word(address)
        return space:read_u8(address) + 256 * space:read_u8(address + 1)
    end
    local function bytes(address, length)
        local result = {}
        for i = 0, length - 1 do
            result[#result + 1] = string.char(space:read_u8(address + i))
        end
        return table.concat(result)
    end
    local function write(address, text)
        for i = 1, #text do space:write_u8(address + i - 1, text:byte(i)) end
    end
    local function entry(name)
        return assert(tonumber(listing:match(name:upper() .. " = (%d+)")),
            "missing assembler symbol " .. name)
    end
    local vectors = {
        {name = "STR$", routine = "pc1350_str", text = "", result = 3, formatted = "123"},
        {name = "VAL", routine = "pc1350_val", text = "123", result = 0},
        {name = "VAL nonnumeric prefix", routine = "pc1350_val", text = "XYZ",
            result = 0, number = string.rep("\0", 8)},
        {name = "VAL carry error (stub)", routine = "pc1350_val", text = "123",
            mock_carry = true, result = 65533},
        {name = "VAL 79", routine = "pc1350_val", text = string.rep("0", 79),
            result = 0, number = string.rep("\0", 8)},
        {name = "VAL empty", routine = "pc1350_val", text = "", result = 65534},
        {name = "VAL 80", routine = "pc1350_val", text = string.rep("1", 80), result = 65534},
        {name = "combined 81", left = string.rep("A", 80), right = "B", result = 65534},
        {name = "combined 160", left = string.rep("A", 80), right = string.rep("A", 80),
            result = 65534},
        {name = "combined 80", left = string.rep("A", 40), right = string.rep("A", 40),
            result = 1},
        {name = "empty equal", left = "", right = "", result = 1},
        {name = "empty less", left = "", right = "A", operation = 2, result = 1},
        {name = "invalid operation", left = "A", right = "A", operation = 6, result = 65534}
    }
    for operation = 0, 5 do
        vectors[#vectors + 1] = {name = "predicate " .. operation,
            left = "A", right = "B", operation = operation,
            result = ({0, 1, 1, 0, 1, 0})[operation + 1]}
    end
    for _, value in ipairs({0, 31, 127, 254, 255}) do
        for _, routine in ipairs({"pc1350_val", "pc1350_compare"}) do
            vectors[#vectors + 1] = {name = routine .. " byte " .. value,
                routine = routine, text = string.char(value), left = string.char(value),
                right = "", result = 65534}
        end
    end
    for _, address in ipairs({8, 24, 64, 88, 0x67fc}) do
        for _, routine in ipairs({"pc1350_str", "pc1350_val"}) do
            vectors[#vectors + 1] = {name = routine .. " pointer " .. address,
                routine = routine, address = address, text = "123",
                result = routine == "pc1350_str" and 3 or 0,
                formatted = routine == "pc1350_str" and "123" or nil}
        end
    end
    for _, address in ipairs({0, 7, 89}) do
        for _, routine in ipairs({"pc1350_str", "pc1350_val"}) do
            vectors[#vectors + 1] = {name = routine .. " invalid pointer " .. address,
                routine = routine, address = address, text = "123", result = 65534}
        end
    end
    for _, routine in ipairs({"pc1350_str", "pc1350_val", "pc1350_compare"}) do
        vectors[#vectors + 1] = {name = routine .. " unsupported ROM",
            routine = routine, text = "", left = "A", right = "B",
            address = 7, operation = 6, unsupported = true, result = 65535}
    end
    lcc2_pc1350_rom_result = {status = "running", vectors = 0}
    local result = lcc2_pc1350_rom_result
    local vector, saved, bcd, patch, stage, frames = nil, {}, nil, nil, "program", 0
    local function restore_rom()
        -- Restore ROM through direct access, not its read-only address mapping.
        if patch then
            for i = 1, #patch do space:write_direct_u8(0x8d57 + i, patch:byte(i)) end
            patch = nil
        end
        space:write_direct_u8(0xfff0, 3)
    end
    local function prepare(address, args)
        for i = 8, 95 do ram:write(i, (i * 3 + 17) % 256) end
        ram:write(0x50, 0)
        ram:write(0x51, 0x67)
        for i, value in ipairs(args) do ram:write(0x51 + i, value) end
        cpu.state.R.value = 0x50
        cpu.state.PC.value = address
        frames = 0
    end
    prepare(origin, {})
    lcc2_pc1350_rom_subscription = emu.add_machine_frame_notifier(function()
        if result.status ~= "running" then return end
        local ok, message = pcall(function()
            frames = frames + 1
            assert(frames < 120, "timeout at " .. string.format("%04X", cpu.state.PC.value))
            if cpu.state.PC.value ~= trap then return end
            assert(cpu.state.R.value == 0x52, "unbalanced stack")
            if stage == "program" then
                local expected = {0, 3, 0, 1, 1, 0, 1, 0, 65534, 65534, 1}
                for i, value in ipairs(expected) do
                    assert(word(0x6810 + 2 * (i - 1)) == value,
                        "C fixture result " .. i .. ": " .. word(0x6810 + 2 * (i - 1)))
                end
                bcd = bytes(0x6800, 8)
                stage = "vectors"
            else
                restore_rom()
                local actual = ram:read(2) + 256 * ram:read(3)
                assert(actual == vector.result, "result " .. actual .. ", expected " .. vector.result)
                assert(space:read_u8(0x6f2f) == 0x71, "buffer pointer not restored")
                if vector.formatted then
                    assert(bytes(0x6e60, actual) == vector.formatted, "wrong STR$ text")
                end
                for i = 8, 95 do
                    local expected = saved[i]
                    if vector.routine == "pc1350_val" and actual == 0
                        and i >= vector.address and i < vector.address + 8 then
                        expected = (vector.number or bcd):byte(i - vector.address + 1)
                    end
                    assert(ram:read(i) == expected, "caller IRAM changed at " .. i)
                end
                if vector.routine == "pc1350_val" and vector.address >= 256 then
                    assert(bytes(vector.address, 8) == (actual == 0 and (vector.number or bcd)
                        or string.rep("\165", 8)),
                        "VAL destination changed incorrectly")
                end
                result.vectors = result.vectors + 1
            end
            vector = vectors[result.vectors + 1]
            if not vector then
                result.status = "PASS"
                print("PASS: PC1350 version-1 ROM strings, C ABI and " .. result.vectors .. " vectors")
                return
            end
            vector.routine = vector.routine or "pc1350_compare"
            vector.address = vector.address or 0x6800
            local address = vector.address
            local args = {math.floor(address / 256), address % 256}
            if vector.routine == "pc1350_val" then
                args[3] = #vector.text
            elseif vector.routine == "pc1350_compare" then
                args = {#vector.right, #vector.left, vector.operation or 0}
            end
            prepare(entry(vector.routine), args)
            if vector.routine ~= "pc1350_compare" and address >= 8 and address <= 88 then
                for i = 0, 7 do ram:write(address + i, bcd:byte(i + 1)) end
            elseif address >= 256 then
                write(address, vector.routine == "pc1350_str" and bcd or string.rep("\165", 8))
            end
            write(0x6e60, vector.routine == "pc1350_compare"
                and (vector.left .. vector.right) or vector.text)
            space:write_u8(0x6f2f, 0x71)
            if vector.mock_carry then
                patch = bytes(0x8d58, 2)
                space:write_direct_u8(0x8d58, 0xd0)
                space:write_direct_u8(0x8d59, 0x37)
                assert(space:read_u8(0x8d58) == 0xd0, "could not install carry-error stub")
            end
            if vector.unsupported then
                space:write_direct_u8(0xfff0, 0xa5)
                assert(space:read_u8(0xfff0) == 0xa5, "could not install unknown ROM signature")
            end
            for i = 8, 95 do saved[i] = ram:read(i) end
            result.current = vector.name
        end)
        if not ok then
            restore_rom()
            result.status = "FAIL"
            result.error = (result.current or stage) .. ": " .. tostring(message)
            print("FAIL: " .. result.error)
        end
    end)
end

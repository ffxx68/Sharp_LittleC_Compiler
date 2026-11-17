-- Preserves the active session's code/RAM/CPU state; leaves the LCD enabled.
function lcc2_pc1350_lcd_test(binary_path, listing_path)
    assert(manager.machine.system.name == "pc1350", "PC-1350 required")
    assert(not lcc2_pc1350_lcd_result or lcc2_pc1350_lcd_result.status ~= "running",
        "LCD test already running")
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces.program
    local ram = emu.item(cpu.items["0/m_ram"])
    local control = emu.item(cpu.items["0/m_c"])
    local function load(path)
        local file = assert(io.open(path, "rb"))
        local text = assert(file:read("*a"))
        file:close()
        return text
    end
    local binary, listing = load(binary_path), load(listing_path)
    local origin = 0x6900
    local trap = origin + #binary
    assert(trap + 3 < 0x6c30, "fixture overlaps firmware RAM")
    local function entry(name)
        return assert(tonumber(listing:match(name:upper() .. " = (%d+)")),
            "missing assembler symbol " .. name)
    end
    local on, off = entry("lcd_on"), entry("lcd_off")
    local original, registers, iram, display = {}, {}, {}, {}
    for i = 0, #binary + 2 do original[i] = space:read_u8(origin + i) end
    for _, name in ipairs({"PC", "DP", "P", "Q", "R", "Carry", "Zero"}) do
        registers[name] = cpu.state[name].value
    end
    for i = 0, 95 do iram[i] = ram:read(i) end
    for i = 0x7000, 0x7eff do display[i] = space:read_u8(i) end
    local original_control = control:read(0)
    local function cleanup(success)
        for i = 0, #binary + 2 do space:write_u8(origin + i, original[i]) end
        for i = 0, 95 do ram:write(i, iram[i]) end
        for name, value in pairs(registers) do cpu.state[name].value = value end
        control:write(0, success and (original_control | 1) or original_control)
        if success then ram:write(95, iram[95] | 1) end
        lcc2_pc1350_lcd_subscription:unsubscribe()
    end
    for i = 1, #binary do space:write_u8(origin + i - 1, binary:byte(i)) end
    space:write_u8(trap, 0x79)
    space:write_u8(trap + 1, math.floor(trap / 256))
    space:write_u8(trap + 2, trap % 256)
    lcc2_pc1350_lcd_result = {status = "running", vectors = 0}
    local result = lcc2_pc1350_lcd_result
    local vectors = {}
    -- Never set HALT/OFF bits 2/3 in a live session.
    for value = 0, 255 do
        if (value & 0x0c) == 0 then
            for _, enabled in ipairs({false, true}) do
                vectors[#vectors + 1] = {value = value, enabled = enabled}
            end
        end
    end
    vectors[#vectors + 1] = {value = original_control & 0xf3, enabled = false, hold = 60}
    vectors[#vectors + 1] = {value = original_control & 0xf3, enabled = true, hold = 60}
    local index, frames, remaining, saved = 0, 0, nil, {}
    local vector
    local function start()
        index = index + 1
        vector = vectors[index]
        if not vector then
            cleanup(true)
            result.status = "PASS"
            print("PASS: PC1350 LCD control, stack/IRAM and display RAM; " .. result.vectors .. " vectors")
            return
        end
        for i = 8, 95 do ram:write(i, (i * 3 + index) % 256) end
        ram:write(95, vector.value)
        control:write(0, vector.value)
        ram:write(0x50, trap % 256)
        ram:write(0x51, math.floor(trap / 256))
        for i = 8, 95 do saved[i] = ram:read(i) end
        cpu.state.R.value = 0x50
        cpu.state.PC.value = vector.enabled and on or off
        frames, remaining = 0, nil
    end
    lcc2_pc1350_lcd_subscription = emu.add_machine_frame_notifier(function()
        if result.status ~= "running" then return end
        local ok, message = pcall(function()
            frames = frames + 1
            assert(frames < 100, "timeout at " .. string.format("%04X", cpu.state.PC.value))
            if cpu.state.PC.value ~= trap then return end
            if remaining then
                remaining = remaining - 1
                if remaining == 0 then start() end
                return
            end
            assert(cpu.state.R.value == 0x52, "unbalanced stack")
            local expected = vector.enabled and (vector.value | 1) or (vector.value & 0xfe)
            assert(control:read(0) == expected,
                string.format("OUTC = %02X, expected %02X", control:read(0), expected))
            for i = 8, 95 do
                assert(ram:read(i) == saved[i], "IRAM changed at " .. i)
            end
            for i = 0x7000, 0x7eff do
                assert(space:read_u8(i) == display[i], "display RAM changed at " .. i)
            end
            result.vectors = result.vectors + 1
            if vector.hold then
                print("PC1350 LCD " .. (vector.enabled and "ON" or "OFF") .. " (3 seconds)")
                remaining = vector.hold
            else
                start()
            end
        end)
        if not ok then
            cleanup(false)
            result.status = "FAIL"
            result.error = "vector " .. index .. ": " .. tostring(message)
            print("FAIL: " .. result.error)
        end
    end)
    start()
end

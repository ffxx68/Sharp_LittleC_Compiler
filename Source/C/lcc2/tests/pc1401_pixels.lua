-- Borrows program/display RAM until a key is pressed, then restores the session.
function lcc2_pc1401_pixels_test(binary_path)
    assert(manager.machine.system.name == "pc1401", "PC-1401 required")
    assert(not lcc2_pc1401_pixels_result
        or lcc2_pc1401_pixels_result.status == "PASS"
        or lcc2_pc1401_pixels_result.status == "FAIL", "Pixel test already active")
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces.program
    local ram = emu.item(cpu.items["0/m_ram"])
    local control = emu.item(cpu.items["0/m_c"])
    local file = assert(io.open(binary_path, "rb"))
    local binary = assert(file:read("*a"))
    file:close()
    local origin, trap, ready = 0x3801, 0x46f0, 0x4700
    assert(origin + #binary < trap, "pixel fixture overlaps return trap")
    local memory, internal, registers = {}, {}, {}
    for _, range in ipairs({{0x3800, 0x47ff}, {0x6000, 0x67ff}}) do
        for i = range[1], range[2] do memory[i] = space:read_u8(i) end
    end
    for i = 0, 95 do internal[i] = ram:read(i) end
    for _, name in ipairs({"PC", "DP", "P", "Q", "R", "Carry", "Zero"}) do
        registers[name] = cpu.state[name].value
    end
    local entry_r = registers.R
    assert(entry_r >= 4 and entry_r <= 95, "unsupported entry stack")
    local old_control = control:read(0)
    local function restore()
        for address, value in pairs(memory) do space:write_u8(address, value) end
        for i = 0, 95 do ram:write(i, internal[i]) end
        control:write(0, old_control)
        for name, value in pairs(registers) do cpu.state[name].value = value end
        lcc2_pc1401_pixels_subscription:unsubscribe()
    end
    for i = 1, #binary do space:write_u8(origin + i - 1, binary:byte(i)) end
    space:write_u8(trap, 0x79)
    space:write_u8(trap + 1, math.floor(trap / 256))
    space:write_u8(trap + 2, trap % 256)
    space:write_u8(ready, 0)
    ram:write(entry_r - 2, trap % 256)
    ram:write(entry_r - 1, math.floor(trap / 256))
    cpu.state.R.value = entry_r - 2
    cpu.state.PC.value = origin
    lcc2_pc1401_pixels_result = {status = "drawing"}
    local result = lcc2_pc1401_pixels_result
    local frames = 0
    lcc2_pc1401_pixels_subscription = emu.add_machine_frame_notifier(function()
        local ok, message = pcall(function()
            frames = frames + 1
            if result.status == "drawing" then
                assert(frames < 600,
                    string.format("drawing timeout at %04X", cpu.state.PC.value))
                if space:read_u8(ready) ~= 42 then return end
                for x = 0, 79 do
                    local expected = 0x41
                    if x % 4 == 0 then expected = expected | (1 << (x % 7)) end
                    if x == 0 or x == 79 then expected = 0x7f end
                    local address = x < 40 and (0x6400 + x) or (0x6440 + 79 - x)
                    assert(space:read_u8(address) == expected,
                        string.format("pixel data mismatch at %04X", address))
                end
                assert((control:read(0) & 1) == 1, "LCD disabled")
                result.status = "waiting"
                result.checked_bytes = 80
                print("PC1401: 80 LCD bytes verified; waiting in key_wait(). Press a key to finish.")
            end
            if cpu.state.PC.value == trap then
                assert(result.status == "waiting", "returned before drawing")
                assert(cpu.state.R.value == entry_r, "stack imbalance")
                restore()
                result.status = "PASS"
                print("PASS: PC1401 pixel test completed; previous session restored.")
            end
        end)
        if not ok then
            restore()
            result.status = "FAIL"
            result.error = tostring(message)
            print("FAIL: " .. result.error)
        end
    end)
end

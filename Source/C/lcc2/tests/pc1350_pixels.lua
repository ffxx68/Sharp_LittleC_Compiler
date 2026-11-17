-- Borrows BASIC RAM while running; restores the active session after a key.
function lcc2_pc1350_pixels_test(binary_path, listing_path)
    assert(manager.machine.system.name == "pc1350", "PC-1350 required")
    assert(not lcc2_pc1350_pixels_result
        or lcc2_pc1350_pixels_result.status == "PASS"
        or lcc2_pc1350_pixels_result.status == "FAIL", "Pixel test already active")
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces.program
    local ram = emu.item(cpu.items["0/m_ram"])
    local control = emu.item(cpu.items["0/m_c"])
    local function load(path)
        local file = assert(io.open(path, "rb"))
        local data = assert(file:read("*a"))
        file:close()
        return data
    end
    local binary, listing = load(binary_path), load(listing_path)
    local origin, trap = 0x6030, 0x6800
    assert(origin + #binary < trap, "pixel fixture overlaps return trap")
    local wait_loop = assert(tonumber(listing:match("PC1350_KEY_WAIT_LOOP = (%d+)")))
    local memory, internal, registers = {}, {}, {}
    for i = 0x6000, 0x7eff do memory[i] = space:read_u8(i) end
    for i = 0, 95 do internal[i] = ram:read(i) end
    for _, name in ipairs({"PC", "DP", "P", "Q", "R", "Carry", "Zero"}) do
        registers[name] = cpu.state[name].value
    end
    local old_control = control:read(0)
    local function restore()
        for i = 0x6000, 0x7eff do space:write_u8(i, memory[i]) end
        for i = 0, 95 do ram:write(i, internal[i]) end
        control:write(0, old_control)
        for name, value in pairs(registers) do cpu.state[name].value = value end
        lcc2_pc1350_pixels_subscription:unsubscribe()
    end
    for i = 1, #binary do space:write_u8(origin + i - 1, binary:byte(i)) end
    space:write_u8(trap, 0x79)
    space:write_u8(trap + 1, math.floor(trap / 256))
    space:write_u8(trap + 2, trap % 256)
    space:write_u8(0x6810, 0)
    local entry_r = registers.R
    ram:write(entry_r - 2, trap % 256)
    ram:write(entry_r - 1, math.floor(trap / 256))
    cpu.state.R.value = entry_r - 2
    cpu.state.PC.value = origin
    lcc2_pc1350_pixels_result = {status = "drawing"}
    local result = lcc2_pc1350_pixels_result
    local frames = 0
    lcc2_pc1350_pixels_subscription = emu.add_machine_frame_notifier(function()
        local ok, message = pcall(function()
            frames = frames + 1
            if result.status == "drawing" then
                assert(frames < 2000,
                    string.format("drawing timeout at %04X", cpu.state.PC.value))
                if space:read_u8(0x6810) ~= 42 then return end
                for bank = 0, 4 do
                    for column = 0, 29 do
                        local x = bank * 30 + column
                        for row, offset in ipairs({0, 0x40, 0x1e, 0x5e}) do
                            local expected = 0
                            if x >= 10 and x < 50 then
                                expected = (x & 4) == 0 and 0x55 or 0xaa
                            end
                            if x >= 60 and x < 92 and row - 1 == ((x - 60) >> 3) then
                                expected = expected | (1 << ((x - 60) & 7))
                            end
                            if row == 1 then expected = expected | 1 end
                            if row == 4 then expected = expected | 0x80 end
                            if x == 0 or x == 149 then expected = 0xff end
                            local address = 0x7000 + bank * 0x200 + offset + column
                            assert(space:read_u8(address) == expected,
                                string.format("pixel data mismatch at %04X", address))
                        end
                    end
                end
                assert((control:read(0) & 1) == 1, "LCD disabled")
                result.status = "waiting"
                result.checked_bytes = 600
                result.key_wait_loop = wait_loop
                print("PC1350: 600 LCD bytes verified; waiting in key_wait(). Press a key to finish.")
            end
            if cpu.state.PC.value == trap then
                assert(result.status == "waiting", "returned before drawing")
                assert(cpu.state.R.value == entry_r, "stack imbalance")
                restore()
                result.status = "PASS"
                print("PASS: PC1350 pixel test completed; previous session restored.")
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

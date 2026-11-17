-- Borrows BASIC RAM while running; restores the active session after a key.
function lcc2_pc1350_lcd_pset_test(binary_path)
    assert(manager.machine.system.name == "pc1350", "PC-1350 required")
    assert(not lcc2_pc1350_lcd_pset_result
        or lcc2_pc1350_lcd_pset_result.status == "PASS"
        or lcc2_pc1350_lcd_pset_result.status == "FAIL", "Pixel test already active")
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces.program
    local ram = emu.item(cpu.items["0/m_ram"])
    local control = emu.item(cpu.items["0/m_c"])
    local file = assert(io.open(binary_path, "rb"))
    local binary = assert(file:read("*a"))
    file:close()
    local origin, trap, ready = 0x6030, 0x6800, 0x6810
    assert(origin + #binary < trap, "pixel fixture overlaps return trap")
    local memory, internal, registers = {}, {}, {}
    for i = 0x6000, 0x7eff do memory[i] = space:read_u8(i) end
    for i = 0, 95 do internal[i] = ram:read(i) end
    for _, name in ipairs({"PC", "DP", "P", "Q", "R", "Carry", "Zero"}) do
        registers[name] = cpu.state[name].value
    end
    local original_control = control:read(0)
    local function restore()
        for i = 0x6000, 0x7eff do space:write_u8(i, memory[i]) end
        for i = 0, 95 do ram:write(i, internal[i]) end
        control:write(0, original_control)
        for name, value in pairs(registers) do cpu.state[name].value = value end
        lcc2_pc1350_lcd_pset_subscription:unsubscribe()
    end
    for i = 1, #binary do space:write_u8(origin + i - 1, binary:byte(i)) end
    space:write_u8(trap, 0x79)
    space:write_u8(trap + 1, math.floor(trap / 256))
    space:write_u8(trap + 2, trap % 256)
    space:write_u8(ready, 0)
    local entry_r = registers.R
    ram:write(entry_r - 2, trap % 256)
    ram:write(entry_r - 1, math.floor(trap / 256))
    cpu.state.R.value = entry_r - 2
    cpu.state.PC.value = origin
    lcc2_pc1350_lcd_pset_result = {status = "running"}
    local result = lcc2_pc1350_lcd_pset_result
    local frames = 0
    lcc2_pc1350_lcd_pset_subscription = emu.add_machine_frame_notifier(function()
        local ok, message = pcall(function()
            frames = frames + 1
            assert(frames < 2000,
                string.format("pixel fixture timeout at %04X", cpu.state.PC.value))
            if result.status == "running" and space:read_u8(ready) == 42 then
                result.runtime_data = {}
                for name, address in pairs({
                    bank = 0x6597,
                    column = 0x6598,
                    page = 0x6599,
                    row = 0x659a,
                    mask_index = 0x659b,
                    mask = 0x659c,
                    address_low = 0x659d,
                    address_high = 0x659e
                }) do
                    result.runtime_data[name] = space:read_u8(address)
                end
                local expected = {
                    [0x705e] = 0x40,
                    [0x707b] = 0x01,
                    [0x721e] = 0x80,
                    [0x781d] = 0x01
                }
                result.lcd_memory = {}
                result.nonzero_lcd = {}
                for address = 0x7000, 0x787B do
                    local value = space:read_u8(address)
                    if value ~= 0 then
                        result.nonzero_lcd[#result.nonzero_lcd + 1] =
                            {address = address, value = value}
                    end
                end
                for address in pairs(expected) do
                    result.lcd_memory[address] = space:read_u8(address)
                end
                for address, value in pairs(expected) do
                    assert(space:read_u8(address) == value,
                        string.format("LCD byte %04X = %02X, expected %02X",
                            address, space:read_u8(address), value))
                end
                result.status = "waiting"
                print("PC1350 lcd_pset passed coordinate, bank, set/clear and bounds checks; press a key to restore the session.")
            end
            if cpu.state.PC.value == trap then
                assert(result.status == "waiting", "fixture returned before pixel checks")
                assert(cpu.state.R.value == entry_r, "stack imbalance")
                restore()
                result.status = "PASS"
                print("PASS: PC1350 lcd_pset test completed; previous session restored.")
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

-- Disposable session only: clears BASIC and overwrites the reserved ASM area.
function lcc2_program_test(binary_path)
    assert(manager.machine.system.name == "pc1403", "PC-1403 required")
    assert(not manager.machine.paused, "resume emulation before testing")
    assert(type(keyline) == "function", "load pc1403_key.lua first")
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces.program
    local ram = emu.item(cpu.items["0/m_ram"])
    local file = assert(io.open(binary_path, "rb"))
    local binary = file:read("*a")
    file:close()
    local origin = 0xe030
    assert(#binary > 0 and origin + #binary <= 0xe600, "binary overlaps outputs")
    -- The wrapper's final MVWD/RTN identifies the return trap without an ASM map.
    local return_offset = assert(binary:find(string.char(0, 11, 0x18, 0x37), 1, true),
        "register restore sequence not found")
    local return_address = origin + return_offset - 1 + 3
    local restore_address = return_address - 3
    local traps = {origin, restore_address, return_address}
    local originals = {}
    for _, address in ipairs(traps) do
        local offset = address - origin + 1
        originals[address] = {binary:byte(offset, offset + 2)}
    end
    local function install(address)
        space:write_u8(address, 0x79)
        space:write_u8(address + 1, math.floor(address / 256))
        space:write_u8(address + 2, address % 256)
    end
    local function restore(address)
        for i, byte in ipairs(originals[address]) do
            space:write_u8(address + i - 1, byte)
        end
    end
    local function dump(address, count)
        local bytes = {}
        for i = 0, count - 1 do
            bytes[#bytes + 1] = string.format("%02X", space:read_u8(address + i))
        end
        return table.concat(bytes, " ")
    end
    lcc2_program_result = {status = "running", calls = 0, returns = 0}
    local stage, entry_r, saved = "idle", nil, {}
    lcc2_program_subscription = emu.add_machine_frame_notifier(function()
        if lcc2_program_result.status ~= "running" then return end
        local ok, message = pcall(function()
            local pc = cpu.state.PC.value
            if stage == "entry" and pc == origin then
                entry_r = cpu.state.R.value
                for i = 0, 11 do saved[i] = ram:read(i) end
                saved[0] = 11 -- LII 11 precedes the legacy EXWD save.
                restore(origin)
                stage = "main"
            elseif stage == "main" and pc == restore_address then
                restore(restore_address)
                -- Functions have returned: temporarily covering their first bytes
                -- is now safe, provided the RTN trap is restored before the next RUN.
                install(return_address)
                stage = "return"
            elseif stage == "return" and pc == return_address then
                assert(cpu.state.R.value == entry_r, "stack imbalance at BASIC return")
                for i = 0, 11 do
                    assert(ram:read(i) == saved[i], "register restoration failed at " .. i)
                end
                assert(dump(0xe600, 7) == "37 12 23 00 FF FF FF",
                    "integrated outputs: " .. dump(0xe600, 7))
                restore(return_address)
                lcc2_program_result.returns = lcc2_program_result.returns + 1
                lcc2_program_result.stack_r = entry_r
                stage = "done"
            end
        end)
        if not ok then
            lcc2_program_result.status = "FAIL"
            lcc2_program_result.error = tostring(message)
            restore(origin)
            restore(restore_address)
            restore(return_address)
        end
    end)
    lcc2_program_coroutine = coroutine.create(function()
        local ok, message = pcall(function()
            keyline("_||")
            keyline("NEW")
            keyline("POKE&FF01,&30,&E8")
            keyline("NEW")
            keyline("10 CALL &E030")
            keyline("20 END")
            keyline("|")
            local basic_start = space:read_u8(0xff01) + 256 * space:read_u8(0xff02)
            assert(basic_start == 0xe830, "BASIC memory reservation failed")
            local basic_before = dump(basic_start, 64)
            for i = 1, #binary do space:write_u8(origin + i - 1, binary:byte(i)) end
            for call = 1, 10 do
                for address = 0xe600, 0xe607 do space:write_u8(address, 0x55) end
                ram:write(32, 0)
                ram:write(33, 0)
                install(origin)
                install(restore_address)
                stage = "entry"
                keyline("RUN")
                for frame = 1, 180 do
                    if stage == "done" or lcc2_program_result.status == "FAIL" then break end
                    emu.wait(1 / 60)
                end
                assert(lcc2_program_result.status ~= "FAIL", lcc2_program_result.error)
                assert(stage == "done", "BASIC CALL did not return")
                emu.wait(0.5)
                keyline("POKE&E607,42")
                assert(space:read_u8(0xe607) == 42, "BASIC caller is not responsive")
                assert(dump(basic_start, 64) == basic_before, "BASIC program changed")
                assert(space:read_u8(0xff01) + 256 * space:read_u8(0xff02) == basic_start,
                    "BASIC memory pointer changed")
                lcc2_program_result.calls = call
                lcc2_program_result.outputs = dump(0xe600, 8)
            end
        end)
        lcc2_program_result.status = ok and "PASS" or "FAIL"
        if not ok then lcc2_program_result.error = tostring(message) end
        print("lcc2 integrated program: " .. lcc2_program_result.status)
        emu.pause()
    end)
    local ok, message = coroutine.resume(lcc2_program_coroutine)
    assert(ok, message)
end

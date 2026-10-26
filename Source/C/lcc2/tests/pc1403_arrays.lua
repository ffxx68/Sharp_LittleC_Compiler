-- Disposable session only: clears BASIC and overwrites reserved test RAM.
function lcc2_array_test(binary_path, wide)
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
    local limit = wide and 0xf400 or 0xec00
    local output = wide and 0xf800 or 0xec00
    local basic = wide and 0xf830 or 0xf030
    local result_value = wide and 0xbeef or 0xd014
    assert(#binary > 0 and origin + #binary <= limit, "binary overlaps outputs")
    local return_offset = assert(binary:find(string.char(0, 11, 0x18, 0x37), 1, true),
        "register restore sequence not found")
    local return_address = origin + return_offset - 1 + 3
    local restore_address = return_address - 3
    local originals = {}
    for _, address in ipairs({origin, restore_address, return_address}) do
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
    local function word(address)
        return space:read_u8(address) + 256 * space:read_u8(address + 1)
    end
    local function dump(address, count)
        local bytes = {}
        for i = 0, count - 1 do
            bytes[#bytes + 1] = string.format("%02X", space:read_u8(address + i))
        end
        return table.concat(bytes, " ")
    end
    local expected = {
        11, 22, 3, 0x468a, 0xcf12, 0,
        18, 30, 3, 0xf257, 0xd014, 0, 256, 0xffff, 0, 0
    }
    if wide then expected = {255, 0xbeef, 255, 0} end
    lcc2_array_result = {status = "running", calls = 0, returns = 0}
    local stage, entry_r, saved = "idle", nil, {}
    lcc2_array_subscription = emu.add_machine_frame_notifier(function()
        if lcc2_array_result.status ~= "running" then return end
        local ok, message = pcall(function()
            local pc = cpu.state.PC.value
            if stage == "entry" and pc == origin then
                entry_r = cpu.state.R.value
                for i = 0, 11 do saved[i] = ram:read(i) end
                saved[0] = 11
                restore(origin)
                stage = "main"
            elseif stage == "main" and pc == restore_address then
                assert(cpu.state.R.value == entry_r, "stack imbalance after main")
                assert(ram:read(2) + 256 * ram:read(3) == result_value,
                    "incorrect indexed word return value")
                for i, value in ipairs(expected) do
                    assert(word(output + 2 * (i - 1)) == value,
                        string.format("result[%d]: got %04X, expected %04X",
                            i - 1, word(output + 2 * (i - 1)), value))
                end
                if wide then
                    assert(space:read_u8(0xf500) == 255, "byte index 256 was truncated")
                    assert(word(0xf700) == 0xbeef, "word offset 256 was truncated")
                else
                    assert(space:read_u8(0xec20) == 2, "index call evaluated more than once")
                    assert(dump(0xeefe, 3) == "12 1E 03", "XRAM byte/page boundary")
                    assert(dump(0xefff, 6) == "57 F2 14 D0 00 00", "XRAM word/page boundary")
                    for address = 0xecfc, 0xed03 do
                        assert(space:read_u8(address) == 0,
                            "XRAM array was not zero initialized")
                    end
                    for address = 22, 29 do
                        assert(ram:read(address) == 0,
                            "internal-RAM array was not zero initialized")
                    end
                end
                restore(restore_address)
                install(return_address)
                stage = "return"
            elseif stage == "return" and pc == return_address then
                assert(cpu.state.R.value == entry_r, "stack imbalance at BASIC return")
                for i = 0, 11 do
                    assert(ram:read(i) == saved[i], "register restoration failed at " .. i)
                end
                restore(return_address)
                lcc2_array_result.returns = lcc2_array_result.returns + 1
                lcc2_array_result.stack_r = entry_r
                stage = "done"
            end
        end)
        if not ok then
            lcc2_array_result.status = "FAIL"
            lcc2_array_result.error = tostring(message)
            restore(origin)
            restore(restore_address)
            restore(return_address)
        end
    end)
    lcc2_array_coroutine = coroutine.create(function()
        local ok, message = pcall(function()
            keyline("_||")
            keyline("NEW")
            keyline(wide and "POKE&FF01,&30,&F8" or "POKE&FF01,&30,&F0")
            keyline("NEW")
            keyline("10 CALL &E030")
            keyline("20 END")
            keyline("|")
            local basic_start = word(0xff01)
            assert(basic_start == basic, "BASIC memory reservation failed")
            local basic_before = dump(basic_start, 64)
            for i = 1, #binary do space:write_u8(origin + i - 1, binary:byte(i)) end
            for call = 1, 10 do
                for address = output, output + 33 do space:write_u8(address, 0x55) end
                if wide then
                    for address = 0xf400, 0xf501 do space:write_u8(address, 0x55) end
                    for address = 0xf600, 0xf703 do space:write_u8(address, 0x55) end
                else
                    for address = 0xeefe, 0xef00 do space:write_u8(address, 0x55) end
                    for address = 0xefff, 0xf004 do space:write_u8(address, 0x55) end
                    for address = 0xecfc, 0xed03 do space:write_u8(address, 0x55) end
                    for address = 12, 29 do ram:write(address, 0x55) end
                end
                install(origin)
                install(restore_address)
                stage = "entry"
                keyline("RUN")
                for frame = 1, 180 do
                    if stage == "done" or lcc2_array_result.status == "FAIL" then break end
                    emu.wait(1 / 60)
                end
                assert(lcc2_array_result.status ~= "FAIL", lcc2_array_result.error)
                assert(stage == "done", "BASIC CALL did not return")
                emu.wait(0.5)
                keyline(wide and "POKE&F821,42" or "POKE&EC21,42")
                assert(space:read_u8(output + 33) == 42, "BASIC caller is not responsive")
                assert(dump(basic_start, 64) == basic_before, "BASIC program changed")
                assert(word(0xff01) == basic_start, "BASIC memory pointer changed")
                lcc2_array_result.calls = call
                lcc2_array_result.outputs = dump(output, #expected * 2)
            end
        end)
        lcc2_array_result.status = ok and "PASS" or "FAIL"
        if not ok then lcc2_array_result.error = tostring(message) end
        print("lcc2 arrays: " .. lcc2_array_result.status)
        emu.pause()
    end)
    local ok, message = coroutine.resume(lcc2_array_coroutine)
    assert(ok, message)
end

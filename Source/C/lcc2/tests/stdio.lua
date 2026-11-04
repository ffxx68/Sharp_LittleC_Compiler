-- Disposable session only: clears BASIC and overwrites the reserved code area.
function lcc2_stdio_test(binary_path, append_test, snapshot_path)
    assert(manager.machine.system.name == "pc1403", "PC-1403 required")
    assert(type(keyline) == "function", "load pc1403_key.lua first")
    if lcc2_stdio_subscription ~= nil then
        lcc2_stdio_subscription:unsubscribe()
    end
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces.program
    local ram = emu.item(cpu.items["0/m_ram"])
    local function read_word(address)
        return space:read_u8(address) + 256 * space:read_u8(address + 1)
    end
    local file = assert(io.open(binary_path, "rb"))
    local binary = file:read("*a")
    file:close()
    local origin = 0xe030
    assert(#binary > 0 and origin + #binary <= 0xe600, "binary overlaps outputs")
    -- The target module ends with its length byte and 25-byte display buffer.
    local length_address = origin + #binary - 26
    local function output_text(count)
        local text = {}
        for i = 1, count do
            text[i] = string.char(space:read_u8(length_address + i))
        end
        return table.concat(text)
    end
    local offset = assert(binary:find(string.char(0, 11, 0x18, 0x37), 1, true))
    local restore_address = origin + offset - 1
    local return_address = restore_address + 3
    local function install(address)
        space:write_u8(address, 0x79)
        space:write_u8(address + 1, math.floor(address / 256))
        space:write_u8(address + 2, address % 256)
    end
    local function restore(address)
        for i = 0, 2 do
            space:write_u8(address + i, binary:byte(address - origin + i + 1))
        end
    end
    lcc2_stdio_result = {status = "running", returns = 0}
    local stage, entry_r, saved = "idle", nil, {}
    lcc2_stdio_subscription = emu.add_machine_frame_notifier(function()
        if lcc2_stdio_result.status ~= "running" then return end
        local pc = cpu.state.PC.value
        if stage == "entry" and pc == origin then
            entry_r = cpu.state.R.value
            for i = 0, 11 do saved[i] = ram:read(i) end
            saved[0] = 11
            restore(origin)
            stage = "input"
        elseif stage == "input" and pc == restore_address then
            lcc2_stdio_result.key_code = read_word(0xe600)
            lcc2_stdio_result.output_result = read_word(0xe602)
            lcc2_stdio_result.completed = space:read_u8(0xe604)
            lcc2_stdio_result.stack_balanced = cpu.state.R.value == entry_r
            lcc2_stdio_result.output_length = space:read_u8(length_address)
            lcc2_stdio_result.output_text =
                output_text(lcc2_stdio_result.output_length)
            if snapshot_path ~= nil then
                manager.machine.screens[":screen"]:snapshot(snapshot_path)
            end
            restore(restore_address)
            install(return_address)
            stage = "return"
        elseif stage == "return" and pc == return_address then
            lcc2_stdio_result.registers_restored = true
            for i = 0, 11 do
                if ram:read(i) ~= saved[i] then
                    lcc2_stdio_result.registers_restored = false
                end
            end
            restore(return_address)
            lcc2_stdio_result.returns = lcc2_stdio_result.returns + 1
            stage = "done"
        end
    end)
    lcc2_stdio_coroutine = coroutine.create(function()
        local ok, message = pcall(function()
            keyline("_||")
            keyline("NEW")
            keyline("POKE&FF01,&30,&E8")
            keyline("NEW")
            keyline("10 CALL &E030")
            keyline("20 END")
            keyline("|")
            assert(read_word(0xff01) == 0xe830, "BASIC reservation failed")
            for i = 1, #binary do space:write_u8(origin + i - 1, binary:byte(i)) end
            install(origin)
            install(restore_address)
            stage = "entry"
            keyline("RUN")
            emu.wait(0.5)
            key("1")
            if append_test then
                for frame = 1, 180 do
                    if space:read_u8(length_address) == 3 then break end
                    emu.wait(1 / 60)
                end
                assert(space:read_u8(length_address) == 3, "three appends not reached")
                assert(output_text(3) == "ABC", "short appended line is incorrect")
                lcc2_stdio_result.short_output = output_text(3)
                emu.wait(1)
                key("2")
            end
            for frame = 1, 180 do
                if stage == "done" then break end
                emu.wait(1 / 60)
            end
            assert(stage == "done", "BASIC CALL did not return; stage=" .. stage)
            assert(lcc2_stdio_result.key_code == 51, "wrong raw key code")
            assert(lcc2_stdio_result.output_result == (append_test and 88 or 65),
                "wrong putchar return")
            local expected = append_test and "ABCDEFGHIJKLMNOPQRSTUVWX" or "A"
            assert(lcc2_stdio_result.output_text == expected, "wrong appended output")
            assert(lcc2_stdio_result.output_length == #expected, "wrong output length")
            assert(space:read_u8(length_address + #expected + 1) == 13,
                "output terminator was overwritten")
            if append_test then
                assert(read_word(0xe606) == 50, "wrong second raw key code")
                assert(read_word(0xe608) == 65535, "full line must return -1")
                assert(read_word(0xe60a) == 65535, "control character must return -1")
                for i = 0, 23 do
                    assert(space:read_u8(0xe610 + i) == 65 + i,
                        "wrong putchar return at column " .. i)
                end
                lcc2_stdio_result.full_result = read_word(0xe608)
                lcc2_stdio_result.control_result = read_word(0xe60a)
            end
            assert(lcc2_stdio_result.completed == 42, "program did not complete")
            assert(lcc2_stdio_result.stack_balanced, "stack imbalance")
            assert(lcc2_stdio_result.registers_restored, "register restoration failed")
            emu.wait(0.5)
            keyline("POKE&E605,42")
            assert(space:read_u8(0xe605) == 42, "BASIC is not responsive")
        end)
        lcc2_stdio_result.status = ok and "PASS" or "FAIL"
        if not ok then lcc2_stdio_result.error = tostring(message) end
        print("lcc2 stdio: " .. lcc2_stdio_result.status)
    end)
    local ok, message = coroutine.resume(lcc2_stdio_coroutine)
    assert(ok, message)
end

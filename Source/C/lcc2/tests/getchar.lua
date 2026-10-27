-- Disposable session only: clears BASIC and overwrites the reserved code area.
function lcc2_getchar_test(binary_path)
    assert(manager.machine.system.name == "pc1403", "PC-1403 required")
    assert(type(keyline) == "function", "load pc1403_key.lua first")
    if lcc2_getchar_subscription ~= nil then
        lcc2_getchar_subscription:unsubscribe()
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
    lcc2_getchar_result = {status = "running", returns = 0}
    local stage, entry_r, saved = "idle", nil, {}
    lcc2_getchar_subscription = emu.add_machine_frame_notifier(function()
        if lcc2_getchar_result.status ~= "running" then return end
        local pc = cpu.state.PC.value
        if stage == "entry" and pc == origin then
            entry_r = cpu.state.R.value
            for i = 0, 11 do saved[i] = ram:read(i) end
            saved[0] = 11
            restore(origin)
            stage = "input"
        elseif stage == "input" and pc == restore_address then
            lcc2_getchar_result.key_codes = {
                read_word(0xe600), read_word(0xe602), read_word(0xe604)
            }
            lcc2_getchar_result.completed = space:read_u8(0xe606)
            lcc2_getchar_result.stack_balanced = cpu.state.R.value == entry_r
            restore(restore_address)
            install(return_address)
            stage = "return"
        elseif stage == "return" and pc == return_address then
            lcc2_getchar_result.registers_restored = true
            for i = 0, 11 do
                if ram:read(i) ~= saved[i] then
                    lcc2_getchar_result.registers_restored = false
                end
            end
            restore(return_address)
            lcc2_getchar_result.returns = lcc2_getchar_result.returns + 1
            stage = "done"
        end
    end)
    lcc2_getchar_coroutine = coroutine.create(function()
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
            for address = 0xe600, 0xe607 do space:write_u8(address, 0) end
            install(origin)
            install(restore_address)
            stage = "entry"
            keyline("RUN")
            emu.wait(0.5)
            assert(stage == "input", "getchar did not wait for input")
            assert(space:read_u8(0xe606) == 0, "program completed before input")
            lcc2_getchar_result.blocking = true
            local keys, expected = {"1", "2", "1"}, {51, 50, 51}
            for i, character in ipairs(keys) do
                key(character)
                for frame = 1, 180 do
                    if read_word(0xe600 + 2 * (i - 1)) == expected[i] then break end
                    emu.wait(1 / 60)
                end
                assert(read_word(0xe600 + 2 * (i - 1)) == expected[i],
                    "wrong raw key code for " .. character)
                emu.wait(0.2)
            end
            for frame = 1, 180 do
                if stage == "done" then break end
                emu.wait(1 / 60)
            end
            assert(stage == "done", "BASIC CALL did not return; stage=" .. stage)
            assert(lcc2_getchar_result.completed == 42, "program did not complete")
            assert(lcc2_getchar_result.stack_balanced, "stack imbalance")
            assert(lcc2_getchar_result.registers_restored, "register restoration failed")
            keyline("POKE&E607,42")
            assert(space:read_u8(0xe607) == 42, "BASIC is not responsive")
            lcc2_getchar_result.basic_responsive = true
        end)
        lcc2_getchar_result.status = ok and "PASS" or "FAIL"
        if not ok then
            lcc2_getchar_result.error = tostring(message)
            restore(origin)
            restore(restore_address)
            restore(return_address)
        end
        lcc2_getchar_subscription:unsubscribe()
        print("lcc2 getchar: " .. lcc2_getchar_result.status)
    end)
    local ok, message = coroutine.resume(lcc2_getchar_coroutine)
    assert(ok, message)
end

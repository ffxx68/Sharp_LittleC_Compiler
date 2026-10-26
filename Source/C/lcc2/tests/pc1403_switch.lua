-- Disposable session only: clears BASIC and overwrites reserved test RAM.
local EXPECTED = "0B 00 16 00 01 00 21 00 2C 00 37 00 04 00 47 00"

function lcc2_switch_test(binary_path)
    assert(manager.machine.system.name == "pc1403", "PC-1403 required")
    assert(type(keyline) == "function", "load pc1403_key.lua first")
    if lcc2_switch_subscription then
        lcc2_switch_subscription:unsubscribe()
    end
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces.program
    local ram = emu.item(cpu.items["0/m_ram"])
    local file = assert(io.open(binary_path, "rb"))
    local binary = file:read("*a")
    file:close()

    local origin = 0xe030
    assert(#binary > 0 and origin + #binary < 0xe720,
        "binary overlaps switch results")
    local return_offset = assert(binary:find(string.char(0, 11, 0x18, 0x37), 1, true),
        "register restore sequence not found")
    local return_address = origin + return_offset - 1 + 3
    local restore_address = return_address - 3
    local originals = {}
    for _, address in ipairs({origin, restore_address, return_address}) do
        local offset = address - origin + 1
        originals[address] = {binary:byte(offset, offset + 2)}
    end

    local function install_loop(address)
        space:write_u8(address, 0x79)
        space:write_u8(address + 1, math.floor(address / 256))
        space:write_u8(address + 2, address % 256)
    end
    local function restore(address)
        for i, byte in ipairs(originals[address]) do
            space:write_u8(address + i - 1, byte)
        end
    end
    local function read_word(address)
        return space:read_u8(address) + 256 * space:read_u8(address + 1)
    end
    local function dump(address, count)
        local bytes = {}
        for index = 0, count - 1 do
            bytes[#bytes + 1] = string.format("%02X", space:read_u8(address + index))
        end
        return table.concat(bytes, " ")
    end

    lcc2_switch_result = {
        status = "running", calls = 0, returns = 0, basic_checks = 0
    }
    local stage, entry_r, expected_registers = "idle", nil, {}
    lcc2_switch_subscription = emu.add_machine_frame_notifier(function()
        if lcc2_switch_result.status ~= "running" then return end
        local ok, message = pcall(function()
            local pc = cpu.state.PC.value
            if stage == "entry" and pc == origin then
                entry_r = cpu.state.R.value
                for index = 0, 11 do
                    expected_registers[index + 1] = ram:read(index)
                end
                expected_registers[1] = 11
                restore(origin)
                stage = "main"
            elseif stage == "main" and pc == restore_address then
                assert(cpu.state.R.value == entry_r, "switch stack imbalance")
                assert(cpu.state.BA.value == 1, "incorrect switch return value")
                local actual = dump(0xe720, 16)
                assert(actual == EXPECTED, "switch results mismatch: " .. actual)
                assert(read_word(0xe740) == 1, "switch expression was not evaluated once")
                restore(restore_address)
                install_loop(return_address)
                stage = "return"
            elseif stage == "return" and pc == return_address then
                assert(cpu.state.R.value == entry_r, "stack imbalance at BASIC return")
                for index, expected in ipairs(expected_registers) do
                    assert(ram:read(index - 1) == expected,
                        "register restoration failed at " .. (index - 1))
                end
                restore(return_address)
                lcc2_switch_result.returns = lcc2_switch_result.returns + 1
                stage = "done"
            end
        end)
        if not ok then
            lcc2_switch_result.status = "FAIL"
            lcc2_switch_result.error = tostring(message)
            restore(origin)
            restore(restore_address)
            restore(return_address)
        end
    end)

    lcc2_switch_coroutine = coroutine.create(function()
        local ok, message = pcall(function()
            keyline("_||")
            keyline("NEW")
            keyline("POKE&FF01,&30,&E8")
            keyline("NEW")
            keyline("10 CALL &E030")
            keyline("20 END")
            keyline("|")
            local basic_start = read_word(0xff01)
            assert(basic_start == 0xe830, "BASIC memory reservation failed")
            local basic_before = dump(basic_start, 64)
            for index = 1, #binary do
                space:write_u8(origin + index - 1, binary:byte(index))
            end
            for call = 1, 10 do
                for address = 0xe720, 0xe74f do
                    space:write_u8(address, 0x55)
                end
                install_loop(origin)
                install_loop(restore_address)
                stage = "entry"
                keyline("RUN")
                for frame = 1, 120 do
                    if stage == "done" or
                       lcc2_switch_result.status == "FAIL" then break end
                    emu.wait(1 / 60)
                end
                assert(lcc2_switch_result.status ~= "FAIL",
                    lcc2_switch_result.error)
                assert(stage == "done", string.format(
                    "BASIC CALL did not return (PC=%04X BA=%04X R=%04X results=%s)",
                    cpu.state.PC.value, cpu.state.BA.value, cpu.state.R.value,
                    dump(0xe720, 16)))
                emu.wait(0.5)
                space:write_u8(0xe710, 0x55)
                keyline("POKE&E710,6*7")
                assert(space:read_u8(0xe710) == 42,
                    "BASIC caller is not responsive")
                assert(dump(basic_start, 64) == basic_before,
                    "BASIC program changed")
                assert(read_word(0xff01) == basic_start,
                    "BASIC memory pointer changed")
                lcc2_switch_result.calls = call
                lcc2_switch_result.basic_checks = call
                lcc2_switch_result.outputs = dump(0xe720, 16)
            end
        end)
        lcc2_switch_result.status = ok and "PASS" or "FAIL"
        if not ok then lcc2_switch_result.error = tostring(message) end
        print("lcc2 standard switch with BASIC: " .. lcc2_switch_result.status)
    end)
    emu.unpause()
    local ok, message = coroutine.resume(lcc2_switch_coroutine)
    assert(ok, message)
end

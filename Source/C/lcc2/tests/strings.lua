-- Disposable session only: clears BASIC and overwrites code, test RAM and IRAM.
function lcc2_string_test(binary_path)
    assert(manager.machine.system.name == "pc1403", "PC-1403 required")
    assert(type(keyline) == "function", "load pc1403_key.lua first")
    if lcc2_string_subscription ~= nil then
        lcc2_string_subscription:unsubscribe()
    end
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces.program
    local ram = emu.item(cpu.items["0/m_ram"])
    local file = assert(io.open(binary_path, "rb"))
    local binary = file:read("*a")
    file:close()
    local origin = 0xe030
    assert(#binary > 0 and origin + #binary <= 0xe500, "binary overlaps test RAM")
    local offset = assert(binary:find(string.char(0, 11, 0x18, 0x37), 1, true),
        "register restore sequence not found")
    local restore_address = origin + offset - 1
    local return_address = restore_address + 3
    local call_address = restore_address - 7
    assert(binary:byte(call_address - origin + 1) == 0x78,
        "startup CALL main not found")
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
    local function read_byte(address)
        if address < 256 then return ram:read(address) end
        return space:read_u8(address)
    end
    local function write_byte(address, value)
        if address < 256 then ram:write(address, value)
        else space:write_u8(address, value) end
    end
    local function write_word(address, value)
        write_byte(address, value % 256)
        write_byte(address + 1, math.floor(value / 256))
    end
    local function read_word(address)
        return read_byte(address) + 256 * read_byte(address + 1)
    end
    local function write_string(address, text)
        for i = 1, #text do write_byte(address + i - 1, text:byte(i)) end
        write_byte(address + #text, 0)
    end
    local function check_string(address, text, label)
        for i = 1, #text do
            assert(read_byte(address + i - 1) == text:byte(i),
                label .. ": incorrect byte at offset " .. (i - 1))
        end
        assert(read_byte(address + #text) == 0, label .. ": missing NUL")
    end
    local function basic_dump(address)
        local bytes = {}
        for i = 0, 63 do bytes[i + 1] = string.char(read_byte(address + i)) end
        return table.concat(bytes)
    end
    local vectors = {
        {name = "copy xram", source = "ABC", initial = "old"},
        {name = "copy empty", source = "", initial = "old"},
        {name = "copy long", source = string.rep("abcdefghij", 26), initial = ""},
        {name = "copy high bytes", source = string.char(128, 255, 90), initial = ""},
        {name = "copy page crossing", source = "ABC", initial = "",
            src = 0xe5ff, dest = 0xe7ff},
        {name = "copy iram to xram", source = "ABC", initial = "", src = 32},
        {name = "copy xram to iram", source = "ABC", initial = "", dest = 48},
        {name = "copy iram to iram", source = "ABC", initial = "", src = 32, dest = 48},
        {name = "append xram", source = "CD", initial = "AB", append = true},
        {name = "append empty source", source = "", initial = "AB", append = true},
        {name = "append empty destination", source = "AB", initial = "", append = true},
        {name = "append long", source = "0123456789",
            initial = string.rep("abcdefghij", 25), append = true},
        {name = "append high bytes", source = string.char(128, 255),
            initial = "A", append = true},
        {name = "append page crossing", source = "CD", initial = "AB",
            src = 0xe5ff, dest = 0xe7ff, append = true},
        {name = "append iram to xram", source = "CD", initial = "AB",
            src = 32, append = true},
        {name = "append xram to iram", source = "CD", initial = "AB",
            dest = 48, append = true},
        {name = "append iram to iram", source = "CD", initial = "AB",
            src = 32, dest = 48, append = true}
    }
    lcc2_string_result = {status = "running", vectors = 0, returns = 0}
    local stage, entry_r, vector = "idle", nil, nil
    local saved = {}
    local function restore_traps()
        restore(origin)
        restore(call_address)
        restore(restore_address)
        restore(return_address)
    end
    lcc2_string_subscription = emu.add_machine_frame_notifier(function()
        if lcc2_string_result.status ~= "running" then return end
        local ok, message = pcall(function()
            local pc = cpu.state.PC.value
            if stage == "entry" and pc == origin then
                entry_r = cpu.state.R.value
                for i = 0, 11 do saved[i] = ram:read(i) end
                saved[0] = 11
                restore(origin)
                stage = "initialize"
            elseif stage == "initialize" and pc == call_address then
                -- BASIC uses IRAM and startup zeroes globals; inject after both.
                local src, dest = vector.src or 0xe500, vector.dest or 0xe700
                local expected_length = (vector.append and #vector.initial or 0) +
                                        #vector.source
                local extent = math.max(#vector.initial, expected_length)
                for i = -1, extent + 1 do write_byte(dest + i, 0xa5) end
                write_string(dest, vector.initial)
                write_string(src, vector.source)
                write_word(0xe610, src)
                write_word(0xe612, dest)
                write_byte(0xe616, vector.append and 1 or 0)
                restore(call_address)
                stage = "main"
            elseif stage == "main" and pc == restore_address then
                assert(cpu.state.R.value == entry_r, "stack imbalance")
                local dest, src = vector.dest or 0xe700, vector.src or 0xe500
                local expected = (vector.append and vector.initial or "") .. vector.source
                assert(read_word(0xe614) == dest, "incorrect destination pointer return")
                assert(read_byte(0xe617) == 42, "program did not complete")
                assert(read_byte(0xe618) == (expected:byte(1) or 0),
                    "dereference of returned pointer failed")
                check_string(dest, expected, "destination")
                check_string(src, vector.source, "source changed")
                assert(read_byte(dest - 1) == 0xa5, "destination underwrite")
                local extent = math.max(#vector.initial, #expected)
                assert(read_byte(dest + extent + 1) == 0xa5, "destination overwrite")
                for i = #expected + 1, extent do
                    assert(read_byte(dest + i) == (vector.initial:byte(i + 1) or 0),
                        "bytes after copied NUL changed")
                end
                restore(restore_address)
                install(return_address)
                stage = "return"
            elseif stage == "return" and pc == return_address then
                assert(cpu.state.R.value == entry_r, "stack imbalance at return")
                for i = 0, 11 do
                    assert(ram:read(i) == saved[i], "register restoration failed at " .. i)
                end
                restore(return_address)
                lcc2_string_result.returns = lcc2_string_result.returns + 1
                stage = "done"
            end
        end)
        if not ok then
            lcc2_string_result.status = "FAIL"
            lcc2_string_result.error = vector.name .. ": " .. tostring(message)
            restore_traps()
        end
    end)
    lcc2_string_coroutine = coroutine.create(function()
        local ok, message = pcall(function()
            keyline("_||")
            keyline("NEW")
            keyline("POKE&FF01,&30,&E8")
            keyline("NEW")
            keyline("10 CALL &E030")
            keyline("20 END")
            keyline("|")
            assert(read_word(0xff01) == 0xe830, "BASIC reservation failed")
            local basic_before = basic_dump(0xe830)
            for i = 1, #binary do space:write_u8(origin + i - 1, binary:byte(i)) end
            for index, current in ipairs(vectors) do
                vector = current
                lcc2_string_result.current = current.name
                install(origin)
                install(call_address)
                install(restore_address)
                stage = "entry"
                keyline("RUN")
                for frame = 1, 180 do
                    if stage == "done" or lcc2_string_result.status == "FAIL" then break end
                    emu.wait(1 / 60)
                end
                assert(lcc2_string_result.status ~= "FAIL", lcc2_string_result.error)
                assert(stage == "done", current.name .. ": BASIC CALL did not return")
                emu.wait(0.2)
                assert(basic_dump(0xe830) == basic_before, "BASIC program changed")
                assert(read_word(0xff01) == 0xe830, "BASIC pointer changed")
                lcc2_string_result.vectors = index
            end
            keyline("POKE&E619,6*7")
            assert(read_byte(0xe619) == 42, "BASIC is not responsive")
            lcc2_string_result.basic_responsive = true
            lcc2_string_result.stack_balanced = true
            lcc2_string_result.registers_restored = true
        end)
        if not ok then
            lcc2_string_result.error = tostring(message)
            restore_traps()
        end
        lcc2_string_result.status = ok and "PASS" or "FAIL"
        lcc2_string_subscription:unsubscribe()
        print("lcc2 strings: " .. lcc2_string_result.status)
    end)
    local ok, message = coroutine.resume(lcc2_string_coroutine)
    assert(ok, message)
end

-- Disposable session only: clears BASIC and overwrites reserved code/test RAM.
function lcc2_library_test(binary_path, snapshot_path)
    assert(manager.machine.system.name == "pc1403", "PC-1403 required")
    assert(type(keyline) == "function", "load pc1403_key.lua first")
    if lcc2_library_subscription ~= nil then
        lcc2_library_subscription:unsubscribe()
    end
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces.program
    local ram = emu.item(cpu.items["0/m_ram"])
    local file = assert(io.open(binary_path, "rb"))
    local binary = file:read("*a")
    file:close()
    local origin = 0xe030
    assert(#binary > 0 and origin + #binary <= 0xe500, "binary overlaps test RAM")
    local offset = assert(binary:find(string.char(0, 11, 0x18, 0x37), 1, true))
    local restore_address = origin + offset - 1
    local return_address = restore_address + 3
    local call_address = restore_address - 7
    assert(binary:byte(call_address - origin + 1) == 0x78, "startup CALL not found")
    local length_address = origin + #binary - 26
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
    local function restore_traps()
        restore(origin)
        restore(call_address)
        restore(restore_address)
        restore(return_address)
    end
    local function read_byte(address)
        if address < 256 then return ram:read(address) end
        return space:read_u8(address)
    end
    local function write_byte(address, value)
        if address < 256 then ram:write(address, value)
        else space:write_u8(address, value) end
    end
    local function read_word(address)
        return read_byte(address) + 256 * read_byte(address + 1)
    end
    local function write_word(address, value)
        write_byte(address, value % 256)
        write_byte(address + 1, math.floor(value / 256))
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
    local function dump(address, count)
        local bytes = {}
        for i = 0, count - 1 do bytes[i + 1] = string.char(read_byte(address + i)) end
        return table.concat(bytes)
    end
    local vectors = {}
    local numbers = {0, 1, 8, 9, 10, 11, 98, 99, 100, 101, 254, 255, 256,
        998, 999, 1000, 1001, 9998, 9999, 10000, 10001, 32767, 32768, 65534, 65535}
    for _, number in ipairs(numbers) do
        vectors[#vectors + 1] = {name = "convert " .. number, number = number, operation = 0}
    end
    for _, dest in ipairs({8, 32, 64, 0xe7ff}) do
        for _, number in ipairs({0, 65535}) do
            vectors[#vectors + 1] = {name = "convert at " .. dest .. ": " .. number,
                number = number, dest = dest, operation = 0}
        end
    end
    local output_vectors = {
        {name = "empty puts", text = "", initial = "", expected = "", result = 0},
        {name = "append puts", text = "ABC", initial = "", expected = "ABC", result = 0},
        {name = "shared putchar", operation = 2, number = 68, expected = "ABCD", result = 68},
        {name = "persistent puts", text = "EF", expected = "ABCDEF", result = 0},
        {name = "fill exact", text = "ABCDEFGHIJKLMNOPQRSTUVWX", initial = "",
            expected = "ABCDEFGHIJKLMNOPQRSTUVWX", result = 0},
        {name = "empty at full", text = "",
            expected = "ABCDEFGHIJKLMNOPQRSTUVWX", result = 0},
        {name = "full puts error", text = "Y",
            expected = "ABCDEFGHIJKLMNOPQRSTUVWX", result = 65535},
        {name = "partial prefix at full", text = "XYZ",
            initial = "ABCDEFGHIJKLMNOPQRSTUVW",
            expected = "ABCDEFGHIJKLMNOPQRSTUVWX", result = 65535},
        {name = "control prefix", text = "AB\nCD", initial = "",
            expected = "AB", result = 65535},
        {name = "newline rejected", text = "\n", initial = "",
            expected = "", result = 65535},
        {name = "high byte rejected", text = string.char(128), initial = "",
            expected = "", result = 65535},
        {name = "iram source", text = "RAM", src = 64, initial = "",
            expected = "RAM", result = 0},
        {name = "source page crossing", text = "PAGE", src = 0xe7ff, initial = "",
            expected = "PAGE", result = 0},
        {name = "long puts prefix", text = string.rep("A", 260), initial = "",
            expected = string.rep("A", 24), result = 65535},
        {name = "legacy conversion display migration", operation = 3, number = 65535,
            initial = "", expected = "65535", result = 0}
    }
    for _, vector in ipairs(output_vectors) do
        vector.operation = vector.operation or 1
        vectors[#vectors + 1] = vector
    end
    lcc2_library_result = {status = "running", vectors = 0, returns = 0,
        conversions = 0, output_checks = 0, basic_runs = 0}
    local stage, entry_r, vector = "idle", nil, nil
    local vector_index, batch_end = 0, 0
    local batch_size = math.ceil(#vectors / 3)
    local saved = {}
    local function start_vector()
        vector_index = vector_index + 1
        vector = assert(vectors[vector_index], "missing library test vector")
        lcc2_library_result.current = vector.name
        install(origin)
        install(call_address)
        install(restore_address)
        stage = "entry"
    end
    lcc2_library_subscription = emu.add_machine_frame_notifier(function()
        if lcc2_library_result.status ~= "running" then return end
        local ok, message = pcall(function()
            local pc = cpu.state.PC.value
            if stage == "entry" and pc == origin then
                entry_r = cpu.state.R.value
                for i = 0, 11 do saved[i] = ram:read(i) end
                saved[0] = 11
                restore(origin)
                stage = "initialize"
            elseif stage == "initialize" and pc == call_address then
                local dest, src = vector.dest or 0xe700, vector.src or 0xe500
                write_word(0xe600, vector.number or 0)
                write_word(0xe602, dest)
                write_word(0xe604, src)
                write_byte(0xe606, vector.operation)
                if vector.operation == 0 or vector.operation == 3 then
                    for i = 0, 5 do write_byte(dest + i, 0xa5) end
                    if dest > 8 then write_byte(dest - 1, 0xa5) end
                    write_byte(dest + 6, 0xa5)
                end
                if vector.text ~= nil then write_string(src, vector.text) end
                if vector.initial ~= nil then
                    write_byte(length_address, #vector.initial)
                    for i = 1, 25 do write_byte(length_address + i, 32) end
                    for i = 1, #vector.initial do
                        write_byte(length_address + i, vector.initial:byte(i))
                    end
                    write_byte(length_address + #vector.initial + 1, 13)
                end
                restore(call_address)
                stage = "main"
            elseif stage == "main" and pc == restore_address then
                assert(cpu.state.R.value == entry_r, "stack imbalance")
                assert(read_byte(0xe60a) == 42, "program did not complete")
                assert(read_byte(0xe60b) == 0x34, "uint8_t truncation failed")
                assert(read_word(0xe60c) == 0xabcd, "word/uint16_t storage failed")
                if vector.operation == 0 or vector.operation == 3 then
                    local dest, expected = vector.dest or 0xe700, tostring(vector.number)
                    check_string(dest, expected, "decimal conversion")
                    if dest > 8 then assert(read_byte(dest - 1) == 0xa5, "underwrite") end
                    for i = #expected + 1, 6 do
                        assert(read_byte(dest + i) == 0xa5, "conversion overwrite")
                    end
                end
                if vector.operation ~= 0 then
                    assert(read_word(0xe608) == vector.result,
                        string.format("wrong output return: %04X; entry R=%02X; source=%q",
                            read_word(0xe608), entry_r,
                            dump(vector.src or 0xe500, 4)))
                    assert(read_byte(length_address) == #vector.expected, "wrong LCD length")
                    assert(dump(length_address + 1, #vector.expected) == vector.expected,
                        "wrong appended LCD text")
                    assert(read_byte(length_address + #vector.expected + 1) == 13,
                        "LCD terminator changed")
                    if vector.text ~= nil then
                        check_string(vector.src or 0xe500, vector.text, "source changed")
                    end
                    if snapshot_path ~= nil and vector.operation == 3 then
                        manager.machine.screens[":screen"]:snapshot(snapshot_path)
                    end
                end
                restore(restore_address)
                install(return_address)
                stage = "return"
            elseif stage == "return" and pc == return_address then
                for i = 0, 11 do
                    assert(ram:read(i) == saved[i], "register restoration failed at " .. i)
                end
                restore(return_address)
                lcc2_library_result.returns = lcc2_library_result.returns + 1
                lcc2_library_result.vectors = vector_index
                if vector.operation == 0 then
                    lcc2_library_result.conversions =
                        lcc2_library_result.conversions + 1
                else
                    lcc2_library_result.output_checks =
                        lcc2_library_result.output_checks + 1
                end
                if vector_index < batch_end then
                    start_vector()
                else
                    stage = "done"
                end
            end
        end)
        if not ok then
            lcc2_library_result.status = "FAIL"
            lcc2_library_result.error = vector.name .. ": " .. tostring(message)
            restore_traps()
        end
    end)
    lcc2_library_coroutine = coroutine.create(function()
        local ok, message = pcall(function()
            keyline("_||")
            keyline("NEW")
            keyline("POKE&FF01,&30,&E8")
            keyline("NEW")
            keyline("10 FOR I=1 TO " .. batch_size)
            keyline("20 CALL &E030")
            keyline("30 NEXT I")
            keyline("40 END")
            keyline("|")
            assert(read_word(0xff01) == 0xe830, "BASIC reservation failed")
            local basic_before = dump(0xe830, 64)
            for i = 1, #binary do space:write_u8(origin + i - 1, binary:byte(i)) end
            assert(#vectors % batch_size == 0, "uneven library test batches")
            for batch = 1, #vectors / batch_size do
                batch_end = batch * batch_size
                start_vector()
                lcc2_library_result.basic_runs = batch
                keyline("RUN")
                for frame = 1, 180 * batch_size do
                    if stage == "done" or lcc2_library_result.status == "FAIL" then break end
                    emu.wait(1 / 60)
                end
                assert(lcc2_library_result.status ~= "FAIL", lcc2_library_result.error)
                assert(stage == "done", vector.name .. ": BASIC CALL did not return")
                emu.wait(0.2)
                assert(dump(0xe830, 64) == basic_before, "BASIC program changed")
                assert(read_word(0xff01) == 0xe830, "BASIC pointer changed")
            end
            keyline("POKE&E60E,6*7")
            assert(read_byte(0xe60e) == 42, "BASIC is not responsive")
            lcc2_library_result.basic_responsive = true
            lcc2_library_result.stack_balanced = true
            lcc2_library_result.registers_restored = true
        end)
        if not ok then
            lcc2_library_result.error = tostring(message)
            restore_traps()
        end
        lcc2_library_result.status = ok and "PASS" or "FAIL"
        lcc2_library_subscription:unsubscribe()
        print("lcc2 libraries: " .. lcc2_library_result.status)
    end)
    local ok, message = coroutine.resume(lcc2_library_coroutine)
    assert(ok, message)
end

function lcc2_device_test(binary_path, snapshot_path)
    assert(manager.machine.system.name == "pc1403", "PC-1403 required")
    assert(type(keyline) == "function", "load pc1403_key.lua first")
    if lcc2_device_subscription ~= nil then
        lcc2_device_subscription:unsubscribe()
    end
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces.program
    local ram = emu.item(cpu.items["0/m_ram"])
    local file = assert(io.open(binary_path, "rb"))
    local binary = file:read("*a")
    file:close()
    local origin = 0xe030
    assert(#binary > 0 and origin + #binary <= 0xe800,
        "binary overlaps the device test results")
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
            space:write_u8(address + i,
                binary:byte(address - origin + i + 1))
        end
    end
    local function read_byte(address)
        if address < 256 then return ram:read(address) end
        return space:read_u8(address)
    end
    local function find_break_field()
        for _, port in pairs(manager.machine.ioport.ports) do
            for _, field in pairs(port.fields) do
                if string.find(string.upper(field.name or ""), "BRK", 1, true) then
                    return field
                end
            end
        end
        return nil
    end
    lcc2_device_result = {status = "running", returns = 0}
    local stage, entry_r, saved = "idle", nil, {}
    lcc2_device_subscription = emu.add_machine_frame_notifier(function()
        if lcc2_device_result.status ~= "running" then return end
        local pc = cpu.state.PC.value
        if stage == "entry" and pc == origin then
            entry_r = cpu.state.R.value
            for i = 0, 11 do saved[i] = ram:read(i) end
            saved[0] = 11
            restore(origin)
            stage = "main"
        elseif stage == "main" and pc == restore_address then
            lcc2_device_result.stack_balanced = cpu.state.R.value == entry_r
            lcc2_device_result.scan_empty = read_byte(0xe800)
            lcc2_device_result.break_empty = read_byte(0xe801)
            lcc2_device_result.scan_pressed = read_byte(0xe802)
            lcc2_device_result.waited_key = read_byte(0xe803)
            lcc2_device_result.break_pressed = read_byte(0xe804)
            lcc2_device_result.break_released = read_byte(0xe805)
            lcc2_device_result.completed = read_byte(0xe806)
            lcc2_device_result.pixels = {
                space:read_u8(0x3000), space:read_u8(0x3001),
                space:read_u8(0x3040), space:read_u8(0x3041),
                space:read_u8(0x3042)
            }
            if snapshot_path ~= nil then
                manager.machine.screens[":screen"]:snapshot(snapshot_path)
            end
            restore(restore_address)
            install(return_address)
            stage = "return"
        elseif stage == "return" and pc == return_address then
            lcc2_device_result.registers_restored = true
            for i = 0, 11 do
                if ram:read(i) ~= saved[i] then
                    lcc2_device_result.registers_restored = false
                end
            end
            restore(return_address)
            lcc2_device_result.returns = 1
            stage = "done"
        end
    end)
    lcc2_device_coroutine = coroutine.create(function()
        local ok, message = pcall(function()
            keyline("_||")
            keyline("NEW")
            keyline("POKE&FF01,&30,&E8")
            keyline("NEW")
            keyline("10 CALL &E030")
            keyline("20 END")
            keyline("|")
            assert(read_byte(0xff01) == 0x30 and read_byte(0xff02) == 0xe8,
                "BASIC reservation failed")
            for i = 1, #binary do
                space:write_u8(origin + i - 1, binary:byte(i))
            end
            install(origin)
            install(restore_address)
            stage = "entry"
            keyline("RUN")
            emu.wait(0.5)
            assert(read_byte(0xe800) == 255, "key_scan no-key sentinel is not 0xFF")
            assert(read_byte(0xe801) == 0, "keybrk should initially be clear")
            key("1")
            for frame = 1, 180 do
                if read_byte(0xe802) ~= 255 then break end
                emu.wait(1 / 60)
            end
            assert(read_byte(0xe802) == 51, "key_scan did not return raw key code 51")
            key("2")
            for frame = 1, 180 do
                if read_byte(0xe803) ~= 0 then break end
                emu.wait(1 / 60)
            end
            assert(read_byte(0xe803) == 50, "key_wait did not return raw key code 50")
            local break_field = assert(find_break_field(), "MAME has no BRK input field")
            break_field:set_value(1)
            for frame = 1, 180 do
                if read_byte(0xe804) == 1 then break end
                emu.wait(1 / 60)
            end
            assert(read_byte(0xe804) == 1, "keybrk did not detect the break key")
            break_field:set_value(0)
            for frame = 1, 300 do
                if stage == "done" then break end
                emu.wait(1 / 60)
            end
            assert(stage == "done", "BASIC CALL did not return")
            assert(lcc2_device_result.returns == 1, "compiled program did not return")
            assert(lcc2_device_result.scan_empty == 255, "wrong no-key result")
            assert(lcc2_device_result.break_empty == 0, "wrong idle break result")
            assert(lcc2_device_result.scan_pressed == 51, "wrong scan key code")
            assert(lcc2_device_result.waited_key == 50, "wrong waited key code")
            assert(lcc2_device_result.break_pressed == 1, "wrong pressed break result")
            assert(lcc2_device_result.break_released == 0, "wrong released break result")
            assert(lcc2_device_result.completed == 42, "program did not complete")
            assert(lcc2_device_result.stack_balanced, "stack imbalance")
            assert(lcc2_device_result.registers_restored, "caller registers changed")
            -- BRK pressed during the program can leave BASIC eating the next key; clear with C-CE.
            emu.wait(0.5)
            key_press(":KEY7", 0x02)
            emu.wait(0.3)
            keyline("POKE&E608,42")
            assert(read_byte(0xe608) == 42, "BASIC is not responsive")
        end)
        lcc2_device_result.status = ok and "PASS" or "FAIL"
        if not ok then lcc2_device_result.error = tostring(message) end
        print("lcc2 device endpoints: " .. lcc2_device_result.status)
    end)
    local ok, message = coroutine.resume(lcc2_device_coroutine)
    assert(ok, message)
end

-- Run only in a disposable PC-1403 MAME session: this overwrites RAM and CPU state.
function lcc2_smoke(binary_path)
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces["program"]
    local ram = emu.item(cpu.items["0/m_ram"])
    local file = assert(io.open(binary_path, "rb"))
    local binary = file:read("*a")
    file:close()
    assert(#binary == 78, "unexpected smoke binary size")
    assert(binary:byte(8) == 0x78, "unexpected startup layout")
    local origin, trap, sentinel = 0xe030, 0xe03a, 0xe003
    for i = 1, #binary do
        space:write_u8(origin + i - 1, binary:byte(i))
    end
    -- CALL entry; JP sentinel. A temporary JP traps main's return before restore.
    local harness = {0x78, 0xe0, 0x30, 0x79, 0xe0, 0x03}
    for i, byte in ipairs(harness) do space:write_u8(0xe000 + i - 1, byte) end
    local expected = {11, 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187}
    for i, byte in ipairs(expected) do ram:write(i - 1, byte) end
    local original = {}
    for i = 0, 2 do original[i] = space:read_u8(trap + i) end
    local function install_trap()
        space:write_u8(trap, 0x79)
        space:write_u8(trap + 1, 0xe0)
        space:write_u8(trap + 2, 0x3a)
    end
    install_trap()
    cpu.state.R.value = 96
    cpu.state.PC.value = 0xe000
    lcc2_smoke_result = {status = "running", calls = 0}
    local stage, frames = "result", 0
    lcc2_smoke_subscription = emu.add_machine_frame_notifier(function()
        if lcc2_smoke_result.status ~= "running" then return end
        frames = frames + 1
        local pc = cpu.state.PC.value
        local function fail(message)
            lcc2_smoke_result.status = message
        end
        if frames > 120 then fail("FAIL: timeout"); return end
        if stage == "result" and pc == trap then
            lcc2_smoke_result.ba = cpu.state.BA.value
            lcc2_smoke_result.main_r = cpu.state.R.value
            if cpu.state.BA.value ~= 42 or cpu.state.R.value ~= 94 then
                fail("FAIL: arithmetic or evaluation stack"); return
            end
            for i = 0, 2 do space:write_u8(trap + i, original[i]) end
            stage = "return"
        elseif stage == "return" and pc == sentinel then
            lcc2_smoke_result.return_r = cpu.state.R.value
            if cpu.state.R.value ~= 96 then fail("FAIL: caller stack"); return end
            local dump = {}
            for i, byte in ipairs(expected) do
                dump[i] = string.format("%02X", ram:read(i - 1))
                if ram:read(i - 1) ~= byte then
                    fail("FAIL: register restoration at " .. (i - 1)); return
                end
            end
            lcc2_smoke_result.register_dump = table.concat(dump, " ")
            lcc2_smoke_result.calls = lcc2_smoke_result.calls + 1
            if lcc2_smoke_result.calls == 10 then
                lcc2_smoke_result.status = "PASS"
            else
                install_trap()
                stage = "result"
                cpu.state.PC.value = 0xe000
            end
        end
    end)
    emu.unpause()
end

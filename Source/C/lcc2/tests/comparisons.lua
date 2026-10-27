-- Disposable PC-1403 session only: overwrites RAM and CPU state.
function lcc2_comparison_test(binary_path)
    assert(manager.machine.system.name == "pc1403", "PC-1403 required")
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces.program
    local ram = emu.item(cpu.items["0/m_ram"])
    local file = assert(io.open(binary_path, "rb"))
    local binary = file:read("*a")
    file:close()
    local origin, caller, sentinel = 0xe030, 0xe000, 0xe003
    assert(#binary > 0 and origin + #binary < 0xf000, "binary overlaps results")
    local call_offset
    for i = 1, #binary do
        space:write_u8(origin + i - 1, binary:byte(i))
        if not call_offset and binary:byte(i) == 0x78 then call_offset = i end
    end
    assert(call_offset, "startup CALL not found")
    local trap = origin + call_offset - 1
    local original = {binary:byte(call_offset, call_offset + 2)}
    local vectors = {
        {0, 0}, {1, 1}, {65535, 65535}, {0, 65535}, {65535, 0},
        {255, 256}, {256, 255}, {0x1201, 0x1202}, {0x1202, 0x1201},
        {0x01ff, 0x0200}, {0x0200, 0x01ff}, {32767, 32768}, {32768, 32767}
    }
    local byte_vectors = {
        {0, 0}, {0, 1}, {1, 0}, {255, 255}, {254, 255},
        {255, 254}, {0, 255}, {255, 0}, {1, 255}, {255, 1}
    }
    local saved = {11, 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187}
    for i, byte in ipairs({0x78, 0xe0, 0x30, 0x79, 0xe0, 0x03}) do
        space:write_u8(caller + i - 1, byte)
    end
    local function install_trap()
        space:write_u8(trap, 0x79)
        space:write_u8(trap + 1, math.floor(trap / 256))
        space:write_u8(trap + 2, trap % 256)
    end
    local function write_word(address, value)
        space:write_u8(address, value % 256)
        space:write_u8(address + 1, math.floor(value / 256))
    end
    lcc2_comparison_result = {status = "running", calls = 0, vectors = #vectors}
    local stage, frames = "input", 0
    for i, byte in ipairs(saved) do ram:write(i - 1, byte) end
    cpu.state.R.value = 96
    cpu.state.PC.value = caller
    install_trap()
    lcc2_comparison_subscription = emu.add_machine_frame_notifier(function()
        if lcc2_comparison_result.status ~= "running" then return end
        frames = frames + 1
        local ok, message = pcall(function()
            assert(frames < 1200, "runtime comparison timeout")
            local pc = cpu.state.PC.value
            local vector = vectors[(lcc2_comparison_result.calls % #vectors) + 1]
            local byte_vector =
                byte_vectors[(lcc2_comparison_result.calls % #byte_vectors) + 1]
            if stage == "input" and pc == trap then
                assert(cpu.state.R.value == 94, "startup stack imbalance")
                write_word(0xf000, vector[1])
                write_word(0xf002, vector[2])
                space:write_u8(0xf020, byte_vector[1])
                space:write_u8(0xf021, byte_vector[2])
                for i = 0, 5 do space:write_u8(0xf010 + i, 0x55) end
                for i = 0, 5 do space:write_u8(0xf022 + i, 0x55) end
                for i, byte in ipairs(original) do space:write_u8(trap + i - 1, byte) end
                stage = "return"
            elseif stage == "return" and pc == sentinel then
                local a, b = vector[1], vector[2]
                local expected = {a == b, a ~= b, a < b, a <= b, a > b, a >= b}
                for i, truth in ipairs(expected) do
                    local actual = space:read_u8(0xf00f + i)
                    assert(actual == (truth and 255 or 0),
                        string.format("comparison %d: %04X vs %04X got %02X",
                            i, a, b, actual))
                end
                local byte_expected = {
                    byte_vector[1] == byte_vector[2],
                    byte_vector[1] ~= byte_vector[2],
                    byte_vector[1] < byte_vector[2],
                    byte_vector[1] <= byte_vector[2],
                    byte_vector[1] > byte_vector[2],
                    byte_vector[1] >= byte_vector[2]
                }
                for i, truth in ipairs(byte_expected) do
                    local actual = space:read_u8(0xf021 + i)
                    assert(actual == (truth and 255 or 0),
                        string.format("byte comparison %d: %02X vs %02X got %02X",
                            i, byte_vector[1], byte_vector[2], actual))
                end
                assert(cpu.state.R.value == 96, "caller stack imbalance")
                for i, byte in ipairs(saved) do
                    assert(ram:read(i - 1) == byte, "register restoration failed")
                end
                lcc2_comparison_result.calls = lcc2_comparison_result.calls + 1
                if lcc2_comparison_result.calls == #vectors * 10 then
                    lcc2_comparison_result.status = "PASS"
                else
                    install_trap()
                    stage = "input"
                    cpu.state.PC.value = caller
                end
            end
        end)
        if not ok then
            lcc2_comparison_result.status = "FAIL"
            lcc2_comparison_result.error = tostring(message)
        end
    end)
    emu.unpause()
end

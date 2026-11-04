-- Disposable session only: clears BASIC and reserves/overwrites the 2 KiB ASM area.
function lcc2_scalar_test(binary_path)
    assert(manager.machine.system.name == "pc1403", "PC-1403 required")
    assert(not manager.machine.paused, "resume emulation before testing")
    assert(type(keyline) == "function", "load pc1403_key.lua first")
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces.program
    local origin, limit = 0xe030, 0xe600
    local file = assert(io.open(binary_path, "rb"))
    local binary = file:read("*a")
    file:close()
    assert(#binary > 0 and origin + #binary <= limit, "binary overlaps test outputs")
    local function dump(address, count)
        local bytes = {}
        for offset = 0, count - 1 do
            bytes[#bytes + 1] = string.format("%02X", space:read_u8(address + offset))
        end
        return table.concat(bytes, " ")
    end
    lcc2_scalar_result = {status = "running", calls = 0}
    lcc2_scalar_coroutine = coroutine.create(function()
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
            for i = 1, #binary do
                space:write_u8(origin + i - 1, binary:byte(i))
            end
            for call = 1, 10 do
                for address = 0xe600, 0xe603 do space:write_u8(address, 0) end
                space:write_u8(0xe7ff, 0)
                space:write_u8(0xe800, 0)
                keyline("RUN")
                emu.wait(0.5)
                local actual = dump(0xe600, 4)
                local cross = dump(0xe7ff, 2)
                assert(actual == "2A 00 3B FF", "scalar outputs: " .. actual)
                assert(cross == "5E 12", "cross-page word: " .. cross)
                assert(space:read_u8(0xff01) + 256 * space:read_u8(0xff02) == basic_start,
                    "BASIC start pointer changed")
                assert(dump(basic_start, 64) == basic_before, "BASIC program changed")
                lcc2_scalar_result.calls = call
                lcc2_scalar_result.outputs = actual
                lcc2_scalar_result.cross_page = cross
            end
        end)
        lcc2_scalar_result.status = ok and "PASS" or "FAIL"
        if not ok then lcc2_scalar_result.error = tostring(message) end
        print("lcc2 scalar test: " .. lcc2_scalar_result.status)
        if not ok then print(tostring(message)) end
    end)
    local ok, message = coroutine.resume(lcc2_scalar_coroutine)
    assert(ok, message)
end

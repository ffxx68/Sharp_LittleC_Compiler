-- Disposable session only: clears BASIC and reserves/overwrites the 2 KiB ASM area.
function lcc2_discarded_assignments_test(binary_path)
    assert(manager.machine.system.name == "pc1403", "PC-1403 required")
    assert(not manager.machine.paused, "resume emulation before testing")
    assert(type(keyline) == "function", "load pc1403_key.lua first")
    local cpu = manager.machine.devices[":maincpu"]
    local space = cpu.spaces.program
    local origin, limit = 0xe030, 0xe700
    local file = assert(io.open(binary_path, "rb"))
    local binary = file:read("*a")
    file:close()
    assert(#binary > 0 and origin + #binary <= limit,
        "binary overlaps test outputs")
    local function dump(address, count)
        local bytes = {}
        for offset = 0, count - 1 do
            bytes[#bytes + 1] = string.format("%02X",
                space:read_u8(address + offset))
        end
        return table.concat(bytes, " ")
    end

    lcc2_discarded_assignments_result = {status = "running", calls = 0}
    lcc2_discarded_assignments_coroutine = coroutine.create(function()
        local ok, message = pcall(function()
            keyline("_||")
            keyline("NEW")
            keyline("POKE&FF01,&30,&E8")
            keyline("NEW")
            keyline("10 CALL &E030")
            keyline("20 END")
            keyline("|")
            local basic_start = space:read_u8(0xff01) +
                256 * space:read_u8(0xff02)
            assert(basic_start == 0xe830, "BASIC memory reservation failed")
            local basic_before = dump(basic_start, 64)
            for index = 1, #binary do
                space:write_u8(origin + index - 1, binary:byte(index))
            end
            for call = 1, 10 do
                for address = 0xe700, 0xe709 do
                    space:write_u8(address, 0)
                end
                keyline("RUN")
                emu.wait(0.5)
                local actual = dump(0xe700, 10)
                assert(actual == "A5 34 12 A5 A5 00 A5 34 12 05",
                    "assignment outputs or RHS call count: " .. actual)
                assert(space:read_u8(0xff01) +
                    256 * space:read_u8(0xff02) == basic_start,
                    "BASIC start pointer changed")
                assert(dump(basic_start, 64) == basic_before,
                    "BASIC program changed")
                lcc2_discarded_assignments_result.calls = call
                lcc2_discarded_assignments_result.outputs = actual
            end
        end)
        lcc2_discarded_assignments_result.status = ok and "PASS" or "FAIL"
        if not ok then
            lcc2_discarded_assignments_result.error = tostring(message)
        end
        print("lcc2 discarded assignments: " ..
            lcc2_discarded_assignments_result.status)
        if not ok then print(tostring(message)) end
    end)
    local ok, message = coroutine.resume(lcc2_discarded_assignments_coroutine)
    assert(ok, message)
end

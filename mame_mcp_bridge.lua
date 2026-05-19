-- mame_mcp_bridge.lua
-- Bridge file-based IPC per MAME MCP Server.
-- Non richiede LuaSocket. Funziona su MAME 0.227+
--
-- Protocollo:
--   MCP server scrive:  mame_cmd.json  (nella dir di MAME)
--   Bridge risponde:    mame_rsp.json
--   Entrambi i file vengono cancellati dopo la lettura/scrittura.

local json = require("json")  -- disponibile in MAME >= 0.227

-- Directory di lavoro (dove MAME è stato avviato)
local CMD_FILE = "mame_cmd.json"
local RSP_FILE = "mame_rsp.json"

-- ─── Utility ────────────────────────────────────────────────────────────────

local function log(msg)
    print("[MAME-BRIDGE] " .. tostring(msg))
end

local function ok(data)
    return json.stringify({ status = "ok", result = data })
end

local function err(msg)
    return json.stringify({ status = "error", message = tostring(msg) })
end

local function read_file(path)
    local f = io.open(path, "r")
    if not f then return nil end
    local content = f:read("*a")
    f:close()
    return content
end

local function write_file(path, content)
    local f = io.open(path, "w")
    if not f then return false end
    f:write(content)
    f:close()
    return true
end

local function delete_file(path)
    os.remove(path)
end

-- ─── Handler dei comandi ────────────────────────────────────────────────────

local handlers = {}

handlers["status"] = function(params)
    local machine = manager.machine
    local screen  = machine.screens[":screen"]
    local result = {
        system  = emu.gamename(),
        paused  = machine.paused,
        frame   = screen and screen:frame_number() or 0,
        width   = screen and screen.width  or 0,
        height  = screen and screen.height or 0,
    }
    local cpus = {}
    for tag, dev in pairs(machine.devices) do
        if dev.shortname ~= "" then
            table.insert(cpus, { tag = tag, name = dev.shortname })
        end
    end
    result.devices = cpus
    return ok(result)
end

handlers["pause"] = function(params)
    manager.machine:pause()
    return ok({ paused = true })
end

handlers["resume"] = function(params)
    manager.machine:resume()
    return ok({ paused = false })
end

handlers["reset"] = function(params)
    manager.machine:soft_reset()
    return ok({ reset = "soft" })
end

handlers["hard_reset"] = function(params)
    manager.machine:hard_reset()
    return ok({ reset = "hard" })
end

handlers["read_memory"] = function(params)
    local space_tag = params.space or ":maincpu"
    local address   = params.address
    local size      = params.size or 1
    local signed    = params.signed or false
    local cpu = manager.machine.devices[space_tag]
    if not cpu then return err("Dispositivo non trovato: " .. space_tag) end
    local space = cpu.spaces["program"]
    if not space then return err("Spazio 'program' non trovato") end
    local value
    if     size == 1 then value = space:read_u8(address)
    elseif size == 2 then value = space:read_u16(address)
    elseif size == 4 then value = space:read_u32(address)
    elseif size == 8 then value = space:read_u64(address)
    else return err("Size non valida: " .. tostring(size)) end
    if signed then
        local max = 2^(size*8-1)
        if value >= max then value = value - 2^(size*8) end
    end
    return ok({ address = address, value = value, size = size })
end

handlers["read_memory_block"] = function(params)
    local space_tag = params.space or ":maincpu"
    local address   = params.address
    local length    = math.min(params.length or 16, 4096)
    local cpu = manager.machine.devices[space_tag]
    if not cpu then return err("Dispositivo non trovato: " .. space_tag) end
    local space = cpu.spaces["program"]
    if not space then return err("Spazio 'program' non trovato") end
    local bytes = {}
    for i = 0, length - 1 do
        bytes[i+1] = space:read_u8(address + i)
    end
    return ok({ address = address, bytes = bytes, length = length })
end

handlers["write_memory"] = function(params)
    local space_tag = params.space or ":maincpu"
    local address   = params.address
    local value     = params.value
    local size      = params.size or 1
    local cpu = manager.machine.devices[space_tag]
    if not cpu then return err("Dispositivo non trovato: " .. space_tag) end
    local space = cpu.spaces["program"]
    if not space then return err("Spazio 'program' non trovato") end
    if     size == 1 then space:write_u8(address, value)
    elseif size == 2 then space:write_u16(address, value)
    elseif size == 4 then space:write_u32(address, value)
    elseif size == 8 then space:write_u64(address, value)
    else return err("Size non valida: " .. tostring(size)) end
    return ok({ address = address, written = value, size = size })
end

handlers["get_registers"] = function(params)
    local device_tag = params.device or ":maincpu"
    local cpu = manager.machine.devices[device_tag]
    if not cpu then return err("Dispositivo non trovato: " .. device_tag) end
    local regs = {}
    for name, val in pairs(cpu.state) do
        regs[name] = val.value
    end
    return ok({ device = device_tag, registers = regs })
end

handlers["set_register"] = function(params)
    local device_tag = params.device or ":maincpu"
    local reg_name   = params.register
    local value      = params.value
    local cpu = manager.machine.devices[device_tag]
    if not cpu then return err("Dispositivo non trovato: " .. device_tag) end
    if not cpu.state[reg_name] then return err("Registro non trovato: " .. reg_name) end
    cpu.state[reg_name].value = value
    return ok({ device = device_tag, register = reg_name, value = value })
end

handlers["list_inputs"] = function(params)
    local ports = {}
    for tag, port in pairs(manager.machine.ioport.ports) do
        local fields = {}
        for fname, field in pairs(port.fields) do
            table.insert(fields, fname)
        end
        table.insert(ports, { port = tag, fields = fields })
    end
    return ok({ ports = ports })
end

handlers["send_input"] = function(params)
    local port_tag  = params.port
    local field_name = params.field
    local value     = params.value or 1
    local port = manager.machine.ioport.ports[port_tag]
    if not port then return err("Porta non trovata: " .. port_tag) end
    local field = port.fields[field_name]
    if not field then return err("Campo non trovato: " .. field_name) end
    field:set_value(value)
    return ok({ port = port_tag, field = field_name, value = value })
end

handlers["exec_lua"] = function(params)
    local code = params.code or ""
    local fn, compile_err = load(code, "exec_lua", "t", _G)
    if not fn then return err("Errore compilazione: " .. tostring(compile_err)) end
    local ok_run, result = pcall(fn)
    if not ok_run then return err("Errore esecuzione: " .. tostring(result)) end
    return ok({ result = tostring(result ~= nil and result or "ok") })
end

handlers["cheat_read"] = function(params)    local space_tag = params.space or ":maincpu"
    local address   = params.address
    local length    = math.min(params.length or 16, 4096)
    local cpu = manager.machine.devices[space_tag]
    if not cpu then return err("Dispositivo non trovato: " .. space_tag) end
    local space = cpu.spaces["program"]
    if not space then return err("Spazio 'program' non trovato") end
    local lines = {}
    for i = 0, length - 1, 16 do
        local row = {}
        local asc = {}
        for j = 0, 15 do
            if i + j < length then
                local b = space:read_u8(address + i + j)
                table.insert(row, string.format("%02X", b))
                table.insert(asc, (b >= 32 and b < 127) and string.char(b) or ".")
            end
        end
        table.insert(lines, string.format("%08X  %-48s  |%s|",
            address + i,
            table.concat(row, " "),
            table.concat(asc, "")))
    end
    return ok({ address = address, hex = table.concat(lines, "\n") })
end

-- ─── Poll su ogni frame ─────────────────────────────────────────────────────

local function poll()

    local content = read_file(CMD_FILE)
    if not content or content == "" then return end
    delete_file(CMD_FILE)

    local ok_parse, req = pcall(json.parse, content)
    if not ok_parse or type(req) ~= "table" then
        write_file(RSP_FILE, err("JSON non valido"))
        return
    end

    local cmd = req.command or ""
    local params = req.params or {}
    local handler = handlers[cmd]
    local response
    if handler then
        local ok_run, result = pcall(handler, params)
        response = ok_run and result or err(tostring(result))
    else
        response = err("Comando sconosciuto: " .. cmd)
    end

    write_file(RSP_FILE, response .. "\n")
end

-- ─── Avvio ──────────────────────────────────────────────────────────────────

delete_file(CMD_FILE)
delete_file(RSP_FILE)

emu.register_frame_done(poll, "mame_bridge_frame")

-- register_periodic disponibile da MAME 0.221+ - funziona anche in pausa/debug
if emu.register_periodic then
    emu.register_periodic(poll, "mame_bridge_periodic")
    log("Bridge attivo: frame_done + periodic (DEBUG-safe)")
else
    log("Bridge attivo: solo frame_done (premi F5 per attivare in debug)")
end
log("CMD=" .. CMD_FILE .. "  RSP=" .. RSP_FILE)

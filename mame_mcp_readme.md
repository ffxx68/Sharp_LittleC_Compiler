---



---
# MAME MCP Server

Control MAME from Claude (or any MCP client) via a Python server.

## Requirements

- **MAME** installed on Windows (tested with 0.267+)
- **Python 3.11+** — use the full path (e.g. `C:\Python313\python.exe`), not the Windows Store stub
- Python dependencies:

```bash
pip install mcp httpx pillow mss
```

## Project Files

| File                  | Description                                       |
|-----------------------|---------------------------------------------------|
| `mame_mcp_server.py`  | Main MCP server (stdio)                           |
| `mame_mcp_bridge.lua` | Lua script loaded inside MAME (TCP bridge)        |

## Setup

### 1. Copy files to the MAME folder

```
C:\mame\
  ├── mame.exe
  ├── mame_mcp_bridge.lua   ← copy here
  ├── roms\
  └── ...
```

### 2. Configure environment variables (optional)

Variables can be set in the system or in the Claude Desktop config file.

| Variable          | Default                   | Description                |
|-------------------|--------------------------|----------------------------|
| `MAME_EXE`        | `C:\mame\mame.exe`        | Path to mame.exe           |
| `MAME_ROM_PATH`   | `C:\mame\roms`            | ROMs folder                |
| `MAME_LUA_BRIDGE` | `C:\mame\mame_bridge.lua` | Path to Lua bridge         |
| `MAME_BRIDGE_PORT`| `6789`                    | TCP port for the bridge    |
| `MAME_HTTP_PORT`  | `8080`                    | MAME HTTP port             |

### 3. Configure the MCP client

#### Option A — IntelliJ IDEA (GitHub Copilot)

Open `%LOCALAPPDATA%\github-copilot\intellij\mcp.json` and add the `mame` entry in the `servers` section:

```json
{
  "servers": {
    "mame": {
      "command": "C:\\Python313\\python.exe",
      "args": [
        "-u",
        "C:/Users/<user>/mame/mame_mcp_server.py"
      ],
      "env": {
        "MAME_EXE": "C:\\Users\\<user>\\mame\\mame.exe",
        "MAME_ROM_PATH": "C:\\Users\\<user>\\mame\\roms",
        "MAME_LUA_BRIDGE": "C:\\Users\\<user>\\mame\\mame_mcp_bridge.lua"
      }
    }
  }
}
```

> **Notes:**
> - Use the full path to `python.exe` (not just `python`): the Windows Store stub does not find installed packages.
> - The `mcp.json` file may contain other MCP servers already configured — just add the `mame` entry.
> - After saving, **Refresh** the server from the IntelliJ AI Assistant (or restart the IDE).

#### Option B — Claude Desktop

Open `%APPDATA%\Claude\claude_desktop_config.json` and add:

```json
{
  "mcpServers": {
    "mame": {
      "command": "C:\\Python313\\python.exe",
      "args": ["-u", "C:\\path\\mame_mcp_server.py"],
      "env": {
        "MAME_EXE": "C:\\mame\\mame.exe",
        "MAME_ROM_PATH": "C:\\mame\\roms",
        "MAME_LUA_BRIDGE": "C:\\mame\\mame_mcp_bridge.lua"
      }
    }
  }
}
```

Restart Claude Desktop.

## Available Tools

| Tool                | Description                                 |
|---------------------|---------------------------------------------|
| `mame_launch`       | Launches MAME with a ROM                    |
| `mame_stop`         | Terminates MAME                             |
| `mame_status`       | Current status (system, frame, pause)       |
| `mame_pause`        | Pauses emulation                            |
| `mame_resume`       | Resumes emulation                           |
| `mame_reset`        | Soft or hard reset                          |
| `mame_screenshot`   | Takes a screenshot (returns image)          |
| `mame_read_memory`  | Reads 1/2/4/8 bytes from an address         |
| `mame_read_memory_block` | Reads a block (hex dump)               |
| `mame_write_memory` | Writes a value to memory                    |
| `mame_get_registers`| Reads all CPU registers                     |
| `mame_set_register` | Sets a CPU register                         |
| `mame_list_inputs`  | Lists I/O ports/fields                      |
| `mame_send_input`   | Simulates an input (key/joystick)           |
| `mame_list_roms`    | Lists ROMs in the folder                    |
| `mame_cheat_read`   | Hex dump for finding values (cheat)         |
| `mame_exec_lua`     | Executes arbitrary Lua code in MAME         |


## Usage 

### Manual MAME launch and Lua bridge loading

If you want to start MAME manually (for advanced debugging or to see the Lua console in the foreground), follow these steps:

1. **Open a command prompt** and move to the MAME folder:
   ```sh
   cd C:\Users\<user>\mame
   ```

2. **Start MAME** with the desired ROM and debug/console options:
   ```sh
   mame.exe pc1403 -debug -nomaximize -console
   ```
   *(Replace `pc1403` with the ROM you want to use)*

3. **Manually load the Lua bridge** from the MAME Lua console (which appears in the console window):
   ```
   [MAME]> dofile("mame_mcp_bridge.lua")
   ```
   After this command, the file-based bridge will be active and you can use all MCP tools from IntelliJ or other clients.

> **Note:**  
> If you want to load additional scripts (e.g. `key.lua`), use:
> ```
> [MAME]> dofile("key.lua")
> ```

### Example commands (in Claude Desktop, or IntelliJ Copilot)

Some snippets, or natural language commands, to control MAME:

"Start PC-1403 with debug and a console"
(this one isn't needed, if MAME is already running with the bridge loaded)

which will execute the command → `mame_launch(rom="pacman")`

"Show me the CPU registers"
→ `mame_get_registers()`

"Hex dump from 0xFF00 for 256 bytes"
→ `mame_read_memory_block(address=0xFF00, length=256)`

"Pause and then show the screen"
→ `mame_pause()`

"Take a screenshot" → `mame_screenshot()` (*to be tested!*)

...

## Technical Notes

### How the Lua bridge works

When `mame_mcp_bridge.lua` is loaded by MAME, at startup via `-autoboot_script`, it's going to  
use a **file-based IPC** to exchange commands with the Python MCP server:

- the Python server writes the command to `mame_cmd.json` in the MAME directory
- the Lua bridge reads the file every frame (`register_frame_done`) and every second (`register_periodic`)
- tt writes the response to `mame_rsp.json`, which Python reads and deletes

Each command is a JSON file:
```json
{"command": "read_memory", "params": {"space": ":maincpu", "address": 57392, "size": 1}}
```

Each response is a JSON file:
```json
{"status": "ok", "result": {"address": 57392, "value": 128, "size": 1}}
```

> **Debug mode note:** with `-debug` the bridge responds via `register_periodic`  
> (about once per second). Commands can take up to ~10 seconds.  
> When MAME is running (F5), it responds every frame (~60 responses/second).

### Supported Lua commands

`status` · `pause` · `resume` · `reset` · `hard_reset` · `read_memory` · `read_memory_block` · `write_memory` · `get_registers` · `set_register` · `list_inputs` · `send_input` · `cheat_read` · `exec_lua`

### exec_lua — debugger commands and arbitrary Lua

`mame_exec_lua` executes Lua code directly in the MAME environment. Practical examples:

```
# Load a binary into memory (equivalent to debugger command "load")
mame_exec_lua('manager.machine.debugger:command("load tmp.bin,e030:maincpu")')

# Set a breakpoint
mame_exec_lua('manager.machine.debugger:command("bps e030")')

# Load key.lua for keyboard automation
mame_exec_lua('dofile("key.lua")')
```

> **Note:** The `key()` / `keyfile()` functions in `key.lua` use `emu.wait()` which requires  
> the Lua console coroutine context. They must be run from the MAME Lua console  
> (visible if MAME is started with `-console` and `CREATE_NEW_CONSOLE`).

### Screenshot

The screenshot uses `mss` to capture the primary monitor. If you prefer to capture only the MAME window, you can extend the code using `pygetwindow`:

```python
pip install pygetwindow
```

## Troubleshooting

**`MCP error -32000: Connection closed` on first start**  
→ The server uses `FastMCP` (mcp >= 1.0). Make sure the `mcp` package is installed for the correct Python:  
`C:\Python313\python.exe -c "from mcp.server.fastmcp import FastMCP; print('ok')"`

**"Bridge did not respond in 30s"**  
→ In debug mode the bridge is slow (~6-10s per response). Normal.  
→ If MAME is running (not paused), responses arrive in less than 1s.  
→ Check for stale `mame_cmd.json` / `mame_rsp.json` files in the MAME dir.

**"mame.exe not found"**  
→ Set the `MAME_EXE` environment variable in the `mcp.json` file.

**`Fatal error: module 'socket' not found` in Lua**  
→ LuaSocket is not available in MAME 0.267. The bridge uses file IPC: no problem.

**The MAME Lua console is not visible**  
→ MAME is started with `CREATE_NEW_CONSOLE` — a separate console window should open.  
→ If it doesn't appear, start MAME manually from `cmd`: `cd C:\Users\<user>\mame && mame.exe pc1403 -debug -nomaximize -console`

**`emu.wait()` does not work from `mame_exec_lua`**  
→ `emu.wait()` requires the Lua console coroutine context. Use it directly from the MAME console window.

## Future extensions

- [ ] Watchpoint on memory addresses (notify when a value changes)
- [ ] Video recording via MAME `-aviwrite`
- [ ] CPU breakpoints via Lua debugger hook
- [ ] Automatic RAM search (cheat engine-like)
- [ ] Savestate save/load

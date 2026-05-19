"""
mame_mcp_server.py  (FastMCP 1.x)
==================================
Server MCP per controllare MAME su Windows.

Canali verso MAME:
  - subprocess  → avvio/stop del processo mame.exe
  - TCP socket  → mame_bridge.lua (porta 6789)
  - HTTP        → MAME HTTP API (porta 8080)

Setup:
  pip install mcp httpx pillow mss

Avvio (stdio MCP):
  python -u mame_mcp_server.py
"""

import asyncio
import base64
import json
import logging
import os
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional

import httpx
from mcp.server.fastmcp import FastMCP
from mcp import types

# ─── Logging su stderr ───────────────────────────────────────────────────────

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    stream=sys.stderr,
)
logger = logging.getLogger(__name__)

# ─── Configurazione ──────────────────────────────────────────────────────────

MAME_EXE        = os.environ.get("MAME_EXE",        r"C:\mame\mame.exe")
MAME_ROM_PATH   = os.environ.get("MAME_ROM_PATH",   r"C:\mame\roms")
MAME_LUA_BRIDGE = os.environ.get("MAME_LUA_BRIDGE", r"C:\mame\mame_bridge.lua")

_mame_proc: Optional[subprocess.Popen] = None
_mame_dir: str = str(Path(MAME_EXE).parent)

# ─── FastMCP ─────────────────────────────────────────────────────────────────

mcp = FastMCP("mame-mcp")

# ─── Bridge Lua file-based IPC ───────────────────────────────────────────────

class LuaBridge:
    """Comunicazione con il bridge Lua via file JSON nella dir di MAME."""

    @property
    def mame_dir(self) -> str:
        return str(Path(MAME_EXE).parent)

    async def send(self, command: str, params: Optional[dict] = None) -> dict:
        cmd_file = os.path.join(self.mame_dir, "mame_cmd.json")
        rsp_file = os.path.join(self.mame_dir, "mame_rsp.json")
        payload  = json.dumps({"command": command, "params": params or {}})
        try:
            # Pulisce file residui da chiamate precedenti
            if os.path.exists(cmd_file):
                os.remove(cmd_file)
            if os.path.exists(rsp_file):
                os.remove(rsp_file)
            with open(cmd_file, "w") as f:
                f.write(payload)
            deadline = time.monotonic() + 30.0
            while time.monotonic() < deadline:
                if os.path.exists(rsp_file):
                    await asyncio.sleep(0.02)
                    try:
                        with open(rsp_file, "r") as f:
                            data = f.read().strip()
                        os.remove(rsp_file)
                        return json.loads(data)
                    except Exception:
                        pass
                await asyncio.sleep(0.05)
            return {"status": "error", "message": "Timeout: bridge non ha risposto in 30s. MAME è in esecuzione?"}
        except Exception as e:
            return {"status": "error", "message": str(e)}


bridge = LuaBridge()

# ─── Tools ───────────────────────────────────────────────────────────────────

@mcp.tool()
async def mame_launch(rom: str, extra_args: list[str] = []) -> str:
    """
    Avvia MAME con una ROM specificata.
    Carica automaticamente il bridge Lua per il controllo avanzato.

    Args:
        rom: Nome della ROM (es. 'pacman', 'mslug', 'sf2')
        extra_args: Argomenti aggiuntivi per mame.exe (opzionale)
    """
    global _mame_proc
    if _mame_proc and _mame_proc.poll() is None:
        return "❌ MAME è già in esecuzione. Usa mame_stop prima."
    mame_dir = str(Path(MAME_EXE).parent)
    cmd = [
        MAME_EXE, rom,
        "-rompath", MAME_ROM_PATH,
        "-autoboot_script", MAME_LUA_BRIDGE,
    ] + extra_args
    try:
        _mame_proc = subprocess.Popen(
            cmd,
            cwd=mame_dir,
            creationflags=(subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NEW_CONSOLE) if sys.platform == "win32" else 0,
        )
    except FileNotFoundError:
        return f"❌ mame.exe non trovato in: {MAME_EXE}\nImposta MAME_EXE come variabile d'ambiente."
    result = {"status": "error"}
    for _ in range(16):
        await asyncio.sleep(0.5)
        result = await bridge.send("status")
        if result.get("status") == "ok":
            break
    bridge_ok = result.get("status") == "ok"
    return (
        f"✅ MAME avviato (PID {_mame_proc.pid}) con ROM '{rom}'.\n"
        f"Bridge Lua: {'✅ connesso' if bridge_ok else '⚠️ non ancora pronto'}"
    )


@mcp.tool()
async def mame_stop() -> str:
    """Termina il processo MAME corrente."""
    global _mame_proc
    if not _mame_proc or _mame_proc.poll() is not None:
        return "MAME non è in esecuzione."
    _mame_proc.terminate()
    try:
        _mame_proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        _mame_proc.kill()
    _mame_proc = None
    return "✅ MAME terminato."


@mcp.tool()
async def mame_status() -> str:
    """Restituisce lo stato attuale dell'emulazione: sistema, pausa, frame, dispositivi."""
    running = _mame_proc is not None and _mame_proc.poll() is None
    result = await bridge.send("status")
    data = {"mame_process_running": running, "pid": _mame_proc.pid if running else None, **result}
    return json.dumps(data, indent=2, ensure_ascii=False)


@mcp.tool()
async def mame_pause() -> str:
    """Mette in pausa l'emulazione."""
    result = await bridge.send("pause")
    return "⏸️ Emulazione in pausa." if result.get("status") == "ok" else f"❌ {result.get('message', 'errore')}"


@mcp.tool()
async def mame_resume() -> str:
    """Riprende l'emulazione dalla pausa."""
    result = await bridge.send("resume")
    return "▶️ Emulazione ripresa." if result.get("status") == "ok" else f"❌ {result.get('message', 'errore')}"


@mcp.tool()
async def mame_reset(hard: bool = False) -> str:
    """
    Esegue un reset della macchina emulata.

    Args:
        hard: Se true esegue hard reset, altrimenti soft reset (default: false)
    """
    result = await bridge.send("hard_reset" if hard else "reset")
    return f"🔄 {'Hard' if hard else 'Soft'} reset eseguito." if result.get("status") == "ok" else f"❌ {result.get('message', 'errore')}"


@mcp.tool()
async def mame_screenshot() -> list:
    """Cattura uno screenshot della schermata corrente di MAME e lo restituisce come immagine."""
    try:
        import mss
        from PIL import Image
        import io
        with mss.mss() as sct:
            sct_img = sct.grab(sct.monitors[1])
            img = Image.frombytes("RGB", sct_img.size, sct_img.bgra, "raw", "BGRX")
            buf = io.BytesIO()
            img.save(buf, format="PNG")
            img_b64 = base64.b64encode(buf.getvalue()).decode()
        return [types.ImageContent(type="image", data=img_b64, mimeType="image/png")]
    except ImportError:
        pass
    try:
        async with httpx.AsyncClient(timeout=5) as client:
            r = await client.get(f"{HTTP_BASE}/screenshot")
            if r.status_code == 200:
                return [types.ImageContent(type="image", data=base64.b64encode(r.content).decode(), mimeType="image/png")]
    except Exception as e:
        return [types.TextContent(type="text", text=f"❌ Screenshot fallito: {e}")]
    return [types.TextContent(type="text", text="❌ Installa mss: pip install mss pillow")]


@mcp.tool()
async def mame_read_memory(address: int, size: int = 1, device: str = ":maincpu", signed: bool = False) -> str:
    """
    Legge uno o più byte dalla memoria del sistema emulato.

    Args:
        address: Indirizzo di memoria (es. 49152 oppure 0xC000)
        size: Dimensione in byte: 1, 2, 4 o 8 (default: 1)
        device: Tag del dispositivo CPU (default: ':maincpu')
        signed: Se true legge come intero con segno (default: false)
    """
    result = await bridge.send("read_memory", {"space": device, "address": address, "size": size, "signed": signed})
    if result.get("status") == "ok":
        r = result["result"]
        return f"Indirizzo:  {hex(r['address'])}\nValore:     {r['value']} (0x{r['value']:X})\nDimensione: {r['size']} byte"
    return f"❌ {result.get('message', 'errore')}"


@mcp.tool()
async def mame_read_memory_block(address: int, length: int, device: str = ":maincpu") -> str:
    """
    Legge un blocco di N byte consecutivi dalla memoria (max 4096). Restituisce un hex dump.

    Args:
        address: Indirizzo di partenza
        length: Numero di byte da leggere (max 4096)
        device: Tag CPU (default: ':maincpu')
    """
    result = await bridge.send("read_memory_block", {"space": device, "address": address, "length": min(length, 4096)})
    if result.get("status") == "ok":
        r = result["result"]
        bytes_ = r["bytes"]
        addr = r["address"]
        lines = []
        for i in range(0, len(bytes_), 16):
            row = bytes_[i:i+16]
            hex_part = " ".join(f"{b:02X}" for b in row)
            asc_part = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
            lines.append(f"{addr+i:08X}  {hex_part:<48}  |{asc_part}|")
        return "Hex dump:\n" + "\n".join(lines)
    return f"❌ {result.get('message', 'errore')}"


@mcp.tool()
async def mame_write_memory(address: int, value: int, size: int = 1, device: str = ":maincpu") -> str:
    """
    Scrive un valore in memoria al sistema emulato.

    Args:
        address: Indirizzo di memoria
        value: Valore da scrivere
        size: Dimensione in byte: 1, 2, 4 o 8 (default: 1)
        device: Tag CPU (default: ':maincpu')
    """
    result = await bridge.send("write_memory", {"space": device, "address": address, "value": value, "size": size})
    if result.get("status") == "ok":
        r = result["result"]
        return f"✅ Scritto {r['written']} (0x{r['written']:X}) all'indirizzo {hex(r['address'])}"
    return f"❌ {result.get('message', 'errore')}"


@mcp.tool()
async def mame_get_registers(device: str = ":maincpu") -> str:
    """
    Legge tutti i registri della CPU specificata.

    Args:
        device: Tag CPU (default: ':maincpu')
    """
    result = await bridge.send("get_registers", {"device": device})
    if result.get("status") == "ok":
        r = result["result"]
        lines = [f"Registri CPU [{r['device']}]:"] + [f"  {k:<10} = {v}" for k, v in sorted(r["registers"].items())]
        return "\n".join(lines)
    return f"❌ {result.get('message', 'errore')}"


@mcp.tool()
async def mame_set_register(register: str, value: int, device: str = ":maincpu") -> str:
    """
    Imposta il valore di un registro CPU specifico.

    Args:
        register: Nome del registro (es. 'D0', 'PC', 'A')
        value: Nuovo valore
        device: Tag CPU (default: ':maincpu')
    """
    result = await bridge.send("set_register", {"device": device, "register": register, "value": value})
    if result.get("status") == "ok":
        r = result["result"]
        return f"✅ Registro {r['register']} impostato a {r['value']}"
    return f"❌ {result.get('message', 'errore')}"


@mcp.tool()
async def mame_list_inputs() -> str:
    """Elenca tutte le porte di input disponibili e i relativi campi."""
    result = await bridge.send("list_inputs")
    if result.get("status") == "ok":
        ports = result["result"]["ports"]
        lines = []
        for p in sorted(ports, key=lambda x: x["port"]):
            lines.append(f"\n📌 {p['port']}")
            for f in sorted(p["fields"]):
                lines.append(f"     └─ {f}")
        return "Porte I/O disponibili:" + "".join(lines)
    return f"❌ {result.get('message', 'errore')}"


@mcp.tool()
async def mame_send_input(port: str, field: str, value: int = 1) -> str:
    """
    Simula la pressione/rilascio di un tasto o pulsante.

    Args:
        port: Tag della porta I/O (es. ':IN0')
        field: Nome del campo (es. 'P1 Button 1', 'Coin 1')
        value: 1=premi, 0=rilascia (default: 1)
    """
    result = await bridge.send("send_input", {"port": port, "field": field, "value": value})
    if result.get("status") == "ok":
        r = result["result"]
        return f"🎮 Input {'premuto' if r['value'] else 'rilasciato'}: {r['port']} / {r['field']}"
    return f"❌ {result.get('message', 'errore')}"


@mcp.tool()
def mame_list_roms(filter: str = "") -> str:
    """
    Elenca le ROM disponibili nella cartella ROM di MAME.

    Args:
        filter: Filtra i nomi ROM che contengono questa stringa (opzionale)
    """
    rom_path = Path(MAME_ROM_PATH)
    if not rom_path.exists():
        return f"❌ Cartella ROM non trovata: {MAME_ROM_PATH}"
    filter_lower = filter.lower()
    roms = [f.stem for f in sorted(rom_path.iterdir())
            if f.suffix.lower() in (".zip", ".7z", ".chd") and (not filter_lower or filter_lower in f.stem.lower())]
    if not roms:
        return "Nessuna ROM trovata" + (f" con filtro '{filter}'" if filter else "") + f" in {MAME_ROM_PATH}"
    return f"ROM disponibili ({len(roms)}):\n" + "\n".join(f"  {r}" for r in roms)


@mcp.tool()
async def mame_exec_lua(code: str) -> str:
    """
    Esegue codice Lua arbitrario nell'ambiente MAME.
    Utile per dofile(), definire funzioni, ecc.

    Args:
        code: Codice Lua da eseguire (es. 'dofile("key.lua")')
    """
    result = await bridge.send("exec_lua", {"code": code})
    if result.get("status") == "ok":
        return f"✅ {result['result'].get('result', 'ok')}"
    return f"❌ {result.get('message', 'errore')}"


@mcp.tool()
async def mame_cheat_read(address: int, length: int, device: str = ":maincpu") -> str:
    """
    Legge un range di memoria in formato hex dump. Utile per trovare valori (vite, punteggio) e fare cheat.

    Args:
        address: Indirizzo di partenza
        length: Quanti byte leggere
        device: Tag CPU (default: ':maincpu')
    """
    result = await bridge.send("cheat_read", {"space": device, "address": address, "length": length})
    if result.get("status") == "ok":
        r = result["result"]
        return f"📋 Hex dump @ {r['address']}:\n{r['hex']}"
    return f"❌ {result.get('message', 'errore')}"


# ─── Entry point ─────────────────────────────────────────────────────────────

def main():
    mcp.run(transport="stdio")


if __name__ == "__main__":
    main()


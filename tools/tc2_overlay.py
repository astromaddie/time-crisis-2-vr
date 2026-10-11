"""Reproducible, fail-closed overlays; never modify the pinned upstream checkout."""
from pathlib import Path
import argparse
ROOT = Path(__file__).resolve().parents[1]
PIN = "0837a397ebe554dbf5341d95629aa1e29fc58a52"

def generate(output, headless=False):
    import subprocess
    upstream = ROOT / "upstream"
    if subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=upstream, text=True).strip() != PIN:
        raise RuntimeError("Unexpected upstream revision")
    output = Path(output); output.mkdir(parents=True, exist_ok=True)
    def emit(name, replacements):
        text = subprocess.check_output(["git", "show", "HEAD:tc2/src/" + name], cwd=upstream).decode()
        for old, new in replacements:
            if text.count(old) != 1:
                raise RuntimeError(f"Overlay anchor mismatch: {name}: {old[:80]}")
            text = text.replace(old, new)
        path = output / name
        if not path.exists() or path.read_text() != text:
            path.write_text(text)
    emit("tc2_chips.c", [
        ('#include "tc2_host.h"', '#include "tc2_host.h"\n#include "tc2_link.h"'),
        ('*v = c422[(p - 0x06400000u) >> 1];', '*v = tc2_link_read((p - 0x06400000u) >> 1, c422);'),
        ('        if (k == 1) { if (v == 0xFFFBu)', '        if (tc2_link_enabled()) { tc2_link_write(k, (uint16_t)v, c422); return 1; }\n        if (k == 1) { if (v == 0xFFFBu)'),
    ])
    emit("tc2_h8.c", [
        ('static const uint8_t io_dsw = 0xFF, io_sw = 0x03;', '#include "tc2_link.h"\n#define io_dsw (tc2_link_enabled() ? 0xF7 : 0xFF)\nstatic const uint8_t io_sw = 0x03;'),
        ('return (uint8_t)~((!!(in & IN_PEDAL)) << 1 | (!!(in & IN_TRIGGER)));', 'return (uint8_t)~((tc2_link_side() << 2) | (!!(in & IN_PEDAL)) << 1 | (!!(in & IN_TRIGGER)));'),
    ])
    replacements = [
        ('#include "tc2_host.h"', '#include "tc2_host.h"\n#include "tc2_link.h"'),
        ('    scripted_inputs(frame);', '    tc2_link_vblank(frame);\n    scripted_inputs(frame);'),
        ('    ins_total += n;', '    ins_total += n;\n    tc2_link_poll();'),
        ('    tc2_io_flush();', '    tc2_link_report();\n    tc2_io_flush();'),
    ]
    if headless:
        replacements += [
            ('#include <SDL2/SDL.h>', '/* Headless host has no SDL dependency. */'),
            ('static void gpu_gate(uint32_t frame)\n{', '#if 0\nstatic void gpu_gate(uint32_t frame)\n{'),
            ('/* THE SESSION RECORDER:', '#endif\nstatic void gpu_gate(uint32_t frame) { (void)frame; }\n/* THE SESSION RECORDER:'),
            ('        { char *bp = SDL_GetBasePath();', '#if 0\n        { char *bp = SDL_GetBasePath();'),
            ('        int tc2_rom_setup(', '#endif\n        int tc2_rom_setup('),
            ('            if (!strcmp(argv[1], "--window")) SDL_ShowSimpleMessageBox', '#if 0\n            if (!strcmp(argv[1], "--window")) SDL_ShowSimpleMessageBox'),
            ('            return 1;\n        }\n    }', '#endif\n            return 1;\n        }\n    }'),
        ]
    emit("tc2_main.c", replacements)
    if headless:
        emit("tc2_cpu.c", [
            ('int rd_on, rd_stop_on, rd_poll_rec;', 'int rd_on = 1, rd_stop_on, rd_poll_rec;'),
            ('int rd_hook(uint32_t ep) { (void)ep; return 0; }', '#include "tc2_link.h"\nint rd_hook(uint32_t ep) { tc2_link_observe(ep); return 0; }'),
        ])

if __name__ == "__main__":
    p = argparse.ArgumentParser(); p.add_argument("output", type=Path); p.add_argument("--headless", action="store_true")
    a = p.parse_args(); generate(a.output, a.headless)

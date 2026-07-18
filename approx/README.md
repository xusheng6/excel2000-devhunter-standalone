# Dev Hunter — standalone

A standalone native reimplementation of **Dev Hunter**, the hidden car-shooter
easter egg in Microsoft Excel 2000 (`MSOWC.DLL`). You drive down a highway,
the names of the Excel 2000 developers scroll past painted on the road, and you
hunt and shoot the "developer" cars ahead of you.

This is a clean C + Win32/GDI rewrite reverse-engineered from the original DLL —
no Excel, no DirectDraw, no DLL required to run. It reuses the material recovered
from the binary: the exact 256-colour palette, the decrypted developer-name /
credits text, the game's floating-point RNG, and the original's 10°-camera /
focal-864 road projection. See `REVERSE_ENGINEERING.md` for the full teardown and
`re/` for the raw HLIL/disassembly.

![screenshot](screenshot.png)

## Build

Needs MSVC (any recent Visual Studio). It builds **32-bit**.

```
build.bat            ->  devhunter.exe
```

(If your VS is not VS 2022 Professional, edit the `vcvars32.bat` path at the top
of `build.bat`.)

## Play

```
devhunter.exe
```

| key | action |
|-----|--------|
| → (right) | accelerate |
| ← (left)  | brake |
| ↑ / ↓     | steer |
| Space     | fire |
| `O`       | 3-shot spread |
| `H`       | toggle dev-name overlay density |
| Esc / F12 | quit |

Shoot the oncoming developer cars for points. As in the original there is **no
car-to-car collision** — bumping other cars is harmless; you hunt them by
shooting. The only way to crash is driving into a **lane-end barrier** (a raised
concrete wall where a lane ends): that spins you out (explosion + full stop +
brief recovery). There are no lives and no game-over screen — you recover and
keep hunting.

## Headless screenshot

Runs N frames (with simulated driving input) and writes a binary PPM — handy for
regression/visual checks without a display:

```
devhunter.exe --shot 120 out.ppm
```

## Files

| file | role |
|------|------|
| `main.c` | window, message loop, input, 8-bit→palette→DIB present, `--shot` |
| `game.c` | RNG, dev-name road strip, physics, spawning/collision, pseudo-3D renderer |
| `data.h` | generated: exact palette, decrypted text, data tables (from `MSOWC.DLL`) |
| `dh.h`   | shared declarations / game state |
| `REVERSE_ENGINEERING.md`, `re/` | full RE notes and raw dumps |

## Credits

Microsoft — Excel 2000 / MSOWC / the original Dev Hunter and its developers
(whose names you are hunting). This reimplementation is a fan/RE project.

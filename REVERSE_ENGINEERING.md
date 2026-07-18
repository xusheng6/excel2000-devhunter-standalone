# Dev Hunter (Excel 2000 easter egg) — Reverse Engineering & Port Notes

Dev Hunter is a pseudo‑3D car‑shooter hidden in `MSOWC.DLL` (the Microsoft
Office Web Components control, CLSID `0002E510-0000-0000-C000-000000000046`).
This document records the reverse engineering behind the standalone,
byte‑exact C reimplementation in this directory.

All addresses use the Binary Ninja image base **`0x3c6d0000`**.
`this`‑relative offsets (e.g. `+0xa0`) are given as used in the disassembly and
map 1:1 onto the `dh.h` accessor macros (`f32(o)`, `i32(o)`, `u8(o)`, …).

Reference dumps live in `re/`:
* `re/hlil.txt`  — HLIL of ~76 game functions (roots + call graph, depth 4)
* `re/asm.txt`   — x86 disassembly + stack layouts of the three big functions
* `re/extra_funcs.txt` — input wndproc, palette/display setup, ddraw helpers

## 1. Entry points / control flow

| addr | offset | name in port | role |
|------|--------|--------------|------|
| `0x3c7ee86a` | `0x11e86a` | (trigger) | `func3` — checks no Ctrl/Alt/Shift, cell WC1999 (`col 0x2bd`, `row 0x7cf`) value `0x258`; allocs `0x9a96c` state, calls func2 |
| `0x3c7dc946` | `0x10c946` | `main.c` loop | `func2` — the loader entry; window + PeekMessage/Translate/Dispatch loop; idle → `game_tick` |
| `0x3c7dc79b` | `0x10c79b` | `main.c` / `setmode_palette` | `func1` — loads ddraw+winmm, registers class (wndproc `0x3c7dfc6b`), CreateWindow 640×480, ddraw surfaces, **palette** |
| `0x3c7df259` | `0x10f259` | `game_init` | one‑time init: decrypt text, procedural terrain, GDI letter atlas |
| `0x3c7dcb73` | `0x10cb73` | `game_tick` | per‑frame: physics, entities, road scroll, collision; calls render |
| `0x3c7de915` | `0x11e915` | `game_render` | per‑frame perspective renderer + surface flip |

The loader (func2) hands func1 a `this` pointer (`ecx`, a zeroed ≥`0x9a96c`
buffer), `arg2 = hMSOWC`, `arg3 = 0`, `arg4 = 0x7fa87860`. `arg4` is the packed
**player‑car colour** `RGB(0x60,0x78,0xa8)` (little‑endian bytes `60 78 a8 7f`);
`sub_3c79353d` reconstructs exactly this value as the fallback, confirming it.

## 2. Video model

640×480, **8‑bit palettized**. The renderer writes palette indices into the
DirectDraw back‑surface; a 256‑entry palette maps them to RGB. The port keeps
this exactly: an 8‑bit framebuffer `g_fb` (`i32(0x34)` = base, `i32(0x20)` =
pitch = 640) is converted through the exact palette into a 32‑bit DIB and
`BitBlt`ed.

### Palette (`sub_3c7dc3a0`, reproduced in `gen_data.py` → `data.h`)
256 `PALETTEENTRY` built as 11 ramps (R = `[-1]`, G = `[0]`, B = `[+1]`, stride 4):

| idx | contents |
|-----|----------|
| 0–63   | grayscale `c=i*4` (road light/shade) |
| 64–95  | `R=(i+30)*2, G=(i+32)*2, B=i+1` (sky) |
| 96–127 | `R=G=i*8, B=i*5` |
| 128–143| `R=255, G=i*16, B=0` (fire) |
| 144–159| **player‑car ramp** from `(R,G,B)*16/32`…`*31/32`, using `0x60,0x78,0xa8` |
| 160–175| `R=G=(i+16)*5, B=(i+16)*3` |
| 176–191| `R=(i+16)*5, G=B=0` |
| 192–207| `R=i+16, G=(i+16)*3, B=(i+16)*2` |
| 208–223| `R=G=i+16, B=(i+16)*4` (blue) |
| 224–239| `R=(i+16)*3, G=i+16, B=(i+16)*4` (purple) |
| 240–255| `R=G=B=(i+16)*5` (white ramp) |

## 3. Input — wndproc `0x3c7dfc6b`

Keydown sets, keyup clears these state flags (read by `game_tick`):

| key | field | meaning |
|-----|-------|---------|
| →  `VK_RIGHT` | `i32(0xa4)` | gas (accelerate) |
| ←  `VK_LEFT`  | `i32(0xa8)` | brake/reverse |
| ↑  `VK_UP`    | `i32(0xac)` | steer |
| ↓  `VK_DOWN`  | `i32(0xb0)` | steer |
| Space | `i32(0x9a6b0)` | fire bullets |
| `O` | `i32(0x9a6ac)` | secondary weapon (6‑shot spread) |
| `H` | `i32(0x9a6a8)` ^= 1 | toggle projected dev‑name overlay |
| Esc / F12 | — | quit (PostMessage WM_CLOSE) |

## 4. Helper / leaf functions (translated in `helpers.c`)

* `sub_3c6db234(x)` → `(int)x` truncation toward zero (classic `_ftol`; sets FPU RC=truncate). Modeled by `FTOL`.
* `sub_3c8d36e0()` → float LCG: `seed = frac(seed*9821 + 0.211327)`, seed₀ = 0.0 (`data_3c924290`). `dh_rand`.
* `sub_3c7897e4(n)` → `abs(int)`; `sub_3c8f74d4` → `fabs`.
* `sub_3c7a9651`/`sub_3c75a432` → `sin`/`cos` (x87 `__fsin`/`__fcos`).
* `sub_3c7de3b7` → spawn 66 explosion particles (`spawn_debris`).
* `sub_3c7dca79` → bilinear splat into a 64×64 int grid at `pp(0x910f4)` (`sprite_accum`).
* `sub_3c7de66f` → textured/shaded horizontal‑span filler (`raster_span`); samples per‑cell texture `pp(0x850e8)` through the 65536‑entry colour/shade LUT at `+0x910f8`.
* `sub_3c7de4a2` → triangle setup: y‑sort 3 verts at `+0x9a948..+0x9a968`, emit ≤2 spans (`raster_tri`).

## 5. State‑struct offset map (identified so far)

Player / camera:
```
+0xa0  float  car "distance"/odometer accumulator (init 48.4)
+0xa4  int    input: gas          +0xa8 int input: brake
+0xac  int    input: steer +      +0xb0 int input: steer -
+0xb4  int    last timeGetTime (frame timer)
+0xb8  float  throttle/accel state +0xbc float steering state
+0xc0  float  forward speed        +0xc4 float lateral pos (init 82.4)
+0xc8  ptr    text stream A cursor  +0xcc ptr text stream B cursor
+0xd0  int    which stream (0/1)    +0xd4 int toggle   +0xd8 int column
+0xdc  int    scroll row (mod 0x40) +0xe0 int frame/row counter
```
Entities (arrays, base + stride):
```
+0x9945c enemies   ×7   stride 0x28 (10 floats): active/x/y/vx/vy/... (+0x99474 = active)
+0x99578 bullets   ×20  stride 0x14 (5 floats)   (+0x99580 = active); write idx +0x99700
+0x99704 debris    ×200 stride 0x14 (5 floats)   (+0x99714 = life);   write idx +0x9a6a4
```
Grids (all indexed `[row*0x40 + col]`, dword each):
```
+0x850ec  elevation/height accumulator (64×64)         used by render as terrain Z
+0x8d0ec  road tile texture pointers  (64×64)          each entry = ptr to a cell texture
+0x890ec  glow/light accumulation grid (64×64)         sprite_accum target (via 0x910f4)
+0x10e8/+0x20e8/+0x30e8/…  cell texture prototypes (plain road, lane paint, letters, …)
+0x910f8  colour/shade LUT  256×256 ints (texture value + light → palette index)
+0x510e8  letter tiles (3D‑extruded A–Z), built from the GDI atlas
```
Road / spawn control:
```
+0x9a6a4 debris write index         +0x9a6a8 name overlay toggle (H)
+0x9a6ac O‑weapon flag              +0x9a6b0 fire flag (space)
+0x9a6b4/b8/bc float fire timers    +0x9a6c0 float boost/recover timer
+0x9a6c4 int spawn countdown (init 0x50)
+0x9a6cc ×6 int  lane occupancy (current)   +0x9a6ec ×6 int (target)
+0x9a7ec..0x9a7f9  byte spawn parameter table
+0x9a7fc byte current enemy pattern index   +0x9a7e8 int spawn phase
+0x9a800/804 HMODULE ddraw/winmm  +0x9a914 atom  +0x9a920 HFONT
+0x9a924/928/92c/930 IDirectDraw / surfaces / palette
```
Text buffers (allocated by func2, pointers stored):
```
+0x910ec ptr → buf1 (0x72c bytes)  — intro narration + full credits
+0x910f0 ptr → buf2 (0x217 bytes)  — taunt stream
```

## 6. Text decryption (in `game_init` / `data.h`)

Two CBC‑XOR streams. `buf[0]` seed then `buf[i] = key[i] ^ buf[i-1]`:
* buf1: key @ `0x3c7ac62c`, len `0x72c`, seed `0x73`.
* buf2: key @ `0x3c7acd58`, len `0x217`, seed `0x2d`.

Decrypted (already baked into `data.h` as `g_text1`/`g_text2`). Word separator
`-`; control bytes: `#` = restart credits (jump to buf1+0xfa), `/` = end/switch
to buf2, `!` = toggle stream. buf1 contains the intro ("you are dev hunter … the
highways of seattle … hunt … and destroy them") and the complete Excel 2000
credits (developers, testers, managers, guest appearances, stunts). buf2 the
taunts ("respect the rectangles", "super funk", "pivot‑pivot‑pivot", …).

## 7. Road / rendering math (`game_render` sub_3c7de915)

Perspective projection, camera pitched **10°**:
`cos10 = 0.98480775`, `sin10 = 0.17364817`; focal `864`, screen centre
`(0x140,0xce) = (320,206)`, height 480. For each of a 64×64 mesh of road cells
it interpolates screen coords into a 65×65 grid (`var_154/var_158`), then fills
each cell as two gouraud/textured triangles (`raster_tri` → `raster_span`),
sampling the cell's texture pointer (`+0x8d0ec[i]`) shaded by the elevation grid
(`+0x850ec[i]`). Cars/bullets/debris are billboarded: world→screen through the
same projection, drawn as sprite polys (car vertex tables at `0x3c7acf72`,
stride `0x30`, 16 `(colour,x,y)` triples; 5 colour variants) or single pixels.

## 7a. Car models — geometry & colour

**Geometry.** Each car is a small 3-D model: a table of **16 vertices × 3 signed
bytes** at `0x3c7acf72` (`0x30` bytes/model). The bytes are `(z, x, y)` — z ≈
`-16` (flat, near the ground), x/y span a ~`0..0x1e` grid. Only *half* the car is
stored; `game_render`'s car loop projects each vertex to both `+x` and `-x`
(mirror-symmetric). Several tables follow (`+0x30` each) = **turn/pose frames**,
selected by the car's steering-animation field `+0x9947c` (`var_20_3`). A fixed
**connectivity table** at `0x3c7ad062` (7-byte records, until `0x3c7ad08c`)
stitches the vertices into triangles; each record's 5th byte is a **material id
0/1/2** → body / windows / roof-trim. Projection uses the 10° camera / focal-864
math (§7); triangles are filled by `sub_3c7de4a2`→`sub_3c7de66f`.

**Colour.** Each car has a **colour-scheme index** in `+0x99478` (enemies: random
1..6 at spawn, set in `game_tick`). That index `<< 0xc` (× `0x1000`) selects a
per-scheme texture bank; the three material colours live at `+0x70e8` / `+0xe0e8`
/ `+0x150e8` inside it. The **player** is special-cased to bank 0, whose colours
derive from the `arg4 = 0x7fa87860` param → `RGB(0x60,0x78,0xa8)` (blue). The six
schemes are built in `game_init` from colour-prototype fills (`+0xe8`,`+0x30e8`…
`+0x60e8`).

## 7b. Collision / losing (important)

There is **no player-vs-car collision** — enemy cars are billboard sprites; the
*only* collision is the player's road-grid cell against a **barrier cell** (type
`arg1+0x20e8`, the raised road edge / a lane that has ended). In `game_tick`
(~`0x3c7dd...`): if the player's cell is a barrier **and** the recovery timer
`+0x9a6c0` has expired, it fires `sub_3c7de3b7` (explosion), zeroes throttle
`+0xb8` and steering `+0xbc`, and sets `+0x9a6c0 = 20.0` (recovery). **No lives,
no game-over screen** — a spin-out you recover from. Enemies hitting a barrier
explode the same way (visible in recording2 as a car runs off the narrowing
edge). The standalone models this with `Wall` barriers + a `recover` timer.

## 8. Port status

The delivered port (`main.c` + `game.c`) is a **faithful, playable
reconstruction** ("close enough", per the brief) rather than a byte‑exact
translation of the DLL's rasterizer. The renderer was tuned against the original
gameplay recordings to match the look: the high near‑top‑down camera, wide
mildly‑tapering multi‑lane road, drifting soft terrain shadows, mottled grass,
and developer names painted on the tarmac (white names / yellow section
headers). It reuses the material recovered exactly from the binary and rebuilds
the gameplay/look on top of it:

* Exact 256‑colour palette + 8‑bit‑framebuffer→DIB present path.
* Decrypted developer‑name / credits text, laid onto the road as scrolling
  decals (built via GDI in `game.c:build_strip`).
* The game's float LCG (`dh_rand`) and a flat‑road projection in the spirit of
  the original's 10° camera / focal‑864 (see the constants in `game.c`).
* Player physics, enemy spawning/AI, bullets, debris, collision and scoring.
* Input wndproc mapping; ESC/F12 quit; HUD; headless `--shot N out.ppm` dumper.

### If a byte‑exact port is ever wanted
Everything needed is captured here and in `re/`. The three heavy functions would
be translated from `re/asm.txt` (not HLIL):
1. `game_init` terrain generator — noise array `var_88c` at **`ebp‑0x88c`**
   (32×32 `int16`), filled `0x15 - (int)(rand()*-8)`, box‑smoothed
   (`(L+R+6*C+up+down)*10/100`) into the elevation grid, plus lane colouring and
   the GDI letter atlas (A–Z in Tahoma 64 bold → 256×256 → extruded 3D tiles).
   Use the raw asm offsets — HLIL renders `var_88c` as ambiguous `ebp`‑relative
   arithmetic.
2. `game_tick` physics/entities (x87 float, self‑contained).
3. `game_render` projection + the gouraud/textured rasterizers `sub_3c7de66f`
   / `sub_3c7de4a2` (HLIL in `re/hlil.txt`; §4/§7 give the math and LUT layout).

## Build & run
```
build.bat            # 32-bit cl.exe -> devhunter.exe
devhunter.exe        # play (arrows steer/gas, space fires, Esc quits)
devhunter.exe --shot 60 out.ppm   # headless: run 60 frames, dump a PPM
```

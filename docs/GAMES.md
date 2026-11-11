# Games and performance

What has been run with this driver, newest driver version first where several were tested.
FPS as seen on screen or from the driver's profiler. Tested on the Huawei P40 (Mali-G76 MC16)
unless the row names another phone; see the [tested GPUs](../README.md#tested-gpus) on the
main page. Tested something else? Open an issue with the app, game, driver version and what
you saw, ideally with a [`panvk-report`](REPORTING.md) zip.

## Results

| App | Game / test | API | Version | Result |
|---|---|---|---|---|
| Winlator | AIO Graphics Test, D3D11 cube | D3D11 (DXVK) | 1.0.0 → | ~260–270 FPS (Sarek 1.11, 1.10.3, 2.3.1, x86_64 container); 1.2.4, ARM64EC container: 1.10.3-async 380–550 FPS; A51 (G72) 213 FPS |
| Winlator | AIO Graphics Test, D3D11 cube | D3D11 (DXVK 2.7.1 gplasync) | 1.2.4 | **180–230 FPS** (1.2.3: black window, device lost; first 1.2.4 build: 104 FPS). The GPU governor keeps the clock near 260 MHz for this load, 1.10.3 gets ~580 MHz |
| Winlator | AIO Graphics Test, D3D9 cube | D3D9 (DXVK) | 1.0.0 → | ~74 FPS with Box64 Performance (5.5 FPS with Compatibility); A51 60 FPS |
| Winlator | AIO Graphics Test, OpenGL cube | OpenGL (Zink) | 1.2.6 | **104 FPS**, no glitches (before 1.2.6 OpenGL was unavailable on Android 10; needs `BOX64_LIBGL`, see the [Winlator guide](apps/winlator.md#opengl-zink-on-android-10)) |
| Winlator | AIO Graphics Test, D3D12 cube | D3D12 (VKD3D-Proton 2.14.1) | 1.2.2 | cube shows |
| Winlator | `d3d12diag` (14 draw/readback cases, presenting) | D3D12 | 1.2.2 | all pass |
| Winlator | NieR:Automata | D3D11 (DXVK-Sarek 1.12) | 1.2.2 | **playable but slow**: ~20 FPS menus, ~10–14 FPS in game (GPU-bound vertex shading); needs `heap_memory_percent=0.42` |
| Winlator | NieR:Automata | D3D11 (DXVK 1.10.3-async) | 1.2.4 | menu 47 FPS; one save fails to load (also on 1.2.3: Sarek gets stuck, 1.10.3 crashes in game code) |
| Winlator | NieR:Automata | D3D11 (DXVK 2.7.1 gplasync) | 1.2.4 | intro and menu correct, one save loads and renders, **another save goes black** (GPU fault in its first heavy pass, open). Needs BCn emulation **off** |
| Winlator | Assassin's Creed | D3D9 (DXVK 1.10.3-async, ARM64EC) | 1.2.8 | runs; flickering textures before 1.2.8 (overlap) |
| Winlator | Assassin's Creed IV: Black Flag | D3D11 (DXVK) | 1.2.8 | GPU hang fixed in 1.2.8; then stops at the intro video (Wine: GStreamer video conversion missing) |
| Winlator | Ori and the Blind Forest DE | D3D11 (DXVK 3.1.1, ARM64EC) | 1.2.7 | **works**; with DXVK 1.10.3-async it glitches and doesn't reach the game |
| Winlator | Need for Speed: Most Wanted (low) | D3D9 | 1.2.0 | runs; ~22 FPS on 1.1.0, per-draw cost cut 2.4× in 1.2.0 |
| Winlator | Mortal Kombat Komplete Edition | D3D9, 32-bit | 1.2.2 | **crashes** (R6025 / page fault in the game under Wine WoW64; no driver errors); ~13 FPS before that |
| Winlator | DmC: Devil May Cry | D3D9, 32-bit | 1.2.2 | **crashes** at start (R6025, same WoW64 issue) |
| Winlator | Renderer Driver = PanVK (compositor) | Vulkan | 1.2.1 | works, 60 Hz |
| Eden | NieR:Automata – The End of YoRHa Edition | Vulkan | 1.2.3 | **runs, no crashes** (crashed on 1.2.2) |
| ARMSX2 | Kingdom Hearts | Vulkan | 1.2.5 | **27–30 FPS** in game (1.2.4: 5–8; Huawei's driver 35–41) |
| Eden | Zelda: Echoes of Wisdom | Vulkan | 1.2.3, 1.2.4 | **playable**: 16–20 FPS with Huawei performance mode (8–12 without), some stutter; 2D menu smooth |
| – | Vulkan CTS (dEQP-VK 1.4.7.0) | Vulkan 1.3 | 1.1.0, 1.2.4, 1.2.5 | 94 k cases pass in a 1/10 sample of the suite (1.1.0); 1.2.4: no new failures on 37 600 cases (regression sample, storage-buffer robustness, sync2, subgroups, buffer-to-image copies, 6 890 indirect-draw and 3 983 vertex-shader image cases; one image-robustness case failed once and passed 3/3 on its own). [docs/CTS.md](CTS.md) |

Per-game settings and details: [Winlator](apps/winlator.md#notes-per-game),
[Eden](apps/eden.md), [ARMSX2](apps/armsx2.md). Tested something else? Open an issue with the app, game,
driver version and what you saw.

## Known issues

- **NieR:Automata with DXVK 2.7.1**: one save goes black (GPU fault in its first heavy pass,
  ~850 draws; cause not found; 1.2.4's draw changes are not it). Use DXVK-Sarek 1.12 or
  1.10.3 for NieR. Set Winlator's **BCn emulation to None** for PanVK containers.
- **DXVK 2.7 is ~2× slower than DXVK 1.10 / Sarek** in the AIO test (180–230 vs 380–550 FPS):
  more GPU work per frame (indirect draws, vertex pulling, more passes and queries), and the
  Huawei governor clocks the GPU at ~260 MHz for it instead of ~580 MHz.

- **NieR:Automata uses a lot of RAM** (1.15 GB DXVK + up to ~450 MiB driver in heavy scenes on
  1.2.7) and shows occasional fragment-job address-range faults (open).
- **ARMSX2: Kingdom Hearts 27–30 FPS vs 35–41 on Huawei's driver.** PCSX2 waits for the GPU
  twice a frame (three command buffers in flight); our GPU time per pass is higher, mostly on
  the fragment side. Not the clock (Huawei's driver reaches 38 FPS at 166 MHz), not memory types,
  not the driver's CPU time.
- **Overlap** (on for PCSX2-based emulators only since 1.2.8) misses a dependency in Assassin's
  Creed (D3D9) that makes textures flicker; not yet found.
- **Eden: Zelda: Echoes of Wisdom is GPU-bound** (16–20 FPS): the emulator issues ~85 render
  passes per frame, three quarters with one draw, and each pass loads and stores its tiles.
  Passes on the same target can't be merged (they are feedback loops: the next pass samples
  what the previous one drew). Folding clear-only passes into the next pass might gain 5–10 %.
- A 256-invocation compute shader with a barrier inside control flow still runs unsplit
  (the split needs top-level barriers); no game is known to break on it.
- **NieR:Automata in game is GPU-bound** (~10–14 FPS at the ocean flight, 800×600). It
  sends ~2.7 M triangles per frame of which ~2 % are visible; the tiler waits on position
  shading for 94 % of its active time, vertex by vertex, so position-shader latency limits
  it (register count and thread occupancy made no difference when measured). Huawei's
  governor also keeps the G76 at 208–509 MHz instead of 600 MHz (not changeable without root).
- **NieR needs `heap_memory_percent=0.42`** (or so) in `panvk_env.txt`: with 0.35 DXVK caps
  it at 2 080 MB and it loops forever at a loading point. Higher values risk Android's
  low-memory killer.
- No sparse resources → VKD3D-Proton reports FL 11_0. Games that require 12_0/12_1 start
  with `VKD3D_FEATURE_LEVEL=12_0` (or `12_1`) in the container's environment, as long as
  they don't actually use tiled resources.
- Open CTS failures (details in [docs/CTS.md](CTS.md)): GPU hangs in tessellation
  with storage images in the evaluation shader and in some adjacency-primitive draws
  (geometry/tessellation emulation); update-after-bind descriptors with storage images;
  per-sample shading with several render targets plus depth resolve; texture offsets
  with MIRRORED_REPEAT (Bifrost hardware, also failing upstream); Huawei AFBC
  hardware buffers can't be imported (Winlator's are linear, not affected).

Full investigation log: [docs/FINDINGS.md](FINDINGS.md).

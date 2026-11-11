# Driver options (`panvk_env.txt`)

The file is `/sdcard/Download/panvk_env.txt`; when the app can't read it,
`/sdcard/Android/data/<package>/files/panvk_env.txt`. `KEY=VALUE` lines (only `PANVK_*`, `PAN_*`, `MESA_*`, `BIFROST_*` and `heap_memory_percent`) are put in the environment when
the driver loads; a leading `!` overrides a variable the app already set.

| Option | Effect |
|---|---|
| `PANVK_GPU_PROF=1` | logcat every 5 s: GPU time per kind of batch, CPU wait time and the app function that waited, submit rate, submit→done latency, batches/s and why they were split, shader compiles over 5 ms; on the first failed job the last 128 jobs (`HIST`) |
| `PANVK_GPU_TS=1` (with `PANVK_GPU_PROF=1`) | GPU timestamps around every vertex/tiler chain: the GPU's own time per kind of batch (adds a little GPU work) |
| `PANVK_SELFPROF=1` | logcat every 5 s: where the app's threads spend CPU time (per thread, library and function) |
| `PANVK_SPLIT_NOOP_BARRIERS=1` | A/B: no-op barriers end the batch again (before 1.2.3) |
| `PANVK_G76_DIAG=1` | log whether the imported swapchain buffers contain non-black pixels |
| `PANVK_HIDE_EXTS=VK_EXT_a,VK_KHR_b` | hide device extensions |
| `MESA_SPIRV_DUMP_PATH=/sdcard/Download/spv` + `!MESA_SHADER_CACHE_DISABLE=true` | dump every SPIR-V shader (named by hash; failed jobs log the hash) |
| `PANVK_ERRORS_FILE=0` | don't write GPU errors to `panvk_errors.txt` |
| `PANVK_V7_VARY_TRIM=0` / `PANVK_JM_DEV_WLS=0` | keep varying memory after heavy scenes / one workgroup-memory buffer per command buffer (before 1.2.7) |
| `PANVK_OVERLAP=1` / `=0` | force the overlap of a pass's vertex/tiler work with the previous fragment work on/off (default: on for PCSX2-based emulators only, from 1.2.8) |
| `PANVK_V7_PUSH_UBO=1` / `=0` | force uniform-buffer constants into registers on/off (default: on for PCSX2-based apps) |
| `PANVK_V7_CPU_INDEX_RANGE=1` / `PANVK_V7_DIRECT_TEX_ALL=1` | experiments, off by default: index ranges found on the CPU (wrong if indices are written after the draw is recorded) / textures indexed straight from descriptor memory |
| `PANVK_GRALLOC_GUESS_LINEAR=0` | refuse buffers without mapper metadata (old behaviour) |
| `PANVK_JM_EXPORT_NOWAIT=1` | A/B only: sync-file export without waiting |
| `heap_memory_percent=0.42` | report a smaller device heap, so DXVK keeps less memory (NieR: 0.42; at 0.35 it can't load the next area) |
| `BIFROST_NO_CSEL_FUSE=1` / `BIFROST_NO_HOIST=1` | A/B: no compare+select fusion / vertex shader loads where nir_opt_move puts them |
| `PANVK_V7_IDVS=0` / `PANVK_V7_BATCH_MINMAX=0` / `PANVK_V7_HOIST=0` | A/B: no IDVS / one helper job per draw / helpers in place |
| `PANVK_V7_NO_SSBO_VEC=1` / `PANVK_V7_BRANCHY_SSBO=1` | A/B: no 16-byte merge of storage-buffer loads / guarded loads |
| `PANVK_SSBO_VEC_LOG=1` | log per shader how many storage-buffer loads were merged |
| `BIFROST_MESA_DEBUG=shaders` + `PANVK_STDERR_FILE=/sdcard/Download/x.txt` | dump compiled shaders from a Winlator game (in ARMSX2 use a path in its `files/` directory; add `MESA_SHADER_CACHE_DISABLE=true`) |

Failed GPU jobs are always logged (`panvk: failed atom N ran: …`).

An example file: [panvk_env.example.txt](panvk_env.example.txt).

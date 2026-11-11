# Patch series

The fixes on top of the zenithblue series, as `patches/00*.patch` (from 1.3.0 on Mesa 27;
the 1.2.x series is in `patches/mesa-26.3/`). Bold rows are the ones users notice most.

| Patch | Problem → fix |
|---|---|
| 0001 | Kernel uAPI 11.16 uses a 32-byte `MEM_IMPORT`; the driver only knew 24/40 bytes → swapchain/AHB imports failed. Retry with the old layout. |
| 0002 | Huawei gralloc has no mapper4 metadata → read width/stride/AFBC flag from Huawei's `private_handle_t`. |
| 0003 | On JM the sync-file export was disabled, so the Vulkan runtime had no SYNC_FD semaphores and Winlator's wrapper hung in `vkAcquireNextImageKHR`. Export now waits for the payload and returns a signaled (-1) fence. |
| 0004 | "Overlap" mode split the tiler heap into two 64 MiB halves → tiler out-of-memory on big frames on the G76. Off by default on Bifrost. |
| 0007 | Swapchain buffers from Huawei gralloc were AFBC (unusable) → request CPU-access usage so gralloc allocates them linear. |
| 0010 | Samsung (A51, AOSP 16, HIDL mapper 2.x only): import buffers as linear when no mapper metadata exists. |
| **0012** | **NieR:Automata hang:** a D3D11 compute shader with 32×30 = 960 invocations per workgroup; the G76 holds at most 384 → the barrier deadlocked and the kernel killed the job (`event=0x4`, DEVICE_LOST). New compiler pass: if all barriers are top-level, the workgroup runs as K copies per hardware invocation (960 → 5 × 192), regions between barriers are emitted once per logical invocation. |
| 0019, 0020 | **HOST_COHERENT memory not coherent** when `memoryMapPlaced` is enabled (`VK_EXT_map_memory_placed`, what Wine WoW64 uses for 32-bit games): every host-visible allocation became an ION buffer allocated CPU-cached, and the G76 does not snoop CPU caches. Now write-combined for coherent memory; cached buffers use `DMA_BUF_IOCTL_SYNC` for flush/invalidate and are cleaned after the kernel's zero fill. Found by CTS: smoke 2/8 → 8/8. |
| 0021 | Global priority LOW was refused (only MEDIUM accepted). |
| 0022 | `VK_KHR_maintenance7` update-after-bind dynamic-buffer totals were 0 on Bifrost. |
| 0023 | `VK_EXT_multisampled_render_to_single_sampled` not advertised on JM: every supported CTS case failed; DXVK/VKD3D-Proton don't use it. |
| 0024 | DRM-modifier image creation crashed (NULL modifier handler) when the application's modifier list was AFBC-only and the device had `customBorderColorWithoutFormat` on. |
| 0025 | **GPU hang with images in vertex shaders**: the descriptor copy job got a byte offset where it adds a buffer index; the vertex job read past its attribute table. Also present in upstream Mesa. |
| 0026 | Null image descriptors: size/level/sample queries returned 1 instead of 0; format-less storage-image loads returned non-zero. |
| 0027 | Debugging knobs used for the CTS work (`PANVK_NO_LWG`, `PANVK_TRACE_JC`, `PANVK_IMPORT_LOG`). |
| 0028 | IDVS (index-driven vertex shading) on the G76, as the vendor driver does: NieR's vertex/tiler batches about halved. `PANVK_V7_IDVS=0` turns it off. |
| 0029, 0030 | One device-wide shader stack buffer instead of one per command buffer (NieR: driver memory 1 550 → 1 160 MiB; Android's low-memory killer had been closing the game); `heap_memory_percent` from `panvk_env.txt`. |
| 0031, 0033 | GPU profiler lines (memory, draws, shaders); `BIFROST_*` keys and `PANVK_STDERR_FILE` in `panvk_env.txt` for shader dumps. |
| 0032 | Index-range helper jobs of indexed draws moved to the head of the batch behind one barrier, instead of a prefetch stall per draw. |
| **0034** | **Robust storage-buffer loads** (DXVK's robustBufferAccess2) were `if`-guarded 32-bit loads, one per clause: NieR's skinning shaders ran at 0.04 instructions/cycle. Now straight-line, and consecutive 32-bit loads of one buffer become one 16-byte load with per-component bounds checks. NieR heavy batches 188/110 → 117/72 ms. |
| **0035** | **One index-range helper job per batch** instead of one per indexed draw (~35 µs of job-manager time each). NFS: Most Wanted 11 500 → 27 300 indexed draws/s. |
| **0036** | **Vertex shaders could read stale descriptor tables** on draws without IDVS since 0032 (63 CTS vertex-shader cases, wrong rendering / lost device): draws now wait for their descriptor copy job. |
| 0037 | Batched index-range helpers also take draws of up to 65 536 indices (one workgroup each) instead of three jobs per draw: 200 × 17 400 indices 158 → 66 ms (vendor 65 ms). |
| 0038 | Robust-buffer bounds checks with one saturating subtract per 16-byte load and one compare per component (were two compares, an add and two ANDs per component): NieR skinning shaders 9–15 % faster per vertex. |
| **0039** | **Winlator Renderer Driver = PanVK: black screen.** `vkQueueSignalReleaseImageANDROID` failed on every present (−13): PanVK copied semaphore payloads with a DRM sync-object transfer the kbase backend doesn't emulate. Off on kbase; the runtime does an empty submit instead. |
| 0040 | Integer compare + select fused into one `CSEL` (was ICMP + MUX): 14–17 % fewer instructions in NieR's position shaders, 5–11 % faster per vertex. `BIFROST_NO_CSEL_FUSE=1` turns it off. |
| 0041 | Vertex shaders issue storage-buffer loads up to three ahead of their use, with their address arithmetic, instead of right before it: NieR's skinning shaders 5–11 % faster per vertex (the IDVS tiler waits on each vertex's position shader). `BIFROST_NO_HOIST=1` turns it off. |
| 0042 | **Eden crash in `vkCmdClearAttachments`**: push-descriptor storage was sized in API descriptors, but a combined image sampler takes up to 5 hardware descriptors → the meta clear overwrote memory. Sized in hardware descriptors now. |
| 0043 | Barriers with nothing to wait for (destination stage only TOP_OF_PIPE, or source only BOTTOM_OF_PIPE) no longer end the batch. `panvk_env.txt` is also read from `/sdcard/Android/data/<package>/files/`. Profiler: why batches were split, slow shader compiles. |
| **0044** | **Eden NieR:Automata crash**: the large-workgroup split (0012) refused shaders with subgroup operations, so 1024-invocation workgroups ran unsplit on the G76 (wrong results, then a GPU fault). `gl_SubgroupID` / `gl_NumSubgroups` are now rewritten per logical invocation and such shaders are split. |
| 0045 | Profiler: on the first failed GPU job, log the last 128 jobs (kind, duration, result). |
| 0046 | Bindless (update-after-bind) textures and samplers indexed set memory from one device-constant base with a 32-bit index; in Wine/FEX processes descriptor pools land more than 64 GiB away. The base is now per draw/dispatch, from the bound sets. `PANVK_VA_HOLE_GB=n` reproduces the placement in tests. |
| **0047** | **DXVK 2.4+ black window / lost device**: an indexed draw without vertex buffers (DXVK 2.x pulls vertices from a texel buffer) made a zero-sized allocation, read as out of memory, and the VS image attribute table stayed NULL. |
| 0048 | Job decoder: bindless texture/sampler tables are not decoded. |
| **0049** | **Indexed indirect draws** (DXVK 2.7 records D3D11 draws this way) got a WRITE_VALUE, a 64-workgroup index search sized by the whole index buffer and a helper job: now one fused job, batched per batch. AIO D3D11 with DXVK 2.7.1: 104 → ~190 FPS. |
| 0050 | Non-indexed indirect draws join the batch's helper job: 0.22–0.27 → 0.05 ms per draw on the benchmark. |
| 0051 | Profiler: every draw counted, render pass size, attachments and load ops in the batch kind. |
| 0052 | A vertex-pulling VS with storage images or texel buffers reuses its image tables while the VS and descriptors are unchanged (no copy job per draw). |
| **0053** | **Buffer-to-image copies that read past their source** are clipped to the rows that exist: the copy shader has no source bounds and the memory after a buffer is often unmapped on kbase (lost device). Hit by Winlator's BCn emulation with DXVK 2.7. |
| 0054 | Job decoder: flags attribute buffers outside known memory, prints push uniforms. |
| 0055 | Compiler: drivers can push their own uniform buffers into registers without gallium's blend-constant slot. |
| **0056** | **ARMSX2 (PCSX2):** rasterization-order attachment access on v6/v7 (PCSX2 no longer splits every pass for a render-target read-back; Kingdom Hearts 5–8 → 27–30 FPS); tile-local in-pass barriers don't split; overlap on by default with 128 MiB heap halves, barriers from fragment to vertex/compute work wait for the fragment job; dynamic uniform-buffer words pushed into registers for PCSX2-based apps; `panvk_env.txt` read without `fopen()` (AdrenoTools redirects it); opt-in CPU index ranges and direct textures; GPU-timestamp and wait-site profiling. |
| 0057 | The driver logs the directory it was loaded from (for `BOX64_LIBGL`, Winlator OpenGL on Android 10). |
| 0058 | kbase: a growable buffer's committed size can be queried and reduced; atoms in flight are counted without blocking. |
| 0059 | The opt-in CPU index range skips index buffers the GPU can write. |
| **0060** | **Less RAM:** workgroup memory shared across command buffers (NieR −103 MiB); varying memory given back when the GPU is idle (`PANVK_V7_VARY_TRIM=0` turns it off). |
| **0061** | **Large compute workgroups:** module-level variables (DXVK's registers) are per logical invocation too; fixes the GPU hang in Assassin's Creed IV. |
| 0062 | Overlap on by default only for PCSX2-based emulators (Assassin's Creed flickered with it). |
| 0063 | GPU errors (failed jobs, the jobs before them) written to `Download/panvk_errors.txt` for bug reports; collected by `tools/panvk-report`. |
| 0064 | Build fixes after the Mesa 27 merge (kbase backend on the new `pan_kmod` interface, BO labels, compile inputs). |
| 0065 | JM keeps its own indirect varying buffer; Mesa 27's `poly_heap` is not used on Bifrost. |
| **0066** | **32-bit games on legacy-ION kernels (MediaTek Helio G80):** placed maps use the legacy ION interface when the new one is missing. |
| 0067 | A failed memory map writes its reason to `panvk_errors.txt`. |
| **0068** | **Renderer on MediaTek:** import the native handle's dma-buf, not MediaTek's `gralloc_extra` metadata fd. |
| **0069** | **Renderer on Android 16 (Samsung):** linear swapchain / AHB images, so gralloc never picks AFBC. |
| 0005, 0006, 0008, 0009, 0011, 0013–0018 | Diagnostics (off unless enabled in `panvk_env.txt`): swapchain pixel check, extension hiding, failed-job batch description, GPU-time profiler, wait/latency statistics. |

Not in this series but required for Winlator: Android-only WSI (`-Dplatforms=android`).
With the X11 WSI compiled in, Winlator's wrapper presents through Mesa's X11
swapchain and every DXVK version fails at `vkAcquireNextImageKHR`.

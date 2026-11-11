# Test results and method

How the driver is tested, and what the tests found. Each release runs a set of Vulkan CTS
lists on the Huawei P40 (the CTS line in [CHANGELOG.md](../CHANGELOG.md)); the other phones
run the driver tests and games listed on the [main page](../README.md#tested-gpus).

## Driver at a glance (1.2.9)

| | Previous Kirin 990 Winlator pack | This driver (1.2.9) |
|---|---|---|
| DXVK 2.x requirements (robustness2, nullDescriptor, …) | missing | all 56 hard requirements present |
| Geometry shaders / tessellation / transform feedback | advertised but not implemented (GPU hang) | implemented (emulated), tests pass |
| VKD3D-Proton (D3D12) | no | feature level 11_0, shader model 6.5; draws, readback and presenting checked (1.2.2); AIO D3D12 cube shows |
| Vulkan CTS (dEQP-VK 1.4.7.0) | – | 94 k cases pass in a 1/10 sample of the whole suite; API/info group 18 failures of 20 k (was 610). See [docs/CTS.md](CTS.md) |

## Huawei P40 details

| Item | Value |
|---|---|
| Phone | Huawei P40 (ANA-NX9) |
| SoC / GPU | Kirin 990 / Mali-G76 MC16 (Bifrost, arch v7) |
| Android | 10 (EMUI 12), API 29, kernel 4.14.116 |
| Kernel GPU driver | stock `mali_kbase`, JM, uAPI 11.16 (56-byte atoms) |
| Driver identity | `panvk`, `PanVK-kbase G76-k990 1.2.9 (Mesa 26.3.0-devel)`, Vulkan 1.3.363, 165 device extensions |

Also checked on a Galaxy A51 (Exynos 9611, Mali-G72 MP3, AOSP 16, kbase uAPI 11.35):
same build runs; all tests pass (see *Results*).

## On-device driver tests (adb, no root)

Each test dlopens the driver directly (`vk_icdGetInstanceProcAddr`), renders or
computes, reads back and compares. P40 and A51, build 1.0.0-winlator (1.1.0-winlator re-run on the P40: same results):

| Group | Tests | P40 (G76) | A51 (G72) |
|---|---|---|---|
| PanProbe Vulkan suite (zenithblue): geometry, tessellation, xfb, pipeline stats, multi-viewport, fill modes, clip/cull, depth bounds, robustness2, robust image access 2, descriptors, samplers, blending, occlusion queries, BC decode, large draws, VKD3D heap/timeline, DXVK/VKD3D requirement probes, … | 38 | 37 pass + `tess_perf` (benchmark, runs past the 90 s limit) | same |
| AHB import/render/readback (Huawei gralloc handles, 964×530 BGRA with padded stride) | 7 | pass | pass |
| Native-buffer (Android WSI) swapchain image, 1080×2340 | 1 | pass | – |
| Compositor-style compute: `imageStore` into a swapchain image while sampling an AHB | 1 | pass | – |
| 960-invocation compute workgroup with shared atomics + 3 barriers (`lwg`) | 1 | 61 440 / 61 440 correct | – |
| sync_fd semaphore/fence import/export (`syncfd`) | 1 | matches the previous working pack (signaled -1 fences, no hangs) | – |
| Tiler stress, 1600×900, up to 300 k large triangles | 1 | pass (old pack/overlap mode: tiler OOM) | – |

Two expected non-passes when run on purpose: swapchain buffers allocated *without*
CPU-access usage are AFBC on Huawei gralloc and are refused by design (the driver
requests linear buffers itself).

## Feature coverage (from the requirement probes)

- **DXVK**: 56 / 56 hard requirements; soft 26 / 38. Missing: `shaderFloat64`,
  `shaderFloat16`, `drawIndirectCount`, `shaderOutputLayer`, `samplerFilterMinmax`,
  `VK_EXT_memory_priority`, `VK_EXT_multi_draw`, `VK_EXT_sample_locations`.
- **VKD3D-Proton**: device creation OK (0 hard gaps), FL 11_0, SM 6.5, resource
  binding tier 3, no tiled resources / conservative rasterization / ROVs / mesh /
  ray tracing.

## How it was tested

1. **adb without root.** `/dev/mali0` is world-accessible on the P40, so the driver
   and test binaries run from `/data/local/tmp` over adb; no Winlator or taps needed.
   `scripts/build-tests.sh`, `scripts/deploy-tests.sh`, `scripts/run-tests.sh`.
2. **Old vs new driver** on the same tests (feature probes, AHB/ANB properties,
   sync behaviour) to find what Winlator's wrapper depends on.
3. **Winlator automation.** Game shortcuts are started over adb
   (`am start -n com.winlator.cmod/.XServerDisplayActivity --es shortcut_path <.desktop> --ei container_id <n>`,
   intents from `dumpsys shortcut`); screenshots with `screencap`; driver logs in logcat.
4. **In-driver diagnostics** switched on from `panvk_env.txt`: which GPU job failed and
   what its batch contained (identified the 960-thread compute shader), SPIR-V dumps
   (`MESA_SPIRV_DUMP_PATH`) to replay the failing shader standalone, a per-batch
   GPU-time profiler.
5. **Host debugging for compiler changes.** The same Mesa tree built for x86 with
   `drm-shim` (`PAN_GPU_ID=7211` = G76), ASan and `NIR_DEBUG=validate`; new compiler
   code is proven there first. On the phone new code runs under `ulimit -v`, because
   an early bug in the workgroup pass exhausted memory and kernel-panicked the P40.
6. **Khronos Vulkan CTS (dEQP-VK 1.4.7.0)** built as a plain Android executable and run
   over adb against the driver `.so` through a small loader shim (no root, no APK):
   the whole API/info group, a 1/10 sample of every other group (286 k cases), every
   failure re-run on its own, and the fixes checked against a 19 k-case sample of
   previously passing tests for regressions. Method, numbers and remaining failures:
   [docs/CTS.md](CTS.md). `scripts/build-cts.sh`, `scripts/run-cts.sh`.
7. **Rooted second phone (A51).** `simpleperf` on the Winlator game process (found the
   D3D9 bottleneck in emulated code, not the driver) and the kernel log.

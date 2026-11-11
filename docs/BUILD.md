# Building the driver

```bash
export ANDROID_NDK_ROOT=/path/to/android-ndk-r30
scripts/prepare-tree.sh work          # Mesa a379b735 + patches/base + patches/00*
scripts/build-android.sh work 1.2.4   # → work/dist/panvk-g76-kirin990-1.2.4.zip
scripts/build-tests.sh work           # → out/tests, out/probes
scripts/deploy-tests.sh work/dist/libvulkan_panfrost.so
adb shell sh /data/local/tmp/run-tests.sh /data/local/tmp/drv
scripts/build-cts.sh work             # optional: dEQP-VK for Android, → work/cts-dist
adb push work/cts-dist/. /data/local/tmp/cts/
scripts/run-cts.sh work/dist/libvulkan_panfrost.so caselist.txt results/
```

Performance tools (built by hand with the NDK clang, run over adb): `tests/stress/idraw.c`
(indexed-draw cost; part of the stress tests), `tests/ssbovec/` (robust 16-byte load
check), `tests/tools/hwcnt.c` (Mali-G76 hardware counters, no root),
`tests/tools/jobdump.c` (`LD_PRELOAD` job-chain dump), `tests/tools/halshim.c` (vendor
driver as an ICD, for comparisons), `tests/tools/anbprobe.c` (Android present path:
`vkQueueSignalReleaseImageANDROID`), `tests/tools/spvcomp/` (compile a shader captured
with `MESA_SPIRV_DUMP_PATH` on a host drm-shim build, or benchmark it on the phone
against the vendor driver; compute mode with `SPVCOMP_FILL` / `SPVCOMP_GROUPS`),
`tests/tools/lwgtest/` (1024-invocation workgroup with subgroup operations, shared memory
and barriers), `tests/tools/gsreplay/` (geometry-shader emulation replay),
`tests/tools/fmtprobe.c` (format feature dump), `tests/stress/vpull.c` (DXVK 2.x-style
draws: vertex pulling from a texel buffer, indexed or non-indexed indirect, large shared
index buffers).

Host requirements: meson, ninja, python3 (mako, pyyaml), LLVM + clang + libclc +
SPIRV-LLVM-Translator (for Mesa's host code generators), glslang, xxd.

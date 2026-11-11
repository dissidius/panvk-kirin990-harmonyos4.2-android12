# HarmonyOS 4.2 Mali-G76 72-byte atom layout experiment

**Status:** Experimental driver; source/build success is not device validation.

## Evidence from the user-provided capture (2026-11-11)

* Device: Huawei ELS-AN00, HarmonyOS 4.2 Android 12, Mali-G76 MC16,
  kernel's Mali job-manager uAPI 11.38.
* Existing PanVK 1.3.0-h42-diag1: its **72-byte** dependency-only probe
  completed with event `0x1`, matching atom and `udata`.
* However, it chose a **64-byte** v2 ("MediaTek frame_nr") layout for real
  job submissions; **36,864+** real JOB_SUBMIT(dep) calls then returned
  `EINVAL` despite `dep0=dep1=0`. Core requests included `0x1`,
  `0x16`, `0x96`. Sampled present buffers remained all black.
* After rotating, the emulator's GS thread crashed with SIGSEGV at
  `0xffffffffffffffff` (stack includes emulator and PanVK frames). The
  teardown crash is not attributed to a specific function yet.
* MaliSX2's known-working JM implementation represents full 72-byte
  atoms as a **64-byte base atom with a leading `seq_nr`**, plus an
  8-byte `frame_nr` tail and uses that layout when a 72-byte probe works.

## Experimental change

For only Mali-G76 with GPU ID `0x72110000`, **when the existing probe
reports 64-byte MediaTek-v2 layout**, request 72-byte full atoms for
normal jobs. PanVK already has a `seq_nr` wrapper that yields 72 bytes;
we reuse it rather than changing atom fields.

To disable the experimental override for comparison, put
`PANVK_H42_ATOM72=0` in `panvk_env.txt`.

Expected log:
```
kbase: H42 Mali-G76 accepted 72-byte probe; using full 72-byte job atoms
```

Continue collecting `PANVK_G76_DIAG=1` and `PANVK_GPU_PROF=1` logcat
for validation. Check whether `H42 JOB_SUBMIT(dep) errno=22` disappears,
whether buffers turn nonblack, and whether screen rotation still crashes.

No change to `main` and no change to the baseline diagnostic branch.

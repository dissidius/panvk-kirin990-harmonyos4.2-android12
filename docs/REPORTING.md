# Reporting a problem

If a game crashes, freezes, shows a black screen, flickers or runs much slower than expected
with this driver, a report with the right logs lets us find the cause without guessing. This
page shows how to make one in a couple of minutes.

## Winlator: panvk-report.exe

`panvk-report.exe` is a small Windows program you run inside Winlator. It collects the
information below into one zip and saves it in your phone's **Download** folder.

### 1. Get the tool

Download `panvk-report.exe` from the [latest release](https://github.com/soulexit/panvk-kirin990/releases)
(it is attached next to the driver zips) and save it anywhere Winlator can see, for example the
phone's **Download** folder (drive `D:` in Winlator).

### 2. Reproduce the problem first

Play the game until the problem happens, then close it. The driver records GPU errors (failed
GPU jobs and what ran just before) in `Download/panvk_errors.txt` from version 1.2.9 on, and the
tool picks that file up.

Turn on Winlator's own logging if you can (Winlator settings → Debug / Enable logs): its logs go
to `/sdcard/Winlator/logs/` and the tool includes the three newest.

### 3. Run the tool in the same container

Open the **same container** (or the game's shortcut), so the same driver, DXVK version and
settings are in use. Start `panvk-report.exe` from Winlator's file manager or desktop (for
example `D:\panvk-report.exe`). It takes a few seconds and then shows:

```text
Saved panvk-report-20261105-201530.zip
(in Android's Download folder)
```

Tip: if you copy the tool into the game's folder and run it from there, it also collects the
game's own `*.log` files (DXVK writes `game_d3d11.log` next to the game when logging is on).

### 4. Open an issue and attach the zip

Go to **[New issue → Bug report](https://github.com/soulexit/panvk-kirin990/issues/new/choose)**,
fill in phone, driver version, game and what happens, and drag the zip from your Download folder
into the **Report zip** box.

## What the zip contains

| File | What it is |
|---|---|
| `report.txt` | Hints (settings that matter, see below); Wine and Windows version, host kernel, RAM and CPU count; the container's graphics settings (environment variables starting with `DXVK_`, `WINE`, `BOX64_`, `FEX`, `MESA_`, `PANVK_`, `WRAPPER_`, `VK_` and similar); the Vulkan device Wine sees (driver name and version, limits, features, memory heaps); what D3D11 (DXVK) creates; two GPU self-tests; which files were added |
| `vulkan-extensions.txt` | Every Vulkan extension the driver offers |
| `panvk_env.txt` | Your driver options file, if you have one |
| `panvk_errors.txt` | GPU errors recorded by the driver (1.2.9 and newer) |
| `winlator-logs/` | The three newest Winlator logs |
| `dxvk-logs/` | DXVK's log from the tool's own D3D11 test |
| `local-logs/` | Up to five `*.log` files from the folder the tool ran in |

The zip stays on your phone until you attach it. It contains no account names, passwords or
files outside the list above, but Winlator logs include file paths and the names of the programs
you ran; look through it before posting if that matters to you. Environment variables with
`TOKEN`, `PASSWORD` or `SECRET` in them are never included.

### Hints

`report.txt` starts with a **Hints** section that points out, in plain words, settings known to
cause trouble or slowness with this driver: a container that isn't using PanVK (or an older
PanVK than you meant to test), Winlator's BCn emulation switched on, Box64's compatibility mode,
Sync frame on or DRI3 off, a failed GPU self-test, and GPU errors recorded by the driver. It is
worth reading before opening an issue: some problems are fixed by a setting.

### The GPU self-tests

`report.txt` ends with two lines such as:

```text
GPU test compute: PASS (16384 results, 3 ms)
GPU test large-workgroup: PASS (16384 results, 4 ms)
```

`FAIL` with wrong results, or `VK_TIMEOUT` / `VK_ERROR_DEVICE_LOST`, means the driver or GPU is
not working correctly in that container on its own, before any game is involved; mention it in
the issue.

## Eden, ARMSX2 and other apps

There is no Windows program to run there. Reproduce the problem, then attach:

- `panvk_errors.txt` from `Download/`, or if the app can't write there, from
  `/sdcard/Android/data/<app package>/files/` (for example
  `/sdcard/Android/data/com.armsx2.nightly/files/panvk_errors.txt`).
- Your `panvk_env.txt` if you use one.
- Screenshots of the problem and the app's own log if it has one.

## For developers: build the tool

```bash
sh tools/panvk-report/build.sh
```

Needs mingw-w64 (`x86_64-w64-mingw32-gcc`), `glslangValidator`, `xxd` and the Vulkan headers.
The output is `out/panvk-report.exe`, an x86-64 program that runs in both x86-64 and ARM64EC
containers.

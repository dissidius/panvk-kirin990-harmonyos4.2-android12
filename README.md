# panvk-kirin990

Mesa **PanVK** Vulkan driver for **Mali Bifrost GPUs** (Mali-G76, G72, G52) on the phone's
stock `mali_kbase` kernel driver: no root, no custom kernel. **One driver for every app** that
loads AdrenoTools drivers (Winlator, GameNative, Eden, ARMSX2 and others); each release is a
single zip. It started on the Huawei P40 (Kirin 990), hence the name.

> **Testing build.** An experimental, non-conformant driver. Keep a working driver installed as
> a fallback.

## Tested GPUs

| Phone | GPU | Android, kernel | kbase uAPI | Tested with 1.3.0 | Status |
|---|---|---|---|---|---|
| Huawei P40 (Kirin 990) | Mali-G76 MC16 | 10, 4.14 | JM 11.16 | Vulkan CTS lists every release; Winlator, GameNative, Eden and ARMSX2 games; Winlator renderer | main test phone |
| Samsung Galaxy A51 (Exynos 9611) | Mali-G72 MP3 | 16 (custom ROM), 4.14 | JM 11.35 | driver tests; Winlator NFS Most Wanted (playable); Winlator renderer | works |
| Vivo V21s (Helio G80) | Mali-G52 MC2 | 13, 4.14 | JM 11.27 | driver tests; Winlator NFS Most Wanted with `WRAPPER_RESOURCE_TYPE=opaque`; Winlator renderer | works, see [common problems](#common-problems) |
| Other Bifrost phones (G71, G31, G51, G52 on other SoCs) | | | | untested | should start; please [report](#something-not-working) |

Per-game results and FPS: [docs/GAMES.md](docs/GAMES.md). How the driver is tested and what the
tests found: [docs/TESTS.md](docs/TESTS.md) and [docs/CTS.md](docs/CTS.md).

## Download

Latest: **1.3.0** (pre-release) from
[releases](https://github.com/soulexit/panvk-kirin990/releases):
`panvk-g76-kirin990-1.3.0.zip` for Winlator, Eden and other apps (includes `libGL_memfd.so` for
Winlator's OpenGL on Android 10), and `panvk-g76-kirin990-1.3.0-armsx2.zip` for ARMSX2 (same
driver, library named as ARMSX2 requires).

```text
zip:     panvk-g76-kirin990-1.3.0.zip
sha256:  cabffbe5d33c40dca073f288ba7310fe478d9a5bd6a91f30051b2241c12e2c60
armsx2:  panvk-g76-kirin990-1.3.0-armsx2.zip
sha256:  1b0113fe3484e631efbf327d8c834a3cff9685ddae96af0bfe08518b022c6ff3
.so:     8faa376d5790f091ee2a855ba2e76ae41f24a52571e4259dac3304f5b42f04c4
libGL_memfd.so: 32100a7900cffddcc708ea4ed1b2557404b5fa26a5c9e06d0f9cd74bbf1ef4d2
panvk-report.exe: e0ce7c6701ba87be143b5cc9052d78a57e7014ba8c0f4f292786b500c1fb190a
source:  Mesa a379b735 (27.0.0-devel) + patches/base (zenithblue series) + patches/0001-0069 (tree 077c47a5c8)
```

Older releases and hashes: [docs/RELEASES.md](docs/RELEASES.md). Changes: [CHANGELOG.md](CHANGELOG.md).

## Install

Install the zip as it is (don't unzip it) in the app's driver manager:

| App | Where | Driver options file | Guide |
|---|---|---|---|
| **Winlator** (cmod 4.1) | Contents / driver manager; then Graphics Driver in the container (OpenGL on Android 10: set `BOX64_LIBGL`, see the guide) | `/sdcard/Download/panvk_env.txt` | [docs/apps/winlator.md](docs/apps/winlator.md) |
| **Eden** (Switch) | Settings → GPU driver manager | `/sdcard/Android/data/dev.legacy.eden_emulator.nightly/files/panvk_env.txt` | [docs/apps/eden.md](docs/apps/eden.md) |
| **ARMSX2** (PS2) | GPU driver setting; use the `-armsx2` zip | `/sdcard/Android/data/com.armsx2.nightly/files/panvk_env.txt` | [docs/apps/armsx2.md](docs/apps/armsx2.md) |
| **GameNative** | its driver manager; then the game's Graphics Driver | `/sdcard/Download/panvk_env.txt` or `/sdcard/Android/data/app.gamenative/files/` | – |
| Other AdrenoTools apps | their driver manager | `/sdcard/Download/` if the app can read it, else `/sdcard/Android/data/<package>/files/` | untested |

Winlator's *Renderer Driver* can be PanVK (from 1.3.0 also on Samsung Android 16 and MediaTek
phones) or System.

## Common problems

| What you see | What to do |
|---|---|
| Not sure the game uses PanVK (GameNative's screen shows the same extension count) | Run `panvk-report.exe` in that container: its first line names the driver in use. Or add `DXVK_HUD=devinfo` to the container. GameNative's own screen always shows the stock driver. |
| Black screen or freeze with Winlator's **Renderer Driver** = PanVK | Update to 1.3.0 (fixes Samsung Android 16 and MediaTek phones), or set the Renderer Driver to **System**: games still run on PanVK. |
| 32-bit games crash at start on a **MediaTek** phone; Wine's log shows `vkMapMemory2EXT failed: -5` | Add `WRAPPER_RESOURCE_TYPE=opaque` to the container's environment (1.3.0 or newer). The cause is in Winlator's Vulkan wrapper. |
| Wrong or missing textures, slow loading | Set Winlator's **BCn emulation** to **None**: PanVK decodes BC textures itself. |
| Low FPS in older (D3D9) games | Use Box64's **Performance** preset (Compatibility adds a barrier to every memory access: 5.5 → 74 FPS in the AIO D3D9 test); in ARM64EC containers turn FEX's TSO options off. |
| Game closes in heavy scenes, or Android closes other apps | Add `heap_memory_percent=0.42` to [`panvk_env.txt`](docs/OPTIONS.md) so DXVK keeps less memory. Much lower values can stop games from loading. |
| No OpenGL in Winlator on Android 10 | Set `BOX64_LIBGL` to the driver's `libGL_memfd.so`: [Winlator guide](docs/apps/winlator.md#opengl-zink-on-android-10). |
| A game glitches or shows black with a newer DXVK | Try DXVK 1.10.3 or Sarek, and the reverse (Ori needs 3.1.1 in Winlator); per-game notes in [docs/GAMES.md](docs/GAMES.md). |
| A D3D12 game needs feature level 12_0 or 12_1 | Add `VKD3D_FEATURE_LEVEL=12_0` (or `12_1`) to the container, as long as the game doesn't use tiled resources. |
| `panvk-report`'s GPU self-tests **fail** (kernel 5.10 phones) | Open issue: likely the cached memory these kernels hand out. Please attach the report zip to an issue. |
| ARMSX2 doesn't list the driver | Install the `-armsx2` zip: ARMSX2 needs the library under a different name. |

## What works

- **Direct3D 9/10/11 through DXVK** (1.10 to 3.x), all 56 of DXVK 2.x's hard requirements.
- **Direct3D 12 through VKD3D-Proton** at feature level 11_0 (no sparse resources).
- **OpenGL through Zink** in Winlator, including on Android 10 (`BOX64_LIBGL`, see the guide).
- **Geometry shaders, tessellation and transform feedback**, emulated on Bifrost.
- **Vulkan CTS**: before every release, CTS lists (regression sample, sync2, workgroup memory,
  large workgroups, …) run on the P40 with no new failures; the numbers are in the changelog.

The fixes behind this, patch by patch: [docs/PATCHES.md](docs/PATCHES.md).

## Something not working?

Run `panvk-report.exe` (attached to every release) in the Winlator or GameNative container where
the problem happens, and attach its zip to a
[bug report](https://github.com/soulexit/panvk-kirin990/issues/new/choose). How-to:
[docs/REPORTING.md](docs/REPORTING.md). Driver options for debugging:
[docs/OPTIONS.md](docs/OPTIONS.md).

## Source and build

Built on [zenithblue-oss/panvk-kbase-android](https://github.com/zenithblue-oss/panvk-kbase-android)
(Bifrost geometry/tessellation/transform-feedback emulation, robustness2, …) on Mesa 27, plus the
fixes in [`patches/`](patches/). Build steps: [docs/BUILD.md](docs/BUILD.md). Investigation
notes: [docs/FINDINGS.md](docs/FINDINGS.md).

This repository was `panvk-kirin990-winlator` until 1.2.3 (old links redirect here). Archived
predecessors: [`panvk-kirin990-termux`](https://github.com/soulexit/panvk-kirin990-termux) and
[`panvk-kirin990-armsx2`](https://github.com/soulexit/panvk-kirin990-armsx2); their apps get
releases from here.

## License

MIT — see [LICENSE](LICENSE). Mesa code in the patches stays under Mesa's licenses; test files
derived from zenithblue-oss/panvk-kbase-android (MIT) are marked in their headers. See
[NOTICE.md](NOTICE.md).

#!/usr/bin/env bash
# Compile the previously tested 72-byte Kirin 990 atom fix plus the
# experimental device-wide rotation workaround inside the reused H42 image.
# scripts/prepare-tree.sh MUST have run beforehand against the pinned source.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$ROOT/work"
VER="1.3.0-h42-atom72-rotfix2-ciopt1"
export ANDROID_NDK_ROOT="${H42_NDK_ROOT:-/usr/local/lib/android/sdk/ndk/28.2.13676358}"
export LIBCLANG_PATH=/usr/lib/llvm-18/lib
export CLANG_PATH=/usr/bin/clang-18
export CC=clang-18 CXX=clang++-18
export PANVK_JOBS=4
export CCACHE_DIR="$ROOT/.ci-ccache"
export CCACHE_BASEDIR="$ROOT"
export CCACHE_COMPILERCHECK=content
export CCACHE_NOHASHDIR=true
export CARGO_HOME="$ROOT/.h42-cargo"
export RUSTUP_HOME=/usr/local/rustup
mkdir -p "$CCACHE_DIR" "$CARGO_HOME"
test -x "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang"
test -f "$WORK/mesa/src/panfrost/lib/kmod/kbase_kmod.c"
test -f "$WORK/mesa/src/panfrost/vulkan/panvk_physical_device.c"
echo "H42: using pinned Mesa + cached build tools, NDK r28c, and guarded rotation patch."
rustc --version
bindgen --version
meson --version
ccache --set-config=max_size=700M || :
python3 harmony42/apply_harmony42.py "$WORK/mesa/src/panfrost/lib/kmod/kbase_kmod.c"
python3 harmony42/apply_atom72.py "$WORK/mesa/src/panfrost/lib/kmod/kbase_kmod.c"
python3 harmony42/apply_rotation_compat.py "$WORK/mesa/src/panfrost/vulkan/panvk_physical_device.c"
grep -Fq 'PANVK_H42_ATOM72' "$WORK/mesa/src/panfrost/lib/kmod/kbase_kmod.c"
grep -Fq 'panvk_h42_rotation_compat(device, &supported_extensions)' "$WORK/mesa/src/panfrost/vulkan/panvk_physical_device.c"
sh scripts/build-android.sh "$WORK" "$VER"
sh tools/winlator-gl/build.sh "$WORK/dist"
test -s "$WORK/dist/libGL_memfd.so"
sh scripts/pack.sh "$WORK/dist/libvulkan_panfrost.so" "$VER" "$WORK/dist"
sh scripts/pack.sh "$WORK/dist/libvulkan_panfrost.so" "$VER" "$WORK/dist" armsx2
STANDARD="$WORK/dist/panvk-g76-kirin990-$VER.zip"
ARMSX2="$WORK/dist/panvk-g76-kirin990-$VER-armsx2.zip"
unzip -Z -1 "$STANDARD" | grep -Fxq 'libvulkan_panfrost.so'
unzip -Z -1 "$STANDARD" | grep -Fxq 'libGL_memfd.so'
unzip -Z -1 "$ARMSX2" | grep -Fxq 'libvulkan_freedreno.so'
if unzip -Z -1 "$ARMSX2" | grep -Fxq 'libGL_memfd.so'; then
  echo "H42: unexpected OpenGL shim in ARMSX2 driver" >&2
  exit 1
fi
(cd "$WORK/dist" && sha256sum *.zip > SHA256SUMS)
ccache --show-stats || :
ls -lh "$STANDARD" "$ARMSX2" "$WORK/dist/SHA256SUMS"
